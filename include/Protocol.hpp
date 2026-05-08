#pragma once
#include <cstdint>

enum class MsgType : char
{
    LOGIN_REQ = 'L',
    LOGIN_RES = 'R',
    ORDER_NEW = 'O',
    ORDER_ACK = 'A',
    ORDER_CANCEL = 'C',
    MARKET_DATA = 'M'
};

#pragma pack(push, 1)
struct MsgHeader
{
    MsgType type;
};

struct LoginRequest
{
    char username[16];
    char password[16];
};

struct LoginResponse
{
    bool success;
    int userId; // The ID assigned by the DB
};

struct OrderMsg
{
    char clOrdId[16];
    double price;
    int qty;
    char side; // 'B' or 'S'
};

struct CancelRequest
{
    uint64_t exchangeId;
    int userId;
};

// response struct to send back the Exchange ID
struct OrderResponse
{
    char clOrdId[16];
    uint64_t exchangeId;
    char status; // 'A' for Accepted, 'R' for Rejected
};

struct Trade
{
    uint64_t makerId;
    uint64_t takerId;
    double price;
    int qty;
    int makerRemainingQty;
};

struct MatchResult
{
    std::vector<Trade> trades;
    int remainingQty;
    bool isFullyFilled;
    bool wasAddedToBook;
};

struct PriceLevel {
    double price;
    int volume;
};

struct MarketDataSnapshot {
    MsgHeader header; // Type: MARKET_DATA
    PriceLevel bids[10];
    PriceLevel asks[10];
    int bidLevels;
    int askLevels;
};

#pragma pack(pop)