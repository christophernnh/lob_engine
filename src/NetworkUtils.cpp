#include "NetworkUtils.hpp"
#include <iostream>

namespace NetworkUtils {

    void sendSnapshotToClient(int client_fd, OrderBook& engine) {
        MarketDataSnapshot snap = engine.getL2Snapshot();
        
        // We send the entire struct in one go (Zero-copy approach)
        ssize_t sent = send(client_fd, &snap, sizeof(snap), 0);
        
        if (sent < 0) {
            std::cerr << "[NET] Failed to unicast snapshot to FD: " << client_fd << std::endl;
        }
    }

    void broadcastSnapshot(const std::vector<struct pollfd>& fds, int server_fd, OrderBook& engine) {
        MarketDataSnapshot snap = engine.getL2Snapshot();

        for (const auto& pfd : fds) {
            // 1. Skip the server's own listening socket
            // 2. Only send if the socket is actually connected/active
            if (pfd.fd != server_fd && pfd.fd > 0) {
                ssize_t sent = send(pfd.fd, &snap, sizeof(snap), 0);
                
                if (sent < 0) {
                    // We don't exit here; one bad client shouldn't stop the broadcast
                    std::cerr << "[NET] Broadcast failed for FD: " << pfd.fd << std::endl;
                }
            }
        }
    }
}