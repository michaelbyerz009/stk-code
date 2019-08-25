
#include "network/race_event_manager.hpp"

#include "karts/controller/controller.hpp"
#include "modes/world.hpp"
#include "network/network_config.hpp"
#include "network/protocols/game_events_protocol.hpp"
#include "network/rewind_manager.hpp"
#include "utils/profiler.hpp"

RaceEventManager::RaceEventManager()
{
    m_running = false;
}   // RaceEventManager

// ----------------------------------------------------------------------------
RaceEventManager::~RaceEventManager()
{
}   // ~RaceEventManager

// ----------------------------------------------------------------------------
/** In network games this update function is called instead of
 *  World::updateWorld(). 
 *  \param ticks Number of physics time steps - should be 1.
 */
void RaceEventManager::update(int ticks)
{
    // Replay all recorded events up to the current time
    // This might adjust dt - if a new state is being played, the dt is
    // determined from the last state till 'now'
    PROFILER_PUSH_CPU_MARKER("RaceEvent:play event", 100, 100, 100);
    RewindManager::get()->playEventsTill(World::getWorld()->getTicksSinceStart(),
                                         &ticks);
    PROFILER_POP_CPU_MARKER();
    World::getWorld()->updateWorld(ticks);
}   // update

// ----------------------------------------------------------------------------
bool RaceEventManager::isRaceOver()
{
    if(!World::getWorld())
        return false;
    return (World::getWorld()->getPhase() > WorldStatus::RACE_PHASE &&
        World::getWorld()->getPhase() != WorldStatus::GOAL_PHASE &&
        World::getWorld()->getPhase() != WorldStatus::IN_GAME_MENU_PHASE);
}   // isRaceOver

// ----------------------------------------------------------------------------
void RaceEventManager::kartFinishedRace(AbstractKart *kart, float time)
{
    if (auto game_events_protocol = m_game_events_protocol.lock())
        game_events_protocol->kartFinishedRace(kart, time);
}   // kartFinishedRace
