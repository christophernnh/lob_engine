#include <iostream>
#include <sys/socket.h> //Core Socket Functions
#include <sys/un.h>     // Unix Domain Sockets
#include <unistd.h>     // close() and read()
#include <vector>       // To manage a list of connected clients
#include <poll.h>       // The multiplexing engine
#include "OrderBook.hpp"
#include "Protocol.hpp"
#include "DatabaseManager.hpp" // For async DB logging
#include "AccountManager.hpp"  // For async DB logging
#include "NetworkUtils.hpp"    // For snapshot sending helpers

#define SOCKET_PATH "/tmp/engine.sock" // Address of server in local PC
#define BUFFER_SIZE 1024               // Max size of a single message

int main()
{
    OrderBook engine;         // Instantiate engine
    int server_fd, client_fd; // File descriptors
    struct sockaddr_un addr;  // Structure to hold the socket address (path)
    char buffer[BUFFER_SIZE]; // Temporary storage for incoming messages

    // 1. Create the socket
    // AF_UNIX is for local communications
    // SOCK_STREAM = reliable, two way connection (similar to TCP)
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    // Deletes old socket file in case of a previous crash
    unlink(SOCKET_PATH);

    // Set the address to the /tmp/ folder
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Binds the socket and the /tmp/ filepath on local pc
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));

    // Listen for incoming request, max 5 clients waiting in line
    listen(server_fd, 5);

    // vector of 'pollfd's, a watch list for the kernel
    std::vector<struct pollfd> fds;
    fds.push_back({server_fd, POLLIN, 0});

    // Print message to show server has started
    std::cout << "[SERVER] Order Engine started at " << SOCKET_PATH << std::endl;

    DatabaseManager dbMgr("lob.db");
    AccountManager accountMgr(dbMgr);
    std::unordered_map<int, int> fdToUserId;

    // 1. Rehydrate the maps with OPEN orders
    std::cout << "[SERVER] Rehydrating Order Book..." << std::endl;
    auto savedOrders = dbMgr.getActiveOrders();
    for (const auto &ord : savedOrders)
    {
        engine.processOrder(ord);
    }

    // 2. Synchronize the ID counter with the Database
    uint64_t lastUsedId = dbMgr.getMaxExchangeId();
    engine.setNextExchangeId(lastUsedId + 1);

    std::cout << "[SERVER] System Ready. Next Exchange ID: " << lastUsedId + 1 << std::endl;
    std::cout << "[SERVER] Active Orders Loaded: " << savedOrders.size() << std::endl;

    // Main loop
    while (true)
    {
        // poll() pauses the CPU until something happens.
        //
        // -1, means to timeout (wait forever)
        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0)
            break; // Handle error

        for (size_t i = 0; i < fds.size(); ++i)
        {
            // Check if the descriptor has an event
            if (fds[i].revents & POLLIN)
            {

                // Case A: Event is on the server socket
                if (fds[i].fd == server_fd)
                {
                    // A new person is trying to connect
                    client_fd = accept(server_fd, NULL, NULL);
                    // Add to watchlist
                    fds.push_back({client_fd, POLLIN, 0});
                    std::cout << "[SERVER] New TUI client connected." << std::endl;
                }

                // Case B: Event on client socket (existing connection)
                else
                {
                    char header_buf[1];
                    int peek_ret = recv(fds[i].fd, header_buf, 1, MSG_PEEK);
                    if (peek_ret == 0)
                    { // Client disconnected
                        std::cout << "[SERVER] Client disconnected." << std::endl;
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        --i;
                        continue;
                    }

                    MsgType type = static_cast<MsgType>(header_buf[0]);

                    if (type == MsgType::LOGIN_REQ)
                    {
                        char login_buf[sizeof(MsgHeader) + sizeof(LoginRequest)];
                        read(fds[i].fd, login_buf, sizeof(login_buf));

                        LoginRequest *req = reinterpret_cast<LoginRequest *>(login_buf + sizeof(MsgHeader));
                        int uid = accountMgr.authenticate(req->username, req->password);

                        // 1. Prepare Header
                        MsgHeader resHeader = {MsgType::LOGIN_RES};
                        // 2. Prepare Payload
                        LoginResponse res = {(uid != -1), uid};

                        // 3. Send BOTH (Header first, then Payload)
                        send(fds[i].fd, &resHeader, sizeof(MsgHeader), 0);
                        send(fds[i].fd, &res, sizeof(LoginResponse), 0);

                        if (uid != -1)
                        {
                            fdToUserId[fds[i].fd] = uid;
                            // 4. Send the initial snapshot ONLY to this user (Unicast)
                            // This ensures they see the book immediately upon login.
                            NetworkUtils::sendSnapshotToClient(fds[i].fd, engine);
                        }
                    }
                    else if (type == MsgType::ORDER_NEW)
                    {
                        if (fdToUserId.find(fds[i].fd) == fdToUserId.end())
                        {
                            std::cout << "[WARN] Unauthenticated order attempt!" << std::endl;
                            // Drain the invalid message so it doesn't stay in the buffer
                            char garbage[sizeof(MsgHeader) + sizeof(OrderMsg)];
                            read(fds[i].fd, garbage, sizeof(garbage));
                            continue;
                        }

                        // FIX: Read the Header AND the Order together to handle the offset
                        char order_buf[sizeof(MsgHeader) + sizeof(OrderMsg)];
                        int bytes_read = read(fds[i].fd, order_buf, sizeof(order_buf));

                        if (bytes_read >= sizeof(order_buf))
                        {
                            // Cast starting AFTER the header
                            OrderMsg *msg = reinterpret_cast<OrderMsg *>(order_buf + sizeof(MsgHeader));

                            std::string clientID(msg->clOrdId, 16);
                            uint64_t exID = engine.generateExchangeId();

                            Order incoming = {
                                exID,
                                clientID,
                                fdToUserId[fds[i].fd],
                                (msg->side == 'B' ? Side::BUY : Side::SELL),
                                msg->price,
                                msg->qty,
                                "OPEN"};

                            MatchResult result = engine.matchOrder(incoming);

                            NetworkUtils::broadcastSnapshot(fds, server_fd, engine);

                            // Async DB logging
                            std::string status = result.isFullyFilled ? "FILLED" : (result.trades.empty() ? "OPEN" : "PARTIAL");
                            dbMgr.enqueueOrder(incoming.exchangeId, incoming.clOrdId, fdToUserId[fds[i].fd], incoming.price, incoming.qty, incoming.side == Side::BUY ? "BUY" : "SELL", status);

                            for (const auto &t : result.trades)
                            {
                                dbMgr.enqueueTrade(t.makerId, t.takerId, t.price, t.qty);
                                // 2. Update the Maker's state in the DB
                                std::string makerStatus = (t.makerRemainingQty == 0) ? "FILLED" : "PARTIAL";
                                dbMgr.updateOrderStatus(t.makerId, t.makerRemainingQty, makerStatus);
                            }

                            // --- Engine FEEDBACK ---
                            if (result.wasAddedToBook)
                            {
                                std::cout << "[INFO] Order " << incoming.exchangeId << " is now resting on the book." << std::endl;
                            }

                            // ... Send ACK ...
                            MsgHeader ackHeader = {MsgType::ORDER_ACK}; // Ensure this exists in your Enum!
                            OrderResponse res;
                            memcpy(res.clOrdId, msg->clOrdId, 16);
                            res.exchangeId = exID;
                            res.status = 'A'; // A for Accepted

                            // Send Header then Payload
                            send(fds[i].fd, &ackHeader, sizeof(MsgHeader), 0);
                            send(fds[i].fd, &res, sizeof(OrderResponse), 0);
                        }
                    }
                    else if (type == MsgType::ORDER_CANCEL)
                    {
                        CancelRequest req;
                        read(fds[i].fd, &req, sizeof(CancelRequest));

                        // TODO: Safety check: Does this user own this order?
                        // if (fdToUserId[fds[i].fd] != accountMgr.getUserIdByOrderId(req.exchangeId)) {
                        //     std::cout << "[WARN] User " << fdToUserId[fds[i].fd] << " attempted to cancel order " << req.exchangeId << " which they do not own!" << std::endl;
                        //     continue;
                        // }

                        engine.cancelOrder(req.exchangeId);
                        dbMgr.updateOrderStatus(req.exchangeId, 0, "CANCELLED");
                    }
                }
            }
        }
    }
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}