#pragma once

#include "ComponentPool.h"
#include "Entity.h"
#include "TypeId.h"
#include "View.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace world_v2
{

// The world's database: it hands out entity handles and owns one
// ComponentPool per component type ever used.
//
// A component type needs no registration and no base class -- the first
// Assign<T> creates T's pool. Anything copyable/movable works; the intended
// shape is a small POD struct.
//
// Registry is single-threaded. Nothing here locks, and pointers into pools
// move as pools grow, so exactly one thread may touch a Registry at a time
// -- the simulation thread that runs the tick. Work arriving from network
// threads has to be handed across a queue and drained at the top of a tick,
// never written into a Registry directly.
class Registry
{
public:
    Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = default;
    Registry& operator=(Registry&&) = default;

    // Allocates a handle, reusing a slot freed by Destroy when one is
    // available. A reused slot comes back with a bumped generation, so the
    // returned handle can never compare equal to a handle previously issued
    // for the same slot -- see Entity.h.
    Entity Create()
    {
        if (!m_freeIndices.empty())
        {
            const std::uint32_t index = m_freeIndices.back();
            m_freeIndices.pop_back();
            ++m_aliveCount;
            return MakeEntity(index, m_generations[index]);
        }

        assert(m_generations.size() <= kMaxEntityIndex && "world_v2: entity index space exhausted");

        const std::uint32_t index = static_cast<std::uint32_t>(m_generations.size());
        m_generations.push_back(0);
        ++m_aliveCount;
        return MakeEntity(index, 0);
    }

    // True only for a handle that Create returned and Destroy has not taken
    // back. A handle whose slot has since been recycled fails on the
    // generation comparison, so it reads as dead rather than as whoever
    // holds the slot now.
    bool Exists(Entity entity) const
    {
        const std::uint32_t index = EntityIndex(entity);
        return index < m_generations.size() && m_generations[index] == EntityGeneration(entity);
    }

    // Evicts `entity` from every pool and returns its slot to the free
    // list with the generation advanced, retiring every outstanding handle
    // to it. No-op for a handle that is already dead.
    //
    // This walks every pool, including ones the entity was never in --
    // IComponentPool::Remove treats absence as an ordinary no-op. That
    // trades a pass over the (small, fixed) pool list for not having to
    // track per-entity component membership.
    //
    // Structural, so it belongs in the synchronization barrier, not inside
    // a system sweep -- except for the entity a view is currently visiting,
    // which View documents as safe.
    void Destroy(Entity entity)
    {
        if (!Exists(entity))
        {
            return;
        }

        for (const std::unique_ptr<IComponentPool>& pool : m_pools)
        {
            if (pool)
            {
                pool->Remove(entity);
            }
        }

        const std::uint32_t index = EntityIndex(entity);
        m_generations[index] = (m_generations[index] + 1) & kEntityGenerationMask;
        m_freeIndices.push_back(index);
        --m_aliveCount;
    }

    std::size_t AliveCount() const
    {
        return m_aliveCount;
    }

    // Constructs a T for `entity` from `args`, brace-initialized, and
    // returns it -- so aggregates need no constructor:
    //
    //     registry.Assign<HealthComponent>(e, 120, 120);
    //     registry.Assign<GridPositionComponent>(e, 14, 9);
    //
    // Replaces any T the entity already had.
    template <typename T, typename... Args>
    T& Assign(Entity entity, Args&&... args)
    {
        assert(Exists(entity) && "world_v2: Assign to a dead entity");
        return GetPool<T>().Emplace(entity, std::forward<Args>(args)...);
    }

    // Precondition: Has<T>(entity). The reference is invalidated by the
    // next Assign<T> anywhere in the registry (the dense vector can
    // reallocate) and by a Remove<T>/Destroy of any entity (swap-and-pop
    // can move a different component into the slot).
    template <typename T>
    T& Get(Entity entity)
    {
        assert(Has<T>(entity) && "world_v2: Get of a component the entity does not have");
        return GetPool<T>().Get(entity);
    }

    // nullptr if absent -- the checked form of Get, same invalidation rules.
    template <typename T>
    T* TryGet(Entity entity)
    {
        ComponentPool<T>* pool = FindPool<T>();
        return pool ? pool->TryGet(entity) : nullptr;
    }

    // Never creates a pool: asking whether anything has a component type
    // that was never assigned is just false.
    template <typename T>
    bool Has(Entity entity) const
    {
        const ComponentPool<T>* pool = FindPool<T>();
        return pool != nullptr && pool->Has(entity);
    }

    // Structural -- same placement rules as Destroy.
    template <typename T>
    void Remove(Entity entity)
    {
        if (ComponentPool<T>* pool = FindPool<T>())
        {
            pool->Remove(entity);
        }
    }

    // How many entities hold T.
    template <typename T>
    std::size_t Count() const
    {
        const ComponentPool<T>* pool = FindPool<T>();
        return pool ? pool->Size() : 0;
    }

    // A filter over the entities holding every one of `Ts` -- see View.h
    // for the iteration order and for which structural changes are legal
    // while iterating one.
    //
    // Materializes any pool that does not exist yet, which is why this is
    // not const: it keeps View free of a "this component was never used"
    // special case, at the cost of one empty pool the first time a system
    // asks about a type nothing has yet.
    template <typename... Ts>
    View<Ts...> view()
    {
        return View<Ts...>(GetPool<Ts>()...);
    }

private:
    template <typename T>
    ComponentPool<T>& GetPool()
    {
        const TypeId id = TypeIdOf<ComponentFamily>::Value<T>();
        if (id >= m_pools.size())
        {
            m_pools.resize(id + 1);
        }

        if (!m_pools[id])
        {
            m_pools[id] = std::make_unique<ComponentPool<T>>();
        }

        return *static_cast<ComponentPool<T>*>(m_pools[id].get());
    }

    template <typename T>
    ComponentPool<T>* FindPool()
    {
        const TypeId id = TypeIdOf<ComponentFamily>::Value<T>();
        if (id >= m_pools.size() || !m_pools[id])
        {
            return nullptr;
        }

        return static_cast<ComponentPool<T>*>(m_pools[id].get());
    }

    template <typename T>
    const ComponentPool<T>* FindPool() const
    {
        const TypeId id = TypeIdOf<ComponentFamily>::Value<T>();
        if (id >= m_pools.size() || !m_pools[id])
        {
            return nullptr;
        }

        return static_cast<const ComponentPool<T>*>(m_pools[id].get());
    }

    // Indexed by TypeIdOf<ComponentFamily>::Value<T>(). A vector rather
    // than a map keyed on std::type_index, because every Has/Get in the
    // tick goes through here -- see TypeId.h. Slots for component types
    // this particular Registry never saw stay null.
    std::vector<std::unique_ptr<IComponentPool>> m_pools;

    // Generation counter per entity slot, indexed by EntityIndex. Grows
    // only; a destroyed slot keeps its entry (with the generation bumped)
    // and goes on m_freeIndices for reuse.
    std::vector<std::uint32_t> m_generations;
    std::vector<std::uint32_t> m_freeIndices;

    std::size_t m_aliveCount = 0;
};

} // namespace world_v2
