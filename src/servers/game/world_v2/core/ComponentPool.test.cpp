#include "ComponentPool.h"
#include "Entity.h"
#include "Test.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace world_v2;

namespace
{

struct Position
{
    int x = 0;
    int y = 0;
};

// A component that owns a heap allocation.
//
// Every other component in world_v2 is a flat POD, so until InventoryComponent
// arrived nothing had ever instantiated ComponentPool<T> for a type with a
// non-trivial copy, move, or destructor -- the brace initialization in
// Emplace and the std::move in Remove's swap-and-pop had simply never been
// compiled against one. The pool's own documentation claims to handle it;
// this is what checks that.
struct Bag
{
    std::vector<int> items;
    std::string label;
};

Bag MakeBag(int owner)
{
    Bag bag;
    bag.items = {owner, owner * 2, owner * 3};
    bag.label = "bag-" + std::to_string(owner);
    return bag;
}

bool BagBelongsTo(const Bag& bag, int owner)
{
    return bag.items.size() == 3 && bag.items[0] == owner && bag.items[1] == owner * 2 &&
           bag.items[2] == owner * 3 && bag.label == "bag-" + std::to_string(owner);
}

// Entity handles are normally minted by Registry; these tests need to build
// them by hand to exercise generation mismatches without standing a whole
// Registry up.
Entity Handle(std::uint32_t index, std::uint32_t generation = 0)
{
    return MakeEntity(index, generation);
}

void AssignAndRead()
{
    ComponentPool<Position> pool;
    CHECK_EQ(pool.Size(), 0u);
    CHECK(!pool.Has(Handle(0)));

    Position& stored = pool.Emplace(Handle(7), 3, 4);
    CHECK_EQ(pool.Size(), 1u);
    CHECK(pool.Has(Handle(7)));
    CHECK_EQ(stored.x, 3);
    CHECK_EQ(pool.Get(Handle(7)).y, 4);

    // Emplace hands back a live reference into the dense array.
    stored.x = 99;
    CHECK_EQ(pool.Get(Handle(7)).x, 99);

    CHECK(pool.TryGet(Handle(7)) != nullptr);
    CHECK(pool.TryGet(Handle(8)) == nullptr);
}

void DefaultAndCopyConstruction()
{
    ComponentPool<Position> pool;

    pool.Emplace(Handle(1));
    CHECK_EQ(pool.Get(Handle(1)).x, 0);
    CHECK_EQ(pool.Get(Handle(1)).y, 0);

    const Position source{11, 22};
    pool.Emplace(Handle(2), source);
    CHECK_EQ(pool.Get(Handle(2)).x, 11);
    CHECK_EQ(pool.Get(Handle(2)).y, 22);
}

void EmplaceOverwritesInPlace()
{
    ComponentPool<Position> pool;

    pool.Emplace(Handle(4), 1, 1);
    pool.Emplace(Handle(4), 5, 6);

    // Re-assigning must reuse the same dense slot, not append a second one.
    CHECK_EQ(pool.Size(), 1u);
    CHECK_EQ(pool.Get(Handle(4)).x, 5);
    CHECK_EQ(pool.Get(Handle(4)).y, 6);
}

void RemoveKeepsDensePacked()
{
    ComponentPool<Position> pool;
    pool.Emplace(Handle(0), 0, 0);
    pool.Emplace(Handle(1), 1, 10);
    pool.Emplace(Handle(2), 2, 20);
    pool.Emplace(Handle(3), 3, 30);

    // Removing from the middle swaps the tail into the hole.
    pool.Remove(Handle(1));

    CHECK_EQ(pool.Size(), 3u);
    CHECK(!pool.Has(Handle(1)));
    CHECK(pool.TryGet(Handle(1)) == nullptr);

    // Every survivor still resolves to its own component -- this is the
    // sparse fix-up in Remove doing its job.
    CHECK_EQ(pool.Get(Handle(0)).y, 0);
    CHECK_EQ(pool.Get(Handle(2)).y, 20);
    CHECK_EQ(pool.Get(Handle(3)).y, 30);

    // Dense storage has no hole in it.
    CHECK_EQ(pool.Raw().size(), 3u);
    CHECK_EQ(pool.Entities().size(), 3u);
    for (std::size_t i = 0; i < pool.Entities().size(); ++i)
    {
        CHECK_EQ(pool.Get(pool.Entities()[i]).y, pool.Raw()[i].y);
    }
}

void RemoveLastAndRemoveAll()
{
    ComponentPool<Position> pool;
    pool.Emplace(Handle(0), 0, 0);
    pool.Emplace(Handle(1), 1, 1);

    // Removing the tail takes the no-swap branch.
    pool.Remove(Handle(1));
    CHECK_EQ(pool.Size(), 1u);
    CHECK(pool.Has(Handle(0)));

    pool.Remove(Handle(0));
    CHECK_EQ(pool.Size(), 0u);
    CHECK(pool.Entities().empty());

    // Removing what was never there, and what is already gone, are both
    // ordinary no-ops -- Registry::Destroy relies on this when it sweeps
    // every pool.
    pool.Remove(Handle(0));
    pool.Remove(Handle(500));
    CHECK_EQ(pool.Size(), 0u);
}

void StaleHandleReadsAsAbsent()
{
    ComponentPool<Position> pool;
    pool.Emplace(Handle(9, 0), 1, 2);

    // Same slot, later generation: a handle to whoever holds slot 9 *now*.
    CHECK(!pool.Has(Handle(9, 1)));
    CHECK(pool.TryGet(Handle(9, 1)) == nullptr);

    // And the reverse -- an old handle must not read the new occupant's
    // component after the slot is reused.
    pool.Remove(Handle(9, 0));
    pool.Emplace(Handle(9, 1), 7, 8);

    CHECK(pool.Has(Handle(9, 1)));
    CHECK(!pool.Has(Handle(9, 0)));
    CHECK_EQ(pool.Get(Handle(9, 1)).x, 7);
}

void SparseGrowsForDistantIndices()
{
    ComponentPool<Position> pool;

    // A high index has to widen the sparse array without disturbing the
    // entries already in it.
    pool.Emplace(Handle(0), 1, 1);
    pool.Emplace(Handle(1000), 2, 2);

    CHECK(pool.Has(Handle(0)));
    CHECK(pool.Has(Handle(1000)));
    CHECK(!pool.Has(Handle(999)));
    CHECK_EQ(pool.Size(), 2u);
}

void OwningComponentsSurviveStorage()
{
    ComponentPool<Bag> pool;

    pool.Emplace(Handle(3), MakeBag(3));
    CHECK(BagBelongsTo(pool.Get(Handle(3)), 3));

    // Overwriting has to release the old contents and take the new ones,
    // not append or alias.
    pool.Emplace(Handle(3), MakeBag(9));
    CHECK_EQ(pool.Size(), 1u);
    CHECK(BagBelongsTo(pool.Get(Handle(3)), 9));

    // A live reference into the dense array is still the real thing.
    pool.Get(Handle(3)).items.push_back(42);
    CHECK_EQ(pool.Get(Handle(3)).items.size(), 4u);
}

void OwningComponentsSurviveSwapAndPop()
{
    ComponentPool<Bag> pool;
    for (int i = 0; i < 8; ++i)
    {
        pool.Emplace(Handle(static_cast<std::uint32_t>(i)), MakeBag(i));
    }

    // Removing from the middle moves the tail element into the hole, which
    // for an owning type is a real move rather than a memcpy. Every
    // survivor still has to hold its own contents afterwards.
    pool.Remove(Handle(2));
    pool.Remove(Handle(5));
    pool.Remove(Handle(0));

    CHECK_EQ(pool.Size(), 5u);
    for (int i = 0; i < 8; ++i)
    {
        const std::uint32_t index = static_cast<std::uint32_t>(i);
        if (i == 0 || i == 2 || i == 5)
        {
            CHECK(!pool.Has(Handle(index)));
            continue;
        }

        CHECK(pool.Has(Handle(index)));
        CHECK(BagBelongsTo(pool.Get(Handle(index)), i));
    }

    // And the dense array is still packed, with each entry owned by the
    // entity the parallel array names.
    CHECK_EQ(pool.Raw().size(), 5u);
    for (std::size_t i = 0; i < pool.Entities().size(); ++i)
    {
        CHECK(pool.Raw()[i].label == pool.Get(pool.Entities()[i]).label);
    }
}

void OwningComponentsSurviveReallocation()
{
    ComponentPool<Bag> pool;

    // Enough to force the dense vector to grow several times, moving every
    // element each time.
    for (int i = 0; i < 500; ++i)
    {
        pool.Emplace(Handle(static_cast<std::uint32_t>(i)), MakeBag(i));
    }

    CHECK_EQ(pool.Size(), 500u);
    for (int i = 0; i < 500; ++i)
    {
        CHECK(BagBelongsTo(pool.Get(Handle(static_cast<std::uint32_t>(i))), i));
    }

    // Emptying it entirely, one at a time from the front, is the heaviest
    // shuffling the swap-and-pop does.
    for (int i = 0; i < 500; ++i)
    {
        pool.Remove(Handle(static_cast<std::uint32_t>(i)));
    }
    CHECK_EQ(pool.Size(), 0u);
}

} // namespace

int main()
{
    AssignAndRead();
    DefaultAndCopyConstruction();
    EmplaceOverwritesInPlace();
    RemoveKeepsDensePacked();
    RemoveLastAndRemoveAll();
    StaleHandleReadsAsAbsent();
    SparseGrowsForDistantIndices();
    OwningComponentsSurviveStorage();
    OwningComponentsSurviveSwapAndPop();
    OwningComponentsSurviveReallocation();

    return world_v2::test::Summary("ComponentPool");
}
