#pragma once
#include <vector>
#include <poll.h>
#include <sys/socket.h>
#include "OrderBook.hpp"
#include "Protocol.hpp"

namespace NetworkUtils {

    // Sends the current L2 Market Data to a specific file descriptor (Unicast)
    void sendSnapshotToClient(int client_fd, OrderBook& engine);

    // Broadcasts the current L2 Market Data to all connected clients (Broadcast)
    void broadcastSnapshot(const std::vector<struct pollfd>& fds, int server_fd, OrderBook& engine);

    // Helper to send a generic response (like Order ACKs or Login Responses)
    template <typename T>
    void sendResponse(int client_fd, const T& response) {
        if (send(client_fd, &response, sizeof(T), 0) < 0) {
            perror("[NET] Failed to send response");
        }
    }
}