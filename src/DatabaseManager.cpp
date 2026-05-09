#include "DatabaseManager.hpp"
#include <iostream>

DatabaseManager::DatabaseManager(const std::string &dbPath)
{
    sqlite3_open(dbPath.c_str(), &db);

    // Prepare ALL statements once at startup
    sqlite3_prepare_v2(db, "INSERT INTO Orders (exchange_id, cl_ord_id, user_id, side, price, qty, status) VALUES (?, ?, ?, ?, ?, ?, ?);", -1, &insertOrderStmt, nullptr);
    sqlite3_prepare_v2(db, "INSERT INTO Trades (maker_order_id, taker_order_id, price, qty) VALUES (?, ?, ?, ?);", -1, &insertTradeStmt, nullptr);
    sqlite3_prepare_v2(db, "UPDATE Orders SET qty = ?, status = ? WHERE exchange_id = ?;", -1, &updateOrderStmt, nullptr);
    sqlite3_prepare_v2(db, "INSERT INTO Holdings (user_id, quantity) VALUES (?, ?) ON CONFLICT(user_id) DO UPDATE SET quantity = quantity + excluded.quantity;", -1, &updateHoldingStmt, nullptr);

    workerThread = std::thread(&DatabaseManager::run, this);

    // WAL mode for better concurrency, and NORMAL sync for performance (since we're using a background thread)
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
}

uint64_t DatabaseManager::getMaxExchangeId()
{
    const char *sql = "SELECT MAX(CAST(exchange_id AS INTEGER)) FROM Orders;";
    sqlite3_stmt *stmt;
    uint64_t maxId = 1000;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            // sqlite3_column_int64 returns 0 if the table is empty or NULL
            uint64_t result = sqlite3_column_int64(stmt, 0);
            if (result > 0)
                maxId = result;
        }
    }
    sqlite3_finalize(stmt);
    return maxId;
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
            // 1: exchange_id
            sqlite3_bind_text(insertOrderStmt, 1, task.exchangeId.c_str(), -1, SQLITE_STATIC);
            // 2: cl_ord_id
            sqlite3_bind_text(insertOrderStmt, 2, task.clOrdId.c_str(), -1, SQLITE_STATIC);
            // 3: user_id
            sqlite3_bind_int(insertOrderStmt, 3, task.userId);
            // 4: side
            sqlite3_bind_text(insertOrderStmt, 4, task.side.c_str(), -1, SQLITE_STATIC);
            // 5: price
            sqlite3_bind_double(insertOrderStmt, 5, task.price);
            // 6: qty
            sqlite3_bind_int(insertOrderStmt, 6, task.qty);
            // 7: status
            sqlite3_bind_text(insertOrderStmt, 7, task.status.c_str(), -1, SQLITE_STATIC);

            sqlite3_step(insertOrderStmt);
            sqlite3_reset(insertOrderStmt);
        }
        else if (task.type == DBTaskType::INSERT_TRADE)
        {
            sqlite3_bind_text(insertTradeStmt, 1, task.makerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertTradeStmt, 2, task.takerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(insertTradeStmt, 3, task.price);
            sqlite3_bind_int(insertTradeStmt, 4, task.qty);

            sqlite3_step(insertTradeStmt);
            sqlite3_reset(insertTradeStmt);
        }
        else if (task.type == DBTaskType::UPDATE_ORDER_STATUS)
        {
            sqlite3_bind_int(updateOrderStmt, 1, task.remainingQty);
            sqlite3_bind_text(updateOrderStmt, 2, task.status.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateOrderStmt, 3, task.exchangeId.c_str(), -1, SQLITE_STATIC);

            sqlite3_step(updateOrderStmt);
            sqlite3_reset(updateOrderStmt);
        }
    }
}

void DatabaseManager::enqueueOrder(uint64_t exId, std::string clOrdId, int userId, double price, int qty, std::string side, std::string status)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push({DBTaskType::INSERT_ORDER,
                        std::to_string(exId),
                        clOrdId,
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
                        "", "", 0, price, qty, "", "", // Empty Order fields
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

std::vector<Order> DatabaseManager::getActiveOrders()
{
    std::vector<Order> orders;
    // We select 7 columns total: 0 to 6
    const char *sql = "SELECT exchange_id, user_id, side, price, qty, cl_ord_id, status "
                      "FROM Orders "
                      "WHERE status IN ('OPEN', 'PARTIAL') AND qty > 0;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "[DB] Failed to prepare rehydration query: " << sqlite3_errmsg(db) << std::endl;
        return orders;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // 1. Capture raw pointers carefully
        const char *raw_ex_id = (const char *)sqlite3_column_text(stmt, 0);
        const char *raw_side = (const char *)sqlite3_column_text(stmt, 2);
        const char *raw_cl_id = (const char *)sqlite3_column_text(stmt, 5);
        const char *raw_status = (const char *)sqlite3_column_text(stmt, 6);

        // 2. CRITICAL: Null Check before processing
        if (!raw_ex_id || !raw_side || !raw_cl_id)
        {
            std::cerr << "[WARN] Skipping incomplete row in Orders table." << std::endl;
            continue;
        }

        try
        {
            Order order;
            // Use the captured pointers directly to avoid extra SQLite overhead
            order.exchangeId = std::stoull(raw_ex_id);
            order.userId = sqlite3_column_int(stmt, 1);

            // Efficient string comparison for Side
            order.side = (std::string(raw_side) == "BUY") ? Side::BUY : Side::SELL;

            order.price = sqlite3_column_double(stmt, 3);
            order.qty = sqlite3_column_int(stmt, 4);

            // Safe copy to fixed-size char array
            // This is the cleanest way if using std::string in the struct
            if (raw_cl_id)
            {
                order.clOrdId = raw_cl_id;
            }

            // Safe assignment for status (std::string)
            order.status = raw_status ? raw_status : "OPEN";

            orders.push_back(order);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ERROR] Data conversion failed for order " << (raw_ex_id ? raw_ex_id : "unknown")
                      << ": " << e.what() << std::endl;
        }
    }

    sqlite3_finalize(stmt);
    return orders;
}