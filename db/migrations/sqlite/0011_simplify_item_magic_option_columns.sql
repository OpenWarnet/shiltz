-- Collapses the NPC magic-option appraiser's per-instance columns
-- (see handlers/ItemConfirmNpc.h):
--
-- 1. item_opt2 and option_bits are the same field, not two -- a slot's
--    option_bits dword doubles as its own "never appraised" sentinel (-1)
--    until the first roll overwrites it. Restore that sentinel on every
--    row that was previously permissive (item_opt2=-1, option_bits=0)
--    before dropping item_opt2, so existing rows keep reading as "never
--    appraised" under the merged field.
-- 2. option_eligible_mask isn't per-instance state -- gate eligibility is
--    recomputed on every appraisal from the item's own template scale
--    columns (ItemScr.h's *_scale fields). Dropped.
UPDATE inventory_slot SET option_bits = -1 WHERE item_opt2 = -1 AND option_bits = 0;
UPDATE equipment_slot SET option_bits = -1 WHERE item_opt2 = -1 AND option_bits = 0;

ALTER TABLE inventory_slot DROP COLUMN item_opt2;
ALTER TABLE inventory_slot DROP COLUMN option_eligible_mask;

ALTER TABLE equipment_slot DROP COLUMN item_opt2;
ALTER TABLE equipment_slot DROP COLUMN option_eligible_mask;
