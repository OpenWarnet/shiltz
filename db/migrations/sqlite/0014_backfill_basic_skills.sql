-- Backfill the universal system skills (Sleep, Trade, Fishing, Refine,
-- Martial Combo, Open Chat Room, Party, Inventory/SkillBank, Emoticon,
-- Seller's Kiosk, Buyer's Kiosk, Duel Request -- see
-- world/data/skill/*.scr) onto every character created before
-- HandleCreateCharacter started granting them. Skill 14 (Inventory) in
-- particular gates the client's bank-transaction permission check.
INSERT OR IGNORE INTO character_skill (character_id, skill_id, level)
SELECT c.id, ids.skill_id, 1
FROM character c, (
    SELECT 1 AS skill_id UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
    UNION ALL SELECT 5 UNION ALL SELECT 12 UNION ALL SELECT 13 UNION ALL SELECT 14
    UNION ALL SELECT 15 UNION ALL SELECT 38 UNION ALL SELECT 108 UNION ALL SELECT 109
) ids;
