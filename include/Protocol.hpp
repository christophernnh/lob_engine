#pragma once
#include <cstdint>

enum class MsgType : char
{
    LOGIN_REQ = 'L',
    LOGIN_RES = 'R',
    ORDER_NEW = 'O',
    ORDER_ACK = 'A'
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

// response struct to send back the Exchange ID
struct OrderResponse
{
    char clOrdId[16];
    uint64_t exchangeId;
    char status; // 'A' for Accepted, 'R' for Rejected
};

#pragma pack(pop)