Structure:

- RBT with Doubly Linked Lists inside
- Each RBT node represents a price level
- Doubly linked lists inside each node represents orders on a chronological order (A queue)

Websocket Connection:

- Uses AF_UNIX local socketstream
- Waits for a connection from a client via Poll()
- Receives a bytestream payload from the client

Running the server:

- "clang++ -std=c++17 -Iinclude src/OrderBook.cpp src/server.cpp -o lob && ./lob"

Defining Schema

- Create schema.sql
- Run "sqlite3 lob.db < schema.sql" to initiate
- Check schemas:
  - Run "sqlite3 lob.db"
  - .tables
  - .schema Orders
  - .quit
- Truncate tables: sqlite3 lob.db "DELETE FROM Users; DELETE FROM Holdings; DELETE FROM Orders; DELETE FROM Trades; DELETE FROM sqlite_sequence WHERE name IN ('Users', 'Orders', 'Trades'); VACUUM;"
