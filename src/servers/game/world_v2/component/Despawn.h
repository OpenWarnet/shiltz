#pragma once

namespace world_v2
{

// A countdown to this entity ceasing to exist.
//
// Deliberately not a general-purpose timer, and the distinction is worth
// stating because the obvious next step is to make it one.
//
// What this does when it reaches zero is fixed: the entity goes away. That
// is exactly why it can be a single float living on the entity itself. The
// timers that are coming are not like that. skill.scr already carries
// `duration_seconds`, `cooldown_seconds` and `buff_1_duration_seconds`, and
// each of those is many-per-entity -- a player holds one cooldown per skill
// and one clock per active buff -- and each ends in something different: a
// component removed, an action unlocked, an effect fired. A scalar cannot
// hold eight buffs, so a "generic" timer shaped like this one would have to
// be replaced the moment buffs land. When they do, they get their own keyed
// structure, and the only thing shared with this is a five-line countdown
// loop that is not worth an abstraction.
//
// So: specific in what happens, general in what it happens to. Unclaimed
// loot is the first user, but a corpse that lingers, a summoned pet, or a
// projectile all want precisely this and nothing more.
struct DespawnTimerComponent
{
    // Seconds left. Counted down by DespawnSystem, which announces the
    // entity as expired when this runs out; the removal itself happens at
    // the barrier.
    //
    // Zero is a valid starting value and means "expire on the next tick",
    // not "already expired" -- the countdown is applied before the test.
    float remaining = 0.0f;
};

// Marks an entity DespawnSystem has already announced.
//
// The same guard, for the same reason, as DeadComponent: an expiry is
// emitted during the simulation stage and resolved at the barrier, so there
// is a window in which an expired entity is still present. Without the tag
// a second pass would announce it twice, and since a game rule may hang
// anything off an expiry -- a corpse leaving loot behind, a summon
// refunding mana -- that is a duplication bug rather than a cosmetic one.
struct ExpiredComponent
{
};

} // namespace world_v2
