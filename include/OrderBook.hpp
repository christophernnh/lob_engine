#pragma once
#include <map>
#include <list>
#include "Order.hpp"

class OrderBook
{
private:
    std::atomic<uint64_t> nextExchangeId{1};

    std::map<double, std::list<Order>, std::greater<double>> bids;
    std::map<double, std::list<Order>> asks;
    std::unordered_map<std::string, std::list<Order>::iterator> orderMap;

    template <typename T>
    void executeMatch(Order &incomingOrder, T &targetBook);

public:
    void processOrder(const Order &order);
    void cancelOrder(const std::string &orderId);

    double getBestBid();
    double getBestAsk();
    double getOrderPrice(std::string orderId);

    void matchOrder(Order &incomingOrder);
    uint64_t generateExchangeId();
};