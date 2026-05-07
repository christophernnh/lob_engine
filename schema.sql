-- Users Table
CREATE TABLE IF NOT EXISTS Users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Holdings Table
CREATE TABLE IF NOT EXISTS Holdings (
    user_id INTEGER,
    quantity INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id),
    FOREIGN KEY (user_id) REFERENCES Users(user_id)
);

-- Orders Table
CREATE TABLE IF NOT EXISTS Orders (
    exchange_id TEXT PRIMARY KEY,
    cl_ord_id TEXT,
    user_id INTEGER,
    side TEXT CHECK(side IN ('BUY', 'SELL')),
    price REAL NOT NULL,
    qty INTEGER NOT NULL,
    status TEXT DEFAULT 'OPEN', -- OPEN, FILLED, CANCELLED
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES Users(user_id)
);