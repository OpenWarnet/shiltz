-- Supports optimistic-concurrency (compare-and-swap) writes on item slots,
-- closing a duplication/loss race where two pipelined requests for the same
-- character both act on the same stale in-memory snapshot before either
-- one's write lands (see repositories/ItemRepository.h).
--
-- 1. inventory_slot.item_id becomes nullable, matching equipment_slot's
--    existing "row stays, item_id = NULL means empty" convention, instead
--    of deleting the row on clear. A CAS write needs to tell "this slot was
--    already emptied by someone else" apart from "this slot was never
--    touched" -- a deleted row can't do that, a NULL-item row can.
--    SQLite can't drop a NOT NULL constraint in place, so the table is
--    rebuilt.
CREATE TABLE inventory_slot_new (
    character_id  INTEGER NOT NULL REFERENCES character(id),
    slot_index    INTEGER NOT NULL,
    item_id       INTEGER,
    quantity      INTEGER,
    refine_level  INTEGER,
    item_level    INTEGER NOT NULL DEFAULT 0,
    option_bits   INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (character_id, slot_index)
);

INSERT INTO inventory_slot_new (character_id, slot_index, item_id, quantity, refine_level,
                                 item_level, option_bits)
SELECT character_id, slot_index, item_id, quantity, refine_level, item_level, option_bits
FROM inventory_slot;

DROP TABLE inventory_slot;
ALTER TABLE inventory_slot_new RENAME TO inventory_slot;

-- 2. bank_items gets the same nullable-item_id/never-delete treatment, plus
--    a unique index on (bank_accounts_id, slot_id) -- it's keyed by a
--    surrogate `id` today with no natural conflict target, so a guarded
--    upsert (INSERT ... ON CONFLICT(...) DO UPDATE ... WHERE ...) has
--    nothing to target without one.
CREATE TABLE bank_items_new (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    bank_accounts_id  INTEGER NOT NULL REFERENCES bank_accounts(id),
    slot_id           INTEGER NOT NULL,
    item_id           INTEGER,
    quantity          INTEGER,
    refine_level      INTEGER,
    option_bits       INTEGER NOT NULL DEFAULT 0,
    created_at        TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO bank_items_new (id, bank_accounts_id, slot_id, item_id, quantity, refine_level,
                             option_bits, created_at)
SELECT id, bank_accounts_id, slot_id, item_id, quantity, refine_level, option_bits, created_at
FROM bank_items;

DROP TABLE bank_items;
ALTER TABLE bank_items_new RENAME TO bank_items;

CREATE UNIQUE INDEX ux_bank_items_account_slot ON bank_items(bank_accounts_id, slot_id);
