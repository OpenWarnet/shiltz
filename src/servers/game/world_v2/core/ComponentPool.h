#pragma once

#include "Entity.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace world_v2
{

// The type-erased face of a ComponentPool<T>. Registry keeps its pools as
// pointers to this so it can do the two things it needs to do without
// knowing T: evict an entity from every pool at once when it's destroyed.
// Anything that needs actual component values goes through the typed
// ComponentPool<T>.
class IComponentPool
{
public:
    virtual ~IComponentPool() = default;

    // Drops `entity`'s component if it has one; a silent no-op if it
    // doesn't. Registry::Destroy calls this on every pool without first
    // checking which ones the entity actually appears in, so "not present"
    // has to be an ordinary outcome, not an error.
    virtual void Remove(Entity entity) = 0;

    virtual std::size_t Size() const = 0;

    // The entity handle for each slot of the dense array, in dense order --
    // that is, Entities()[i] owns component i. This is what View iterates:
    // walking one pool's dense array touches only packed memory, and every
    // element it yields is guaranteed to have at least that component.
    virtual const std::vector<Entity>& Entities() const = 0;
};

// Sparse-set storage for one component type.
//
// Three parallel pieces of state:
//
//   m_dense          the components themselves, packed back to back with no
//                    holes, so a system sweeping them walks straight down
//                    cache lines
//   m_denseToEntity  which entity owns m_dense[i] -- needed to fix up the
//                    sparse array when a removal shuffles the dense one,
//                    and to let View yield entities while iterating
//   m_sparse         indexed by an entity's *index* (EntityIndex, not the
//                    raw handle), holding that entity's position in
//                    m_dense, or kUnassigned
//
// So a lookup is two dereferences and no search, and an iteration is a flat
// walk. The price is that removal reorders m_dense -- see Remove.
//
// Not thread-safe, by design. One Registry belongs to one simulation thread;
// see the tick sequence for where structural changes are legal.
template <typename T>
class ComponentPool final : public IComponentPool
{
public:
    static constexpr std::int32_t kUnassigned = -1;

    // Constructs T in place for `entity` from `args` and returns it.
    //
    // Uses brace initialization, so plain aggregates work without writing
    // constructors for them:
    //
    //     pool.Emplace(e, 12, 34);          // T{12, 34}
    //     pool.Emplace(e);                  // T{}     value-initialized
    //     pool.Emplace(e, existingValue);   // T{copy}
    //
    // Braces also mean a narrowing argument (an int for a float field) is a
    // compile error rather than a silent truncation.
    //
    // If `entity` already holds a T it is overwritten in place and no new
    // dense slot is used. That also quietly covers the case where a
    // recycled entity index inherits a stale sparse entry -- the handle
    // stored alongside is refreshed here.
    template <typename... Args>
    T& Emplace(Entity entity, Args&&... args)
    {
        const std::uint32_t index = EntityIndex(entity);
        if (index >= m_sparse.size())
        {
            m_sparse.resize(index + 1, kUnassigned);
        }

        const std::int32_t existing = m_sparse[index];
        if (existing != kUnassigned)
        {
            m_dense[static_cast<std::size_t>(existing)] = T{std::forward<Args>(args)...};
            m_denseToEntity[static_cast<std::size_t>(existing)] = entity;
            return m_dense[static_cast<std::size_t>(existing)];
        }

        m_sparse[index] = static_cast<std::int32_t>(m_dense.size());
        m_dense.push_back(T{std::forward<Args>(args)...});
        m_denseToEntity.push_back(entity);
        return m_dense.back();
    }

    // True only if this exact handle holds a component. A handle whose slot
    // was destroyed and reissued to someone else compares unequal to the
    // handle stored in m_denseToEntity -- generation bits differ -- so a
    // stale handle reads as absent rather than as the new occupant's
    // component. This is the check that makes Entity's generation counter
    // worth carrying.
    bool Has(Entity entity) const
    {
        const std::uint32_t index = EntityIndex(entity);
        if (index >= m_sparse.size())
        {
            return false;
        }

        const std::int32_t dense = m_sparse[index];
        return dense != kUnassigned && m_denseToEntity[static_cast<std::size_t>(dense)] == entity;
    }

    // Precondition: Has(entity). Call TryGet if that isn't already known.
    T& Get(Entity entity)
    {
        return m_dense[static_cast<std::size_t>(m_sparse[EntityIndex(entity)])];
    }

    // nullptr if `entity` has no T. Note that the returned pointer is only
    // good until the next structural change to *this* pool -- an Emplace
    // can reallocate m_dense, a Remove can move a different component into
    // this slot. Don't hold one across either.
    T* TryGet(Entity entity)
    {
        return Has(entity) ? &Get(entity) : nullptr;
    }

    // Swap-and-pop: the last dense element is moved into the vacated slot
    // and the tail dropped, which keeps m_dense hole-free at the cost of
    // reordering it. That reordering is exactly why systems can't remove
    // arbitrary components mid-iteration -- View documents which removals
    // are safe and which aren't.
    void Remove(Entity entity) override
    {
        if (!Has(entity))
        {
            return;
        }

        const std::uint32_t index = EntityIndex(entity);
        const std::size_t removed = static_cast<std::size_t>(m_sparse[index]);
        const std::size_t last = m_dense.size() - 1;

        if (removed != last)
        {
            m_dense[removed] = std::move(m_dense[last]);

            const Entity moved = m_denseToEntity[last];
            m_denseToEntity[removed] = moved;
            m_sparse[EntityIndex(moved)] = static_cast<std::int32_t>(removed);
        }

        m_dense.pop_back();
        m_denseToEntity.pop_back();
        m_sparse[index] = kUnassigned;
    }

    std::size_t Size() const override
    {
        return m_dense.size();
    }

    const std::vector<Entity>& Entities() const override
    {
        return m_denseToEntity;
    }

    // The packed component array itself, for a system that wants to sweep
    // values without caring which entity owns each one (Entities()[i] is
    // the owner of Raw()[i] if it does).
    std::vector<T>& Raw()
    {
        return m_dense;
    }

private:
    std::vector<T> m_dense;
    std::vector<Entity> m_denseToEntity;
    std::vector<std::int32_t> m_sparse;
};

} // namespace world_v2
