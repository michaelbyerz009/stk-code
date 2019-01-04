//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013 Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "karts/kart_rewinder.hpp"

#include "items/attachment.hpp"
#include "items/powerup.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/explosion_animation.hpp"
#include "karts/rescue_animation.hpp"
#include "karts/controller/player_controller.hpp"
#include "karts/kart_properties.hpp"
#include "karts/max_speed.hpp"
#include "karts/skidding.hpp"
#include "modes/world.hpp"
#include "network/protocols/client_lobby.hpp"
#include "network/rewind_manager.hpp"
#include "network/network_string.hpp"
#include "physics/btKart.hpp"
#include "utils/vec3.hpp"

#include <string.h>

KartRewinder::KartRewinder(const std::string& ident,
                           unsigned int world_kart_id, int position,
                           const btTransform& init_transform,
                           PerPlayerDifficulty difficulty,
                           std::shared_ptr<RenderInfo> ri)
            : Rewinder(std::string("K") + StringUtils::toString(world_kart_id))
            , Kart(ident, world_kart_id, position, init_transform, difficulty,
                   ri)
{
    m_steering_smoothing_dt = -1.0f;
    m_prev_steering = m_steering_smoothing_time = 0.0f;
}   // KartRewinder

// ----------------------------------------------------------------------------
/** Resets status in case of a resetart.
 */
void KartRewinder::reset()
{
    m_last_animation_end_ticks = -1;
    m_transfrom_from_network =
        btTransform(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f));
    Kart::reset();
    Rewinder::reset();
    SmoothNetworkBody::setEnable(true);
    SmoothNetworkBody::setSmoothRotation(true);
    SmoothNetworkBody::setAdjustVerticalOffset(true);
    m_has_server_state = false;
}   // reset



// ----------------------------------------------------------------------------
/** This function is called immediately before a rewind is done and saves
 *  the current transform for the kart. The difference between this saved
 *  transform and the new transform after rewind is the error that needs
 *  (smoothly) be applied to the graphical position of the kart. 
 */
void KartRewinder::saveTransform()
{
    if (!getKartAnimation())
    {
        Moveable::prepareSmoothing();
        m_skidding->prepareSmoothing();
    }

    m_prev_steering = getSteerPercent();
    m_has_server_state = false;
}   // saveTransform

// ----------------------------------------------------------------------------
void KartRewinder::computeError()
{
    AbstractKartAnimation* ka = getKartAnimation();
    if (ka == NULL)
    {
        Moveable::checkSmoothing();
        m_skidding->checkSmoothing();
    }
    else if (!isEliminated())
        ka->checkNetworkAnimationCreationSucceed(m_transfrom_from_network);

    float diff = fabsf(m_prev_steering - AbstractKart::getSteerPercent());
    if (diff > 0.05f)
    {
        m_steering_smoothing_time = getTimeFullSteer(diff) / 2.0f;
        m_steering_smoothing_dt = 0.0f;
    }
    else
        m_steering_smoothing_dt = -1.0f;

    if (!m_has_server_state && !isEliminated())
    {
        const int kartid = getWorldKartId();
        Log::debug("KartRewinder", "Kart id %d disconnected.", kartid);
        World::getWorld()->eliminateKart(kartid,
            false/*notify_of_elimination*/);
        setPosition(World::getWorld()->getCurrentNumKarts() + 1);
        finishedRace(World::getWorld()->getTime(), true/*from_server*/);
        if (race_manager->supportsLiveJoining())
        {
            RemoteKartInfo& rki = race_manager->getKartInfo(kartid);
            rki.makeReserved();
        }
    }
    else if (m_has_server_state && isEliminated())
    {
        if (auto cl = LobbyProtocol::get<ClientLobby>())
        {
            Log::debug("KartRewinder", "Kart id %d connected.",
                getWorldKartId());
            cl->requestKartInfo((uint8_t)getWorldKartId());
            // New live join kart, hide the node until new kart info is received
            // see ClientLobby::handleKartInfo
            World::getWorld()->addReservedKart(getWorldKartId());
            reset();
            // Final ticks come from server
            m_live_join_util = std::numeric_limits<int>::max();
            getNode()->setVisible(false);
        }
    }
}   // computeError

// ----------------------------------------------------------------------------
/** Saves all state information for a kart in a memory buffer. The memory
 *  is allocated here and the address returned. It will then be managed
 *  by the RewindManager.
 *  \param[out] ru The unique identity of rewinder writing to.
 *  \return The address of the memory buffer with the state.
 */
