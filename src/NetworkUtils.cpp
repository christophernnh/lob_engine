#include "NetworkUtils.hpp"
#include <iostream>

namespace NetworkUtils
{

    void sendSnapshotToClient(int client_fd, OrderBook &engine)
    {
        MarketDataSnapshot snap = engine.getL2Snapshot();
        snap.header.type = MsgType::MARKET_DATA;
        send(client_fd, &snap, sizeof(snap), 0);
    }

    void broadcastSnapshot(const std::vector<struct pollfd> &fds, int server_fd, OrderBook &engine)
    {
        MarketDataSnapshot snap = engine.getL2Snapshot();

        for (const auto &pfd : fds)
        {
            // 1. Skip the server's own listening socket
            // 2. Only send if the socket is actually connected/active
            if (pfd.fd != server_fd && pfd.fd > 0)
            {
                ssize_t sent = send(pfd.fd, &snap, sizeof(snap), 0);

                if (sent < 0)
                {
                    // We don't exit here; one bad client shouldn't stop the broadcast
                    std::cerr << "[NET] Broadcast failed for FD: " << pfd.fd << std::endl;
                }
                else
                {
                    std::cout << "[NET] Broadcasted snapshot to FD: " << pfd.fd << std::endl;
                }
            }
        }
    }
}