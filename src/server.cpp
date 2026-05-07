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

                        LoginResponse res = {(uid != -1), uid};
                        send(fds[i].fd, &res, sizeof(LoginResponse), 0);

                        if (uid != -1)
                            fdToUserId[fds[i].fd] = uid;
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
                                clientID,
                                exID,
                                msg->price,
                                msg->qty,
                                (msg->side == 'B' ? Side::BUY : Side::SELL)};

                            engine.matchOrder(incoming);

                            // ... Send ACK ...
                            OrderResponse res;
                            memcpy(res.clOrdId, msg->clOrdId, 16);
                            res.exchangeId = exID;
                            res.status = 'A';
                            send(fds[i].fd, &res, sizeof(OrderResponse), 0);
                        }
                    }
                }
            }
        }
    }
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}