BareNetworkString* KartRewinder::saveState(std::vector<std::string>* ru)
{
    if (m_eliminated)
        return nullptr;

    ru->push_back(getUniqueIdentity());
    const int MEMSIZE = 17*sizeof(float) + 9+3;

    BareNetworkString *buffer = new BareNetworkString(MEMSIZE);

    // 1) Firing and related handling
    // -----------
    buffer->addUInt16(m_bubblegum_ticks);
    bool plunger_on = m_view_blocked_by_plunger != 0;
    // m_invulnerable_ticks will not be negative
    AbstractKartAnimation* ka = getKartAnimation();
    bool has_animation = ka != NULL && ka->usePredefinedEndTransform();
    uint16_t fire_and_invulnerable = (m_fire_clicked ? 1 << 15 : 0) |
        (has_animation ? 1 << 14 : 0) | (plunger_on ? 1 << 13 : 0) |
        m_invulnerable_ticks;
    buffer->addUInt16(fire_and_invulnerable);
    if (plunger_on)
        buffer->addUInt16(m_view_blocked_by_plunger);

    // 2) Kart animation status (tells the end transformation) or
    // physics values (transform and velocities)
    // -------------------------------------------
    btRigidBody *body = getBody();
    if (has_animation)
    {
        const btTransform& trans = ka->getEndTransform();
        buffer->add(trans.getOrigin());
        btQuaternion quat = trans.getRotation();
        buffer->add(quat);
        unsigned et = ka->getEndTicks() & 134217727;
        et |= ka->getAnimationType() << 27;
        buffer->addUInt32(et);
    }
    else
    {
        const btTransform &t = body->getWorldTransform();
        buffer->add(t.getOrigin());
        btQuaternion q = t.getRotation();
        buffer->add(q);
    }

    buffer->add(body->getLinearVelocity());
    buffer->add(body->getAngularVelocity());
    buffer->addUInt16(m_vehicle->getTimedRotationTicks());
    buffer->addFloat(m_vehicle->getTimedRotation());

    // For collision rewind
    buffer->addUInt16(m_bounce_back_ticks);
    buffer->addUInt16(m_vehicle->getCentralImpulseTicks());
    buffer->add(m_vehicle->getAdditionalImpulse());

    // 3) Steering and other player controls
    // -------------------------------------
    getControls().saveState(buffer);
    getController()->saveState(buffer);

    // 4) Attachment, powerup, nitro
    // -----------------------------
    getAttachment()->saveState(buffer);
    getPowerup()->saveState(buffer);
    buffer->addFloat(getEnergy());

    // 5) Max speed info
    // ------------------
    m_max_speed->saveState(buffer);

    // 6) Skidding
    // -----------
    m_skidding->saveState(buffer);

    return buffer;
}   // saveState

// ----------------------------------------------------------------------------
/** Actually rewind to the specified state. 
 *  \param buffer The buffer with the state info.
 *  \param count Number of bytes that must be used up in this function (not
 *         used).
 */
