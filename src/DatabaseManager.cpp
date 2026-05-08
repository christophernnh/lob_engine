#include "DatabaseManager.hpp"
#include <iostream>

DatabaseManager::DatabaseManager(const std::string &dbPath)
{
    sqlite3_open(dbPath.c_str(), &db);

    // Prepare ALL statements once at startup
    sqlite3_prepare_v2(db, "INSERT INTO Orders (exchange_id, user_id, side, price, qty, status) VALUES (?, ?, ?, ?, ?, ?);", -1, &insertOrderStmt, nullptr);
    sqlite3_prepare_v2(db, "INSERT INTO Trades (maker_order_id, taker_order_id, price, qty) VALUES (?, ?, ?, ?);", -1, &insertTradeStmt, nullptr);
    sqlite3_prepare_v2(db, "UPDATE Orders SET qty = ?, status = ? WHERE exchange_id = ?;", -1, &updateOrderStmt, nullptr);
    sqlite3_prepare_v2(db, "INSERT INTO Holdings (user_id, quantity) VALUES (?, ?) ON CONFLICT(user_id) DO UPDATE SET quantity = quantity + excluded.quantity;", -1, &updateHoldingStmt, nullptr);

    workerThread = std::thread(&DatabaseManager::run, this);
}

void DatabaseManager::run()
{
    while (true)
    {
        DBTask task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this]
                    { return !taskQueue.empty() || stop; });
            if (stop && taskQueue.empty())
                break;

            task = taskQueue.front();
            taskQueue.pop();
        }

        // Execute Orders and Trades
        if (task.type == DBTaskType::INSERT_ORDER)
        {
            sqlite3_bind_text(insertOrderStmt, 1, task.exchangeId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(insertOrderStmt, 2, task.userId);
            sqlite3_bind_text(insertOrderStmt, 3, task.side.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(insertOrderStmt, 4, task.price);
            sqlite3_bind_int(insertOrderStmt, 5, task.qty);

            sqlite3_step(insertOrderStmt);
            sqlite3_reset(insertOrderStmt);
            sqlite3_clear_bindings(insertOrderStmt);
        }
        else if (task.type == DBTaskType::INSERT_TRADE)
        {
            sqlite3_bind_text(insertTradeStmt, 1, task.makerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertTradeStmt, 2, task.takerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(insertTradeStmt, 3, task.price);
            sqlite3_bind_int(insertTradeStmt, 4, task.qty);

            sqlite3_step(insertTradeStmt);
            sqlite3_reset(insertTradeStmt);
            sqlite3_clear_bindings(insertTradeStmt);
        }
        else if (task.type == DBTaskType::UPDATE_ORDER_STATUS)
        {
            sqlite3_bind_int(updateOrderStmt, 1, task.remainingQty);
            sqlite3_bind_text(updateOrderStmt, 2, task.status.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateOrderStmt, 3, task.exchangeId.c_str(), -1, SQLITE_STATIC);

            sqlite3_step(updateOrderStmt);
            sqlite3_reset(updateOrderStmt);
            sqlite3_clear_bindings(updateOrderStmt);
        }
    }
}

void DatabaseManager::enqueueOrder(uint64_t exId, int userId, double price, int qty, std::string side, std::string status)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push({DBTaskType::INSERT_ORDER,
                        std::to_string(exId),
                        userId,
                        price,
                        qty,
                        side,
                        status,
                        "", ""});
    }
    cv.notify_one();
}

void DatabaseManager::enqueueTrade(uint64_t makerId, uint64_t takerId, double price, int qty)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push({DBTaskType::INSERT_TRADE,
                        "", 0, price, qty, "", "", // Empty Order fields
                        std::to_string(takerId),
                        std::to_string(makerId)});
    }
    cv.notify_one();
}

void DatabaseManager::updateOrderStatus(uint64_t exId, int remainingQty, std::string status)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        DBTask task;
        task.type = DBTaskType::UPDATE_ORDER_STATUS;
        task.exchangeId = std::to_string(exId);
        task.remainingQty = remainingQty;
        task.status = status;
        taskQueue.push(task);
    }
    cv.notify_one();
}

DatabaseManager::~DatabaseManager()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }
    cv.notify_one();
    if (workerThread.joinable())
        workerThread.join();

    sqlite3_finalize(insertOrderStmt);
    sqlite3_finalize(updateHoldingStmt);
    sqlite3_close(db);
}

AuthData DatabaseManager::getUserAuthData(const std::string &username)
{
    const char *sql = "SELECT user_id, password_hash FROM Users WHERE username = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    AuthData result = {-1, ""};

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        result.id = sqlite3_column_int(stmt, 0);
        result.hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    }

    sqlite3_finalize(stmt);
    return result;
}