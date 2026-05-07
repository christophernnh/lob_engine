#include "DatabaseManager.hpp"
#include <iostream>

DatabaseManager::DatabaseManager(const std::string& dbPath) {
    sqlite3_open(dbPath.c_str(), &db);

    // Pre-compile the SQL. This is done ONCE at startup.
    const char* sql = "INSERT INTO Orders (exchange_id, user_id, side, price, qty) VALUES (?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql, -1, &insertOrderStmt, nullptr);

    const char* holdSql = "INSERT INTO Holdings (user_id, quantity) VALUES (?, ?) "
                          "ON CONFLICT(user_id) DO UPDATE SET quantity = quantity + excluded.quantity;";

    sqlite3_prepare_v2(db, holdSql, -1, &updateHoldingStmt, nullptr);

    workerThread = std::thread(&DatabaseManager::run, this);
}

void DatabaseManager::run() {
    while (true) {
        DBTask task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this] { return !taskQueue.empty() || stop; });
            if (stop && taskQueue.empty()) break;
            
            task = taskQueue.front();
            taskQueue.pop();
        }

        // Execute Prepared Statement
        if (task.type == DBTaskType::INSERT_ORDER) {
            sqlite3_bind_text(insertOrderStmt, 1, task.exchangeId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(insertOrderStmt, 2, task.userId);
            sqlite3_bind_text(insertOrderStmt, 3, task.side.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(insertOrderStmt, 4, task.price);
            sqlite3_bind_int(insertOrderStmt, 5, task.qty);

            sqlite3_step(insertOrderStmt);
            sqlite3_reset(insertOrderStmt); // Clear bindings for next use
            sqlite3_clear_bindings(insertOrderStmt);
        }
    }
}

void DatabaseManager::enqueueOrder(const std::string& exId, int userId, double price, int qty, std::string side) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push({DBTaskType::INSERT_ORDER, exId, userId, price, qty, side, ""});
    }
    cv.notify_one();
}

DatabaseManager::~DatabaseManager() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }
    cv.notify_one();
    if (workerThread.joinable()) workerThread.join();
    
    sqlite3_finalize(insertOrderStmt);
    sqlite3_finalize(updateHoldingStmt);
    sqlite3_close(db);
}

AuthData DatabaseManager::getUserAuthData(const std::string& username) {
    const char* sql = "SELECT user_id, password_hash FROM Users WHERE username = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    AuthData result = {-1, ""};

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result.id = sqlite3_column_int(stmt, 0);
        result.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }

    sqlite3_finalize(stmt);
    return result;
}