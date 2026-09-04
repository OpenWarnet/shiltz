#pragma once

#include <cstdint>

namespace world_v2
{

using TypeId = std::uint32_t;

// Hands out small, dense, sequential ids for types, on first use, one
// independent numbering per `Family` tag.
//
// This exists so that "find the storage for type T" is a vector index
// rather than a std::unordered_map<std::type_index, ...> lookup. Both
// Registry and EventManager do that lookup inside the per-tick loop -- once
// per component access, once per emitted event -- and the map version pays
// a hash of an RTTI type record plus a pointer chase every time, for what
// is really just "which slot". It also drops the dependency on RTTI being
// enabled at all.
//
// The cost is that ids are assigned in first-use order, so they are stable
// within a single run but NOT across runs or builds: never serialize one,
// never persist one, never send one on the wire. They are an in-memory
// implementation detail of the storage layout.
//
// `Family` keeps the numberings apart -- ComponentFamily's ids are dense
// from 0 across component types only, EventFamily's dense from 0 across
// event types only, so neither vector is pockmarked with holes belonging to
// the other.
template <typename Family>
class TypeIdOf
{
public:
    template <typename T>
    static TypeId Value()
    {
        // Function-local static: initialized once, on first use, and
        // thread-safe to initialize per the C++11 memory model.
        static const TypeId id = Next();
        return id;
    }

private:
    static TypeId Next()
    {
        static TypeId next = 0;
        return next++;
    }
};

// Family tags. Empty types; they exist only to name a numbering.
struct ComponentFamily
{
};

struct EventFamily
{
};

struct CommandFamily
{
};

} // namespace world_v2
