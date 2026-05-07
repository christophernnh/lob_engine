#include <iostream>
#include <sys/socket.h> //Core Socket Functions
#include <sys/un.h>     // Unix Domain Sockets
#include <unistd.h>     // close() and read()
#include <vector>       // To manage a list of connected clients
#include <poll.h>       // The multiplexing engine
#include "OrderBook.hpp"
#include "Protocol.hpp"

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
                    // A client sent an order
                    // int bytes_read = read(fds[i].fd, buffer, sizeof(buffer) - 1);

                    // if (bytes_read <= 0) {
                    //     // If disconnected or error, close socket and remove
                    //     close(fds[i].fd);
                    //     fds.erase(fds.begin() + i);
                    //     std::cout << "[SERVER] Client disconnected." << std::endl;
                    // } else {
                    //     // Parse the data heree
                    //     buffer[bytes_read] = '\0';
                    //     std::string msg(buffer);
                    //     std::cout << "[SERVER] Received: " << msg << std::endl;

                    //     // TODO: Parse msg (e.g., "BUY|AAPL|150.0|10")
                    //     // and call engine.matchOrder(...)
                    // }

                    int bytes_read = read(fds[i].fd, buffer, sizeof(buffer));

                    if (bytes_read >= sizeof(OrderMsg)) {
                        // 1. Zero-copy cast
                        OrderMsg* msg = reinterpret_cast<OrderMsg*>(buffer);

                        // 2. Extract Client ID
                        std::string clientID(msg->clOrdId, 16);
                        
                        // 3. Generate the Exchange ID (The "Truth")
                        uint64_t exID = engine.generateExchangeId();

                        std::cout << "[BOOK] Assigning ID " << exID << " to Client ID " << clientID << std::endl;

                        // 4. Create internal Order object
                        Order incoming = {
                            clientID, 
                            exID, 
                            msg->price, 
                            msg->qty, 
                            (msg->side == 'B' ? Side::BUY : Side::SELL)
                        };

                        // 5. Match and Process
                        engine.matchOrder(incoming);
                        std::cout << "TOP OF BOOK: " << "Best Bid = " << engine.getBestBid() << ", Best Ask = " << engine.getBestAsk() << std::endl;

                        // 6. OPTIONAL: Send Acknowledgement back to Client
                        OrderResponse res;
                        memcpy(res.clOrdId, msg->clOrdId, 16);
                        res.exchangeId = exID;
                        res.status = 'A';
                        
                        send(fds[i].fd, &res, sizeof(OrderResponse), 0);
                        std::cout << "[SERVER] Sent ACK for Client ID " << clientID << " with Exchange ID " << exID << std::endl;
                    }
                }
            }
        }
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}