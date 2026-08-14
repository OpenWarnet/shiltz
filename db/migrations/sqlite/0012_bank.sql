CREATE TABLE bank_accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL REFERENCES accounts(id),
    server_id INTEGER NOT NULL,
    password VARCHAR(16) NOT NULL,
    money INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE bank_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    bank_accounts_id INTEGER NOT NULL REFERENCES bank_accounts(id),
    slot_id INTEGER NOT NULL,
    item_id INTEGER NOT NULL,
    quantity INTEGER,
    refine_level INTEGER,
    option_bits INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
