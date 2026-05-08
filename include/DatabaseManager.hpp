#pragma once
#include <sqlite3.h>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Protocol.hpp" // Access to structs like OrderMsg and OrderResponse

enum class DBTaskType
{
    INSERT_ORDER,
    INSERT_TRADE,
    UPDATE_HOLDING,
    UPDATE_ORDER_STATUS
};

struct DBTask
{
    DBTaskType type;
    // For Orders
    std::string exchangeId;
    int userId;
    double price;
    int qty;
    std::string side;
    std::string status;
    // For Trades
    std::string takerId;
    std::string makerId;
    int remainingQty;
};

struct AuthData
{
    int id;
    std::string hash;
};

class DatabaseManager
{
private:
    sqlite3 *db;
    std::queue<DBTask> taskQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread workerThread;
    bool stop = false;

    // Prepared Statements (The Gold Standard for performance/security)
    sqlite3_stmt *insertOrderStmt;
    sqlite3_stmt *updateHoldingStmt;
    sqlite3_stmt *insertTradeStmt;
    sqlite3_stmt *updateOrderStmt;

    void run(); // Background worker loop

public:
    DatabaseManager(const std::string &dbPath);
    ~DatabaseManager();
    AuthData getUserAuthData(const std::string &username);

    void enqueueOrder(uint64_t exId, int userId, double price, int qty, std::string side, std::string status);
    void enqueueTrade(uint64_t takerId, uint64_t makerId, double price, int qty);
    void updateOrderStatus(uint64_t exId, int remainingQty, std::string status);
};