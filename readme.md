Low-latency C++ Limit Order Book Engine & Terminal UI
Features a matching engine with UDS and a terminal-based dashboard

Core Components:

1. Matching Engine: Uses std::map (binary tree) for price discovery and std::list (doubly linked list) for price-time priority. It runs entirely in memory for maximum throughput.

2. Database Manager: Operates on a background thread with a task queue. It uses Write-Ahead Logging (WAL) and transaction batching (up to 500 tasks per batch) to ensure data durability without slowing down the engine.

3. Network Protocol: A binary-packed protocol using #pragma pack(push, 1). Every message starts with a MsgHeader to prevent desynchronization during transmission.

4. TUI (FTXUI): A reactive terminal interface that uses a dedicated background listener thread to update market depth snapshots without freezing the UI.

Key Technical Features:
Persistence - SQLite with Transaction Batching & WAL mode.
Concurrency - std::mutex and std::condition_variable for the DB task queue.
TUI Update - screen.PostEvent(Event::Custom) triggered by the background socket listener.
Memory - Zero-copy binary struct casting for network packets.

Prerequisites:
brew install ftxui sqlite3 openssl

Building the server:
clang++ -std=c++17 \
 src/server.cpp \
 src/OrderBook.cpp \
 src/DatabaseManager.cpp \
 src/AccountManager.cpp \
 src/NetworkUtils.cpp \
 -Iinclude \
 -I$(brew --prefix ftxui)/include \
    -L$(brew --prefix ftxui)/lib \
 -lsqlite3 -lpthread \
 -o engine_server

Building the TUI client:
clang++ -std=c++17 \
 src/client.cpp \
 -Iinclude \
 -I$(brew --prefix ftxui)/include \
    -L$(brew --prefix ftxui)/lib \
 -lftxui-screen -lftxui-dom -lftxui-component -lpthread \
 -o engine_client

Setting up SQLite

- Run "sqlite3 lob.db < schema.sql" to initiate
- Check schemas:
  - Run "sqlite3 lob.db"
  - .tables
  - .schema Orders
  - .quit

Truncate SQLite table:
sqlite3 lob.db "DELETE FROM Users; DELETE FROM Holdings; DELETE FROM Orders; DELETE FROM Trades; DELETE FROM sqlite_sequence WHERE name IN ('Users', 'Orders', 'Trades'); VACUUM;"
