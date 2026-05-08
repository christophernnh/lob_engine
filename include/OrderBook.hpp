#pragma once
#include <map>
#include <list>
#include "Order.hpp"
#include "Protocol.hpp"

class OrderBook
{
private:
    std::atomic<uint64_t> nextExchangeId{1};

    std::map<double, std::list<Order>, std::greater<double>> bids;
    std::map<double, std::list<Order>> asks;
    std::unordered_map<uint64_t, std::list<Order>::iterator> orderMap;

    template <typename T>
    void executeMatch(Order &incomingOrder, T &targetBook, std::vector<Trade> &trades);

public:
    void processOrder(const Order &order);
    void cancelOrder(uint64_t orderId);

    MarketDataSnapshot getL2Snapshot();

    MatchResult matchOrder(Order &incomingOrder);
    uint64_t generateExchangeId();
};