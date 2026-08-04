CREATE TABLE character (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id            INTEGER NOT NULL REFERENCES accounts(id),
    server_id             INTEGER NOT NULL,
    slot                  INTEGER NOT NULL,

    name                  TEXT NOT NULL,
    gender                INTEGER NOT NULL,
    hairstyle_id          INTEGER NOT NULL,
    face_id               INTEGER NOT NULL,

    job_id                INTEGER NOT NULL,
    level                 INTEGER NOT NULL DEFAULT 1,

    stats_str             INTEGER NOT NULL DEFAULT 0,
    stats_int             INTEGER NOT NULL DEFAULT 0,
    stats_dex             INTEGER NOT NULL DEFAULT 0,
    stats_con             INTEGER NOT NULL DEFAULT 0,
    stats_men             INTEGER NOT NULL DEFAULT 0,
    stats_sen             INTEGER NOT NULL DEFAULT 0,

    scheduled_deletion_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE character_position (
    character_id  INTEGER PRIMARY KEY REFERENCES character(id),
    map_id        INTEGER NOT NULL,
    location_x    INTEGER NOT NULL,
    location_y    INTEGER NOT NULL
);

CREATE TABLE equipment_slot (
    character_id  INTEGER NOT NULL REFERENCES character(id),
    slot          INTEGER NOT NULL, -- enum: headgear/top/bottom/shoes/weapon/shield/...
    item_id       INTEGER,
    refine_level  INTEGER,
    PRIMARY KEY (character_id, slot)
);
