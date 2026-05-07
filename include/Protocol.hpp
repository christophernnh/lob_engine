#pragma once

#pragma pack(push, 1) // Force 1-byte alignment (no padding)

struct OrderMsg
{
    char clOrdId[16]; // Fixed size
    double price;     // 8 bytes
    int qty;          // 4 bytes
    char side;        // 1 byte ('B' or 'S')
};

// response struct to send back the Exchange ID
struct OrderResponse {
    char clOrdId[16];
    uint64_t exchangeId;
    char status;      // 'A' for Accepted, 'R' for Rejected
};
#pragma pack(pop)