#pragma once

#include "Map.h"

namespace world_v2
{

// The uniform face of a stage-2 system, so Simulation can hold a list of
// them instead of seven named members.
//
// Every system keeps its own narrow Update() as its real entry point --
// AISystem::Update takes a `const Tile&` because it must not move anyone,
// GridMovementSystem::Update takes a mutable one because moving people is
// its whole job. Those signatures are how a system declares what it
// touches, and they are checked by the compiler. Collapsing them all into
// Run(Map&) would throw that away and leave the claim to a comment.
//
// So Run is an adapter, not a replacement: two lines that unpack the Map
// into whatever that system actually asked for. The narrow Update stays
// public, which is also why the standalone system tests still drive systems
// directly without going near a Simulation.
//
// One indirect call per system per tick. Against per-entity sweeps that is
// not a measurable cost, and it is what buys the whole module design.
class ISystem
{
public:
    virtual ~ISystem() = default;

    // Used in assertion messages and by After<T>'s diagnostics, so it
    // should read like the class name.
    virtual const char* Name() const = 0;

    // Stage 2. Read and write component values freely; the only structural
    // change permitted is to the entity your own view is currently visiting
    // (View.h says why that one is safe). Anything wider is emitted as an
    // event and settles at the barrier.
    virtual void Run(Map& world, float deltaSeconds) = 0;
};

} // namespace world_v2
