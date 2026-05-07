#include "AccountManager.hpp"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <iostream>

/**
 * Converts a raw password string into a SHA-256 hex string.
 * This is a deterministic process: same password = same hash.
 */
std::string AccountManager::computeHash(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    
    // Perform the hash calculation
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), 
           password.length(), 
           hash);
    
    // Convert the binary hash to a hex string for storage/comparison
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

/**
 * Validates credentials and returns the User ID for the session.
 * Returns -1 if authentication fails.
 */
int AccountManager::authenticate(const std::string& username, const std::string& password) {
    // Optimization: Fetch both hash and ID in a single database call
    // You will need to implement getUserAuthData in your DatabaseManager
    auto authData = dbMgr.getUserAuthData(username);
    
    // User not found in database
    if (authData.id == -1) {
        std::cout << "[AUTH] User not found: " << username << std::endl;
        return -1;
    }

    // Hash the input password and compare it to the stored hash
    std::string inputHash = computeHash(password);
    
    if (inputHash == authData.hash) {
        std::cout << "[AUTH] Success: " << username << " logged in with ID " << authData.id << std::endl;
        return authData.id;
    }

    // Password mismatch
    std::cout << "[AUTH] Failure: Incorrect password for " << username << std::endl;
    return -1;
}