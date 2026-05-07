#pragma once
#include <string>
#include "Side.hpp"

struct Order {
    std::string clOrdId;
    uint64_t exchangeId;
    double price;
    int qty;
    Side side;
};