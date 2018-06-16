#include "network/protocols/game_events_protocol.hpp"

#include "karts/abstract_kart.hpp"
#include "modes/world.hpp"
#include "network/event.hpp"
#include "network/game_setup.hpp"
#include "network/network_config.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"

#include <stdint.h>

/** This class handles all 'major' game events. E.g.
 *  finishing a race etc. The game events manager is notified from the 
 *  game code, and it calls the corresponding function in this class.
 *  The server then notifies all clients. Clients receive the message
 *  in the synchronous notifyEvent function here, decode the message
 *  and call the original game code. The functions name are identical,
 *  e.g. kartFinishedRace(some parameter) is called from the GameEventManager
 *  on the server, and the received message is then handled by 
 *  kartFinishedRace(const NetworkString &).
 */
GameEventsProtocol::GameEventsProtocol() : Protocol(PROTOCOL_GAME_EVENTS)
{
}   // GameEventsProtocol

// ----------------------------------------------------------------------------
GameEventsProtocol::~GameEventsProtocol()
{
}   // ~GameEventsProtocol

// ----------------------------------------------------------------------------
bool GameEventsProtocol::notifyEvent(Event* event)
{
    // Avoid crash in case that we still receive race events when
    // the race is actually over.
    if (event->getType() != EVENT_TYPE_MESSAGE || !World::getWorld())
        return true;
    NetworkString &data = event->data();
    if (data.size() < 1) // for type
    {
        Log::warn("GameEventsProtocol", "Too short message.");
        return true;
    }
    uint8_t type = data.getUInt8();
    switch (type)
    {
    case GE_KART_FINISHED_RACE:
        kartFinishedRace(data);     break;
    case GE_PLAYER_DISCONNECT:
        eliminatePlayer(data);      break;
    default:
        Log::warn("GameEventsProtocol", "Unkown message type.");
        break;
    }
    return true;
}   // notifyEvent

// ----------------------------------------------------------------------------
void GameEventsProtocol::eliminatePlayer(const NetworkString &data)
{
    assert(NetworkConfig::get()->isClient());
    if (data.size() < 1)
    {
        Log::warn("GameEventsProtocol", "eliminatePlayer: Too short message.");
    }
    int kartid = data.getUInt8();
    World::getWorld()->eliminateKart(kartid, false/*notify_of_elimination*/);
    World::getWorld()->getKart(kartid)->setPosition(
        World::getWorld()->getCurrentNumKarts() + 1);
    World::getWorld()->getKart(kartid)->finishedRace(
        World::getWorld()->getTime());
}   // eliminatePlayer

// ----------------------------------------------------------------------------
/** This function is called from the server when a kart finishes a race. It
 *  sends a notification to all clients about this event.
 *  \param kart The kart that finished the race.
 *  \param time The time at which the kart finished.
 */
void GameEventsProtocol::kartFinishedRace(AbstractKart *kart, float time)
{
    NetworkString *ns = getNetworkString(20);
    ns->setSynchronous(true);
    ns->addUInt8(GE_KART_FINISHED_RACE).addUInt8(kart->getWorldKartId())
       .addFloat(time);
    sendMessageToPeers(ns, /*reliable*/true);
    delete ns;
}   // kartFinishedRace

// ----------------------------------------------------------------------------
/** This function is called on a client when it receives a kartFinishedRace
 *  event from the server. It updates the game with this information.
 *  \param ns The message from the server.
 */
void GameEventsProtocol::kartFinishedRace(const NetworkString &ns)
{
    if (ns.size() < 5)
    {
        Log::warn("GameEventsProtocol", "kartFinisheRace: Too short message.");
        return;
    }

    uint8_t kart_id = ns.getUInt8();
    float time      = ns.getFloat();
    World::getWorld()->getKart(kart_id)->finishedRace(time,
                                                      /*from_server*/true);
}   // kartFinishedRace
