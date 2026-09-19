#include "CreatureAi.h"

#include "states/IdleState.h"
#include "states/StateSupport.h"
#include "states/WanderState.h"

#include <utility>

CreatureAi::CreatureAi() : m_state(&IdleState::Enter(m_data, false)) {}

void CreatureAi::Reset()
{
    m_state = &IdleState::Enter(m_data, false);
}

std::optional<CreatureAiSenseRequest> CreatureAi::Advance(std::chrono::milliseconds delta)
{
    if (!creature_ai::AdvanceTimer(m_data.timer, delta))
        return std::nullopt;

    return CreatureAiSenseRequest{m_state->Sense(), m_data.target_id};
}

std::optional<CreatureAiIntent> CreatureAi::Decide(const Placement& self,
                                                   const MonsterRecord& monster,
                                                   const CreatureAiPerception& perception)
{
    CreatureAiDecision decision = m_state->Decide(m_data, self, monster, perception);
    m_state = &decision.next;

    return std::move(decision.intent);
}

void CreatureAi::CompleteWanderStep()
{
    if (m_state->Kind() == CreatureAiStateKind::Wander && WanderState::CompleteStep(m_data))
        m_state = &IdleState::Enter(m_data, false);
}

CreatureAiStateKind CreatureAi::State() const noexcept
{
    return m_state->Kind();
}
