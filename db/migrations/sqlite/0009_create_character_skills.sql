CREATE TABLE character_skill (
    character_id  INTEGER NOT NULL REFERENCES character(id),
    skill_id      INTEGER NOT NULL,
    level         INTEGER NOT NULL,
    PRIMARY KEY (character_id, skill_id)
);
