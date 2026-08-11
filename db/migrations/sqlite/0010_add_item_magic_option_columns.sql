-- Per-instance data for the NPC magic-option appraiser (see
-- handlers/ItemConfirmNpc.h). Nothing in this codebase generates real
-- values for these yet (no per-item-instance generation system exists) --
-- item_opt2 defaults to -1 (exempt from the level-90 gate) and
-- option_eligible_mask defaults to all 10 bits set (every gate eligible
-- to roll), a permissive placeholder so the appraiser is exercisable
-- end-to-end on any item today, pending a real item-instance system.
ALTER TABLE inventory_slot ADD COLUMN item_level INTEGER NOT NULL DEFAULT 0;
ALTER TABLE inventory_slot ADD COLUMN item_opt2 INTEGER NOT NULL DEFAULT -1;
ALTER TABLE inventory_slot ADD COLUMN option_bits INTEGER NOT NULL DEFAULT 0;
ALTER TABLE inventory_slot ADD COLUMN option_eligible_mask INTEGER NOT NULL DEFAULT 1023;

ALTER TABLE equipment_slot ADD COLUMN item_level INTEGER NOT NULL DEFAULT 0;
ALTER TABLE equipment_slot ADD COLUMN item_opt2 INTEGER NOT NULL DEFAULT -1;
ALTER TABLE equipment_slot ADD COLUMN option_bits INTEGER NOT NULL DEFAULT 0;
ALTER TABLE equipment_slot ADD COLUMN option_eligible_mask INTEGER NOT NULL DEFAULT 1023;
