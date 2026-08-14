CREATE TABLE quest_flags (
    character_id INTEGER NOT NULL REFERENCES character(id),
    quest_id     INTEGER NOT NULL,
    flag         BOOLEAN NOT NULL DEFAULT 0,
    created_at   TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (character_id, quest_id)
);