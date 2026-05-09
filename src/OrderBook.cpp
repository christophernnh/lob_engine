#include <iostream>
#include <algorithm>
#include "OrderBook.hpp"
#include "Protocol.hpp"

void OrderBook::processOrder(const Order &order)
{
    Order newOrder = order;

    if (newOrder.side == Side::BUY)
    {
        // 'bids' is std::map<double, std::list<Order>, std::greater<double>>
        auto &list = bids[newOrder.price];
        list.push_back(newOrder);
        orderMap[newOrder.exchangeId] = std::prev(list.end());
    }
    else
    {
        // 'asks' is std::map<double, std::list<Order>, std::less<double>>
        auto &list = asks[newOrder.price];
        list.push_back(newOrder);
        orderMap[newOrder.exchangeId] = std::prev(list.end());
    }
}

void OrderBook::cancelOrder(uint64_t orderId)
{
    // Point to the order map directly
    auto it = orderMap.find(orderId);
    if (it == orderMap.end())
        return;

    // Get the Order pointer
    auto orderIter = it->second;

    // Get the Order price and side
    double price = orderIter->price;
    Side side = orderIter->side;

    if (side == Side::BUY)
    {
        // Erase from the bids
        bids[price].erase(orderIter);
        // Erase from the bids if there are no more bids at the price point
        if (bids[price].empty())
            bids.erase(price);
    }
    else
    {
        // Erase from the asks
        asks[price].erase(orderIter);
        // Erase from the asks if there are no more asks at the price point
        if (asks[price].empty())
            asks.erase(price);
    }

    std::cout << "Erasing Order ID: " << orderIter->clOrdId << std::endl;

    orderMap.erase(it);
}

MarketDataSnapshot OrderBook::getL2Snapshot()
{
    MarketDataSnapshot snapshot;
    snapshot.header.type = MsgType::MARKET_DATA;

    // Extract top 10 bids
    int bidCount = 0;
    for (auto const &[price, list] : bids)
    {
        if (bidCount >= 10)
            break;
        int levelVolume = 0;
        for (const auto &order : list)
        {
            levelVolume += order.qty;
        }
        snapshot.bids[bidCount++] = {price, levelVolume};
    }

    // Extract top 10 asks
    int askCount = 0;
    for (auto const &[price, list] : asks)
    {
        if (askCount >= 10)
            break;
        int levelVolume = 0;
        for (const auto &order : list)
        {
            levelVolume += order.qty;
        }
        snapshot.asks[askCount++] = {price, levelVolume};
    }

    snapshot.bidLevels = bidCount;
    snapshot.askLevels = askCount;
    return snapshot;
}

// template method that takes any targetBook type
template <typename T>
void OrderBook::executeMatch(Order &incomingOrder, T &targetBook, std::vector<Trade> &trades)
{
    while (incomingOrder.qty > 0 && !targetBook.empty())
    {
        auto bestLevelIt = targetBook.begin();
        double bestPrice = bestLevelIt->first;
        auto &makerList = bestLevelIt->second;

        // The only "Side" check we need
        if (incomingOrder.side == Side::BUY && incomingOrder.price < bestPrice)
            break;
        if (incomingOrder.side == Side::SELL && incomingOrder.price > bestPrice)
            break;

        while (incomingOrder.qty > 0 && !makerList.empty())
        {
            Order &makerOrder = makerList.front();
            int matchedQty = std::min(incomingOrder.qty, makerOrder.qty);

            // 1. PERFORM THE MATH FIRST
            incomingOrder.qty -= matchedQty;
            makerOrder.qty -= matchedQty; // makerOrder.qty is now the ACTUAL remaining amount

            // 2. NOW CAPTURE FOR THE TRADE RECORD
            // This ensures the DB gets the quantity AFTER the match
            trades.push_back({
                makerOrder.exchangeId,
                incomingOrder.exchangeId,
                bestPrice,
                matchedQty,
                makerOrder.qty // This is now correct (e.g., 0 if fully filled)
            });

            std::cout << "[MATCHED] " << matchedQty << " units at " << bestPrice << std::endl;

            if (makerOrder.qty == 0)
            {
                // Be careful here: earlier we discussed using exchangeId for the map
                // to avoid client-side ID collisions. Ensure this matches your Map Key!
                orderMap.erase(makerOrder.exchangeId);
                makerList.pop_front();
            }
        }

        if (makerList.empty())
        {
            targetBook.erase(bestLevelIt);
        }
    }
}

MatchResult OrderBook::matchOrder(Order &incomingOrder)
{
    MatchResult result;

    if (incomingOrder.side == Side::BUY)
    {
        executeMatch(incomingOrder, asks, result.trades); // asks is std::map<double, list>
    }
    else
    {
        executeMatch(incomingOrder, bids, result.trades); // bids is std::map<double, list, std::greater>
    }

    result.remainingQty = incomingOrder.qty;
    result.isFullyFilled = (incomingOrder.qty == 0);

    if (incomingOrder.qty > 0)
    {
        processOrder(incomingOrder);
        result.wasAddedToBook = true;
    }

    return result;
}

uint64_t OrderBook::generateExchangeId()
{
    return nextExchangeId.fetch_add(1, std::memory_order_relaxed);
}

void OrderBook::rehydrate(const std::vector<Order>& activeOrders)
{
    uint64_t maxId = 1000;
    for (const auto& order : activeOrders)
    {
        processOrder(order);
        if (order.exchangeId > maxId)
            maxId = order.exchangeId;
    }
    nextExchangeId.store(maxId + 1, std::memory_order_relaxed);
    std::cout << "Rehydrated " << activeOrders.size() << " orders. Next Exchange ID: " << nextExchangeId.load() << std::endl;
}