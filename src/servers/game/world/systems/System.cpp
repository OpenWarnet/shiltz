#include "System.h"

#include "CombatSystem.h"
#include "DropSystem.h"
#include "EnterSystem.h"
#include "MovementSystem.h"

std::span<const System::Factory> System::Factories()
{
    static constexpr Factory kFactories[] = {
        [](Map& map, const Outbox& outbox, const GameData& data) -> std::unique_ptr<System>
        { return std::make_unique<EnterSystem>(map, outbox, data); },
        [](Map& map, const Outbox& outbox, const GameData& data) -> std::unique_ptr<System>
        { return std::make_unique<MovementSystem>(map, outbox, data); },
        [](Map& map, const Outbox& outbox, const GameData& data) -> std::unique_ptr<System>
        { return std::make_unique<CombatSystem>(map, outbox, data); },
        [](Map& map, const Outbox& outbox, const GameData& data) -> std::unique_ptr<System>
        { return std::make_unique<DropSystem>(map, outbox, data); },
    };
    return kFactories;
}
