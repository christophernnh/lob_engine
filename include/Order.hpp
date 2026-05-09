#pragma once
#include <string>
#include "Side.hpp"

struct Order
{
    uint64_t exchangeId;
    std::string clOrdId;
    int userId;
    Side side;
    double price;
    int qty;
    std::string status;
};