#include <iostream>
#include <algorithm>
#include "OrderBook.hpp"

void OrderBook::processOrder(const Order &order)
{
    Order newOrder = order;

    if (newOrder.side == Side::BUY)
    {
        auto &list = bids[newOrder.price];
        list.push_back(newOrder);
        orderMap[newOrder.clOrdId] = std::prev(list.end());
    }
    else
    {
        auto &list = asks[newOrder.price];
        list.push_back(newOrder);
        orderMap[newOrder.clOrdId] = std::prev(list.end());
    }
}

void OrderBook::cancelOrder(const std::string &orderId)
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

double OrderBook::getBestBid()
{
    if (bids.empty())
        return -1.0;
    return bids.begin()->first;
}

double OrderBook::getBestAsk()
{
    if (asks.empty())
        return -1.0;
    return asks.begin()->first;
}

double OrderBook::getOrderPrice(std::string orderId)
{
    auto it = orderMap.find(orderId);
    if (it == orderMap.end())
    {
        throw std::runtime_error("Order ID " + orderId + " not found");
    }
    return it->second->price;
}

// template method that takes any targetBook type
template <typename T>
void OrderBook::executeMatch(Order &incomingOrder, T &targetBook)
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

            std::cout << "[MATCHED] " << matchedQty << " units at " << bestPrice << std::endl;

            incomingOrder.qty -= matchedQty;
            makerOrder.qty -= matchedQty;

            if (makerOrder.qty == 0)
            {
                orderMap.erase(makerOrder.clOrdId);
                makerList.pop_front();
            }
        }

        if (makerList.empty())
        {
            targetBook.erase(bestLevelIt);
        }
    }
}

void OrderBook::matchOrder(Order &incomingOrder)
{
    if (incomingOrder.side == Side::BUY)
    {
        executeMatch(incomingOrder, asks); // asks is std::map<double, list>
    }
    else
    {
        executeMatch(incomingOrder, bids); // bids is std::map<double, list, std::greater>
    }

    if (incomingOrder.qty > 0)
    {
        processOrder(incomingOrder);
    }
}

uint64_t OrderBook::generateExchangeId()
{
    return nextExchangeId.fetch_add(1, std::memory_order_relaxed);
}