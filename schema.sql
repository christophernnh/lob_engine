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

-- Trades Table
CREATE TABLE IF NOT EXISTS Trades (
    trade_id INTEGER PRIMARY KEY AUTOINCREMENT,
    maker_order_id TEXT NOT NULL, -- The order that was already on the book
    taker_order_id TEXT NOT NULL, -- The order that just arrived and matched
    price REAL NOT NULL,
    qty INTEGER NOT NULL,
    executed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (maker_order_id) REFERENCES Orders(exchange_id),
    FOREIGN KEY (taker_order_id) REFERENCES Orders(exchange_id)
);