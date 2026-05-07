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
    UPDATE_HOLDING,
    UPDATE_STATUS
};

struct DBTask
{
    DBTaskType type;
    std::string exchangeId;
    int userId;
    double price;
    int qty;
    std::string side;
    std::string status;
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

    void run(); // Background worker loop

public:
    DatabaseManager(const std::string &dbPath);
    ~DatabaseManager();
    AuthData getUserAuthData(const std::string &username);

    void enqueueOrder(const std::string &exId, int userId, double price, int qty, std::string side);
};