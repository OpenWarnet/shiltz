CREATE TABLE inventory_slot (
    character_id  INTEGER NOT NULL REFERENCES character(id),
    slot_index    INTEGER NOT NULL,
    item_id       INTEGER NOT NULL,
    quantity      INTEGER,
    refine_level  INTEGER,
    PRIMARY KEY (character_id, slot_index)
);
