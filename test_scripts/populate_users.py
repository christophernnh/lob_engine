import hashlib
import sqlite3


def hash_password(password):
    # Simple SHA-256 for this demo (Industry uses BCrypt, but this is zero-config)
    return hashlib.sha256(password.encode()).hexdigest()


db = sqlite3.connect("lob.db")
cursor = db.cursor()

# Test data
users = [("trader_1", "password123"), ("trader_2", "hk_finance_2026")]

for username, pwd in users:
    cursor.execute(
        "INSERT OR IGNORE INTO Users (username, password_hash) VALUES (?, ?)",
        (username, hash_password(pwd)),
    )

db.commit()
print("Users table populated successfully.")
db.close()