void KartRewinder::restoreState(BareNetworkString *buffer, int count)
{
    m_has_server_state = true;

    // 1) Firing and related handling
    // -----------
    m_bubblegum_ticks = buffer->getUInt16();
    uint16_t fire_and_invulnerable = buffer->getUInt16();
    m_fire_clicked = ((fire_and_invulnerable >> 15) & 1) == 1;
    bool has_animation = ((fire_and_invulnerable >> 14) & 1) == 1;
    bool has_plunger = ((fire_and_invulnerable >> 13) & 1) == 1;
    if (has_plunger)
        m_view_blocked_by_plunger = buffer->getUInt16();
    else
        m_view_blocked_by_plunger = 0;

    m_invulnerable_ticks = fire_and_invulnerable & ~(1 << 13);

    // 2) Kart animation status or transform and velocities
    // -----------
    m_transfrom_from_network.setOrigin(buffer->getVec3());
    m_transfrom_from_network.setRotation(buffer->getQuat());

    if (has_animation)
    {
        unsigned et = buffer->getUInt32();
        int end_ticks = et & 134217727;
        KartAnimationType kat = (KartAnimationType)(et >> 27);
        if (!getKartAnimation() && end_ticks != m_last_animation_end_ticks)
        {
            Log::info("KartRewinder", "Creating animation %d from state", kat);
            switch (kat)
            {
            case KAT_RESCUE:
                new RescueAnimation(this, false/*is_auto_rescue*/,
                    true/*from_state*/);
                break;
            case KAT_EXPLOSION_DIRECT_HIT:
                new ExplosionAnimation(this, getSmoothedXYZ(),
                    true/*direct_hit*/, true/*from_state*/);
                break;
            case KAT_EXPLOSION:
                new ExplosionAnimation(this, getSmoothedXYZ(),
                    false/*direct_hit*/, true/*from_state*/);
                break;
            default:
                Log::warn("KartRewinder", "Unknown animation %d from state",
                    kat);
                break;
            }
        }
        if (getKartAnimation())
        {
            getKartAnimation()->setEndTransformTicks(m_transfrom_from_network,
                end_ticks);
        }
        m_last_animation_end_ticks = end_ticks;
    }

    Vec3 lv = buffer->getVec3();
    Vec3 av = buffer->getVec3();
    // Don't restore to phyics position if showing kart animation
    if (!getKartAnimation())
    {
        // Clear any forces applied (like by plunger or bubble gum torque)
        btRigidBody *body = getBody();
        body->clearForces();
        body->setLinearVelocity(lv);
        body->setAngularVelocity(av);
        // This function also reads the velocity, so it must be called
        // after the velocities are set
        body->proceedToTransform(m_transfrom_from_network);
        // Update kart transform in case that there are access to its value
        // before Moveable::update() is called (which updates the transform)
        setTrans(m_transfrom_from_network);
    }

    uint16_t time_rot = buffer->getUInt16();
    float timed_rotation_y = buffer->getFloat();
    // Set timed rotation divides by time_rot
    m_vehicle->setTimedRotation(time_rot,
        stk_config->ticks2Time(time_rot) * timed_rotation_y);

    // Collision rewind
    m_bounce_back_ticks = buffer->getUInt16();
    uint16_t central_impulse_ticks = buffer->getUInt16();
    Vec3 additional_impulse = buffer->getVec3();
    m_vehicle->setTimedCentralImpulse(central_impulse_ticks,
        additional_impulse, true/*rewind*/);

    // For the raycast to determine the current material under the kart
    // the m_hardPointWS of the wheels is used. So after a rewind we
    // must restore the m_hardPointWS to the new values, otherwise they
    // would still point at the kart position at the previous rewind
    // (i.e. different terrain --> different slowdown).
    m_vehicle->updateAllWheelTransformsWS();

    // 3) Steering and other controls
    // ------------------------------
    getControls().rewindTo(buffer);
    getController()->rewindTo(buffer);

    // 4) Attachment, powerup, nitro
    // ------------------------------
    getAttachment()->rewindTo(buffer);
    // Required for going back to anvil when rewinding
    updateWeight();

    getPowerup()->rewindTo(buffer);
    float nitro = buffer->getFloat();
    setEnergy(nitro);

    // 5) Max speed info
    // ------------------
    m_max_speed->rewindTo(buffer);

    // 6) Skidding
    // -----------
    m_skidding->rewindTo(buffer);

}   // restoreState

// ----------------------------------------------------------------------------
/** Called once a frame. It will add a new kart control event to the rewind
 *  manager if any control values have changed.
 */
void KartRewinder::update(int ticks)
{
    Kart::update(ticks);
}   // update

// ----------------------------------------------------------------------------
std::function<void()> KartRewinder::getLocalStateRestoreFunction()
{
    if (m_eliminated)
        return nullptr;

    // Variable can be saved locally if its adjustment only depends on the kart
    // itself
    int brake_ticks = m_brake_ticks;
    int8_t min_nitro_ticks = m_min_nitro_ticks;
    float bubblegum_torque = m_bubblegum_torque;

    // Attachment local state
    float initial_speed = getAttachment()->getInitialSpeed();

    // Controller local state
    int steer_val_l = 0;
    int steer_val_r = 0;
    PlayerController* pc = dynamic_cast<PlayerController*>(m_controller);
    if (pc)
    {
        steer_val_l = pc->m_steer_val_l;
        steer_val_r = pc->m_steer_val_r;
    }

    // Max speed local state (terrain)
    float current_fraction = m_max_speed->m_speed_decrease
        [MaxSpeed::MS_DECREASE_TERRAIN].m_current_fraction;
    float max_speed_fraction = m_max_speed->m_speed_decrease
        [MaxSpeed::MS_DECREASE_TERRAIN].m_max_speed_fraction;

    // Skidding local state
    float remaining_jump_time = m_skidding->m_remaining_jump_time;

    return [brake_ticks, min_nitro_ticks, bubblegum_torque,
        initial_speed, steer_val_l, steer_val_r, current_fraction,
        max_speed_fraction, remaining_jump_time, this]()
    {
        m_brake_ticks = brake_ticks;
        m_min_nitro_ticks = min_nitro_ticks;
        m_bubblegum_torque = bubblegum_torque;
        getAttachment()->setInitialSpeed(initial_speed);
        PlayerController* pc = dynamic_cast<PlayerController*>(m_controller);
        if (pc)
        {
            pc->m_steer_val_l = steer_val_l;
            pc->m_steer_val_r = steer_val_r;
        }
        m_max_speed->m_speed_decrease[MaxSpeed::MS_DECREASE_TERRAIN]
            .m_current_fraction = current_fraction;
        m_max_speed->m_speed_decrease[MaxSpeed::MS_DECREASE_TERRAIN]
            .m_max_speed_fraction = max_speed_fraction;
        m_skidding->m_remaining_jump_time = remaining_jump_time;
    };
}   // getLocalStateRestoreFunction
