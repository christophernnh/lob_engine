#pragma once
#include "DatabaseManager.hpp"
#include <string>

class AccountManager {
private:
    DatabaseManager& dbMgr; // Reference to your existing DB manager
    std::string computeHash(const std::string& password);

public:
    AccountManager(DatabaseManager& db) : dbMgr(db) {}
    
    // This will return the User ID if successful, or -1 if failed
    int authenticate(const std::string& username, const std::string& password);
};