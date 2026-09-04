#pragma once

#include "ComponentPool.h"
#include "Entity.h"

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

namespace world_v2
{

// A filter over the entities that hold *every* component in `Ts`.
//
// Cheap to construct and meant to be thrown away each time -- build one at
// the top of a system's Update, iterate, drop it. It owns nothing and
// allocates nothing; it is a handful of pool pointers.
//
// How iteration works
// -------------------
// The view picks whichever requested pool currently holds the fewest
// components and walks *that* pool's dense array, probing the other pools
// to reject entities missing anything. So the cost scales with the rarest
// component, not with how many entities the world has: a
// View<GridPositionComponent, MoveIntentComponent> over 5,000 monsters of
// which 12 are moving this tick visits 12 entities, not 5,000.
//
// The walk runs *backwards* over the dense array. That's not an accident --
// see the structural-change rules below.
//
// What you may change while iterating
// -----------------------------------
// ComponentPool::Remove swaps the last dense element into the vacated slot,
// so a removal reorders the array underneath the iteration. Walking
// backwards makes the common cases safe anyway:
//
//   SAFE    removing components from, or destroying, the entity currently
//           being visited -- the element swapped into its slot came from
//           beyond the cursor and has already been seen
//   SAFE    assigning components, including to the driving pool -- new
//           components land past the cursor and simply aren't visited this
//           pass, which is what you want for a request created this tick
//   UNSAFE  removing from, or destroying, some *other* entity that hasn't
//           been visited yet -- the tail element lands in its slot and gets
//           visited a second time
//
// The first case is the one systems actually need: "consume this entity's
// MoveIntentComponent now that I've acted on it". For the third, emit an
// event and let the synchronization barrier apply it -- that is what the
// deferred EventManager is for.
//
// One more caveat, unrelated to ordering: a T& handed to you by this view
// points into a pool's dense vector, so an Emplace of that same component
// type can reallocate it out from under you. Don't hold a reference across
// an Assign of its own type.
template <typename... Ts>
class View
{
    static_assert(sizeof...(Ts) > 0, "View needs at least one component type");

public:
    explicit View(ComponentPool<Ts>&... pools)
        : m_pools(&pools...)
        , m_driver(Smallest(pools...))
    {
    }

    // True if `entity` holds every component this view filters on.
    bool Contains(Entity entity) const
    {
        return std::apply([entity](auto*... pool) { return (pool->Has(entity) && ...); }, m_pools);
    }

    // Precondition: Contains(entity) -- which every entity yielded by this
    // view satisfies. T must be one of Ts.
    template <typename T>
    T& Get(Entity entity) const
    {
        return std::get<ComponentPool<T>*>(m_pools)->Get(entity);
    }

    // How many entities the walk will *consider*. An upper bound on the
    // number actually yielded, not the exact count -- the driving pool's
    // members still have to hold the other components too.
    std::size_t CandidateCount() const
    {
        return m_driver->Size();
    }

    // Calls fn(Entity, Ts&...) for each match. The preferred form: the
    // references are handed in fresh per entity, so there is no way to
    // accidentally cache one across a structural change.
    template <typename Fn>
    void Each(Fn&& fn) const
    {
        const std::vector<Entity>& entities = m_driver->Entities();

        std::int64_t cursor = static_cast<std::int64_t>(entities.size()) - 1;
        while (cursor >= 0)
        {
            // The callback may have destroyed several entities at once
            // (Registry::Destroy evicts from every pool), shrinking the
            // driving array past the cursor. Re-clamp rather than index off
            // the end.
            const std::int64_t size = static_cast<std::int64_t>(entities.size());
            if (cursor >= size)
            {
                cursor = size - 1;
                continue;
            }

            const Entity entity = entities[static_cast<std::size_t>(cursor)];
            if (Contains(entity))
            {
                fn(entity, Get<Ts>(entity)...);
            }

            --cursor;
        }
    }

    // Range-for support: `for (Entity e : view) ... view.Get<T>(e) ...`.
    // Same backwards walk and same rules as Each; reach for Each unless the
    // imperative shape genuinely reads better.
    class Iterator
    {
    public:
        Iterator(const View* view, std::int64_t cursor)
            : m_view(view)
            , m_cursor(cursor)
        {
            SkipUnmatched();
        }

        Entity operator*() const
        {
            return m_view->m_driver->Entities()[static_cast<std::size_t>(m_cursor)];
        }

        Iterator& operator++()
        {
            --m_cursor;
            SkipUnmatched();
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return m_cursor != other.m_cursor;
        }

    private:
        // Advances past entities that don't hold every component, and
        // re-clamps if the driving pool shrank since the last step. Lands
        // on -1 when nothing is left, which is what end() compares against.
        void SkipUnmatched()
        {
            const std::vector<Entity>& entities = m_view->m_driver->Entities();
            while (m_cursor >= 0)
            {
                const std::int64_t size = static_cast<std::int64_t>(entities.size());
                if (m_cursor >= size)
                {
                    m_cursor = size - 1;
                    continue;
                }

                if (m_view->Contains(entities[static_cast<std::size_t>(m_cursor)]))
                {
                    return;
                }

                --m_cursor;
            }
        }

        const View* m_view;
        std::int64_t m_cursor;
    };

    Iterator begin() const
    {
        return Iterator(this, static_cast<std::int64_t>(m_driver->Size()) - 1);
    }

    Iterator end() const
    {
        return Iterator(this, -1);
    }

private:
    // Whichever requested pool has the fewest entries right now -- the one
    // whose dense array is cheapest to walk. Chosen once, at construction:
    // a view is short-lived enough that re-picking mid-iteration would only
    // invite the double-visit hazard described above.
    static const IComponentPool* Smallest(ComponentPool<Ts>&... pools)
    {
        const IComponentPool* candidates[] = {&pools...};

        const IComponentPool* best = candidates[0];
        for (const IComponentPool* candidate : candidates)
        {
            if (candidate->Size() < best->Size())
            {
                best = candidate;
            }
        }

        return best;
    }

    std::tuple<ComponentPool<Ts>*...> m_pools;
    const IComponentPool* m_driver;
};

} // namespace world_v2
