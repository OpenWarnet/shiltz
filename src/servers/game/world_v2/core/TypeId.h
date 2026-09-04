#pragma once

#include <atomic>
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
        // Atomic, and this is not belt-and-braces.
        //
        // The magic static above serializes the initialization of *one*
        // type's id -- a second thread asking for T's id blocks until the
        // first has finished computing it. It says nothing about two
        // *different* types being first touched at the same moment: those
        // are two unrelated statics, both initializers run concurrently,
        // and both land here. A plain `next++` is then a read-modify-write
        // race, and losing it hands two component types the same id, which
        // in Registry means the same pool.
        //
        // Reachable in exactly the case the rest of the framework is built
        // for: several map threads coming up at once, each touching a
        // component or event type nothing has touched yet. Relaxed is
        // enough -- uniqueness is the whole requirement, and nothing
        // orders other memory against this counter.
        static std::atomic<TypeId> next{0};
        return next.fetch_add(1, std::memory_order_relaxed);
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
