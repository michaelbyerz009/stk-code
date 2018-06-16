//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006-2015 Joerg Henrichs
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

#ifndef HEADER_POWERUPMANAGER_HPP
#define HEADER_POWERUPMANAGER_HPP

#include "utils/no_copy.hpp"

#include "btBulletDynamicsCommon.h"

#include <string>
#include <vector>

class Material;
class XMLNode;
namespace irr
{
    namespace scene { class IMesh; }
}

/**
  * \ingroup items
  */

/** This class manages all powerups. It reads in powerup.xml to get the data,
 *  initialise the static member of some flyables (i.e. powerup.xml contains
 *  info about cakes, plunger etc which needs to be stored), and maintains
 *  the 'weights' (used in randomly chosing which item was collected) for all
 *  items depending on position. The latter is done so that as the first player
 *  you get less advantageous items (but no useless ones either, e.g. anchor),
 *  while as the last you get more useful ones.
 *  The weight distribution works as follow:
 *  The position in a race is mapped to one of five position classes:
 *  first, top, middle, bottom, last - e.g. for a 6 player game the distribution
 *  is:
 *  position  1     2   3      4      5      6
 *  class     first top middle middle bottom last
 *  For each class the weight distribution is read in from powerup.xml:
 *   <!--      bubble cake bowl zipper plunger switch para anvil -->
 *   <last  w="0      1    1    2      2       0      2    2"     />
 *  So a (well, in this case 'the') player belonging to the class 'last'
 *  will not get a bubble gum or switch. Cakes and bowling balls have
 *  lower probability.
 *  At the start of each race two mappings are computed in updateWeightsForRace:
 *  m_position_to_class maps each postion to the class using the function
 *                      convertPositionToClass.
 *  m_powerups_for_position contains a list of items for each class. A item
 *  with higher weight is included more than once, so at runtime we can
 *  just pick a random item from this list to get the right distribution.
 *  In the example above the list for 'last' will be:
 *  [cake, bowling,zipper,zipper,plunger,plunger,parachute,parachute,
 *   anvil,anvil.
 */

class PowerupManager : public NoCopy
{
public:
    // The anvil and parachute must be at the end of the enum, and the
    // zipper just before them (see Powerup::hitBonusBox).
    enum PowerupType {POWERUP_NOTHING,
                      POWERUP_FIRST,
                      POWERUP_BUBBLEGUM = POWERUP_FIRST,
                      POWERUP_CAKE,
                      POWERUP_BOWLING, POWERUP_ZIPPER, POWERUP_PLUNGER,
                      POWERUP_SWITCH, POWERUP_SWATTER, POWERUP_RUBBERBALL,
                      POWERUP_PARACHUTE,
                      POWERUP_ANVIL,      //powerup.cpp assumes these two come last
                      POWERUP_LAST=POWERUP_ANVIL,
                      POWERUP_MAX
    };

    /** The different position classes, used to map a kart's position to a
     *  weight distribution for the different powerups. The battle mode is
     *  listed as a separate 'position' - this way the same implementation
     *  as used for normal racing can be used to define which items are
     *  available in battle mode*/
    enum PositionClass {POSITION_FIRST,
                        POSITION_TOP33,
                        POSITION_MID33,
                        POSITION_END33,
                        POSITION_LAST,
                        POSITION_BATTLE_MODE,
                        POSITION_SOCCER_MODE,
                        POSITION_TUTORIAL_MODE,
                        POSITION_COUNT};

private:
    /** The icon for each powerup. */
    Material*     m_all_icons [POWERUP_MAX];

    /** Last time the bouncing ball was collected */
    int           m_rubber_ball_collect_ticks;

    /** The mesh for each model (if the powerup has a model), e.g. a switch
        has none. */
    irr::scene::IMesh *m_all_meshes[POWERUP_MAX];

    /** For each powerup the weight (probability) used depending on the
     *  number of players. */
    std::vector<int> m_weights[POSITION_COUNT];

    /** A list of all powerups for a specific class. If a powerup
     *  has weight 5, it will be listed 5 times in this list, so
     *  randomly picking an entry from this for a position class will
     *  result in the right distribution of items. */
    std::vector<PowerupType> m_powerups_for_position[POSITION_COUNT];

    /** The mapping of each position to the corresponding position class.
     *  There is one map for each different number of players, so it is
     *  used like  m_position_to_class[number_players][position] */
    std::vector<PositionClass> m_position_to_class;

    PowerupType   getPowerupType(const std::string &name) const;
    void          loadWeights(const XMLNode &root,
                              const std::string &class_name,
                              PositionClass position_class);
    PositionClass convertPositionToClass(unsigned int num_karts,
                                         unsigned int position);
public:
                  PowerupManager  ();
                 ~PowerupManager  ();
    void          loadAllPowerups ();
    void          unloadPowerups  ();
    void          LoadPowerup     (PowerupType type, const XMLNode &node);
    void          updateWeightsForRace(unsigned int num_karts);
    PowerupManager::PowerupType
        getRandomPowerup(unsigned int pos, unsigned int *n, int random_number);
    // ------------------------------------------------------------------------
    /** Returns the icon(material) for a powerup. */
    Material* getIcon(int type) const {return m_all_icons [type];}
    // ------------------------------------------------------------------------
    /** Returns the mesh for a certain powerup.
     *  \param type Mesh type for which the model is returned. */
    irr::scene::IMesh *getMesh(int type) const {return m_all_meshes[type];}
    // ------------------------------------------------------------------------
    /** Returns the last time a rubber ball was collected. */
    int getBallCollectTicks() const {return m_rubber_ball_collect_ticks;}
    // ------------------------------------------------------------------------
    /** Updates the last time at which a rubber ball was collected. */
    void setBallCollectTicks(int ticks) {m_rubber_ball_collect_ticks=ticks;}
    // ------------------------------------------------------------------------
};   // class PowerupManager

extern PowerupManager* powerup_manager;

#endif
