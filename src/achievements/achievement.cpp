//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013-2015 Glenn De Jonghe
//            (C) 2014-2015 Joerg Henrichs
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


#include "achievements/achievement.hpp"
#include "achievements/achievements_manager.hpp"
#include "achievements/achievement_info.hpp"
#include "guiengine/message_queue.hpp"
#include "io/utf_writer.hpp"
#include "config/player_manager.hpp"
#include "utils/log.hpp"
#include "utils/translation.hpp"

#include <stdlib.h>

/** Constructur, initialises this object with the data from the
 *  corresponding AchievementInfo.
 */
Achievement::Achievement(const AchievementInfo * info)
           : m_achievement_info(info)
{
    m_achieved = false;
}   // Achievement

// ----------------------------------------------------------------------------
Achievement::~Achievement()
{
}   // ~Achievement

// ----------------------------------------------------------------------------
/** Loads the value from an XML node.
 *  \param input*/
void Achievement::loadProgress(const XMLNode *node)
{
    node->get("achieved", &m_achieved);
}   // load

// ----------------------------------------------------------------------------
/** Saves the achievement status to a file.
 *  \param Output stream.
 */
void Achievement::saveProgress(UTFWriter &out)
{
    out << "        <achievement id=\"" << getID() << "\" "
        << "achieved=\"" << m_achieved << "\"";
 
    out << "/>\n";
}   // save

// ----------------------------------------------------------------------------
/** Returns the value for a key.
 */
int Achievement::getValue(const std::string & key)
{
    if (m_progress_map.find(key) != m_progress_map.end())
        return m_progress_map[key];
    return 0;
}
// ----------------------------------------------------------------------------
/** Resets all currently key values to 0. Called if the reset-after-race flag
 *  is set for the corresponding AchievementInfo.
 */
void Achievement::reset()
{
    std::map<std::string, int>::iterator iter;
    for (iter = m_progress_map.begin(); iter != m_progress_map.end(); ++iter)
    {
        iter->second = 0;
    }
}   // reset

// ----------------------------------------------------------------------------
/** Returns how much of an achievement has been achieved in the form n/m.
 *  The AchievementInfo adds up all goal values to get 'm', and this
 *  this class end up all current key values for 'n'.
 */
irr::core::stringw Achievement::getProgressAsString() const
{
    int progress = 0;
    std::map<std::string, int>::const_iterator iter;

    // For now return N/N in case of an achieved achievement.
    if (m_achieved)
        return getInfo()->toString() +"/" + getInfo()->toString();

     for (iter = m_progress_map.begin(); iter != m_progress_map.end(); ++iter)
     {
        progress += iter->second;
     }

    return StringUtils::toWString(progress) + "/" + getInfo()->toString();
}   // getProgressAsString

// ----------------------------------------------------------------------------
/** Increases the value of a key by a specified amount, but make sure to not
 *  increase the value above the goal (otherwise the achievement progress
 *  could be 12/10 (e.g. if one track is used 12 times for the Christoffel
 *  achievement), even though the achievement is not achieved.
 *  \param key The key whose value is increased.
 *  \param increase Amount to add to the value of this key.
 */
void Achievement::increase(const std::string & key, 
                           const std::string &goal_key, int increase)
{
    std::map<std::string, int>::iterator it;
    it = m_progress_map.find(key);
    if (it != m_progress_map.end())
    {
        it->second += increase;
        if (it->second > m_achievement_info->getGoalValue(goal_key))
            it->second = m_achievement_info->getGoalValue(goal_key);
    }
    else
    {
        if (increase>m_achievement_info->getGoalValue(goal_key))
            increase = m_achievement_info->getGoalValue(goal_key);
        m_progress_map[key] = increase;
    }
    check();
}   // increase

// ----------------------------------------------------------------------------
/** Checks if this achievement has been achieved.
 */
void Achievement::check()
{
    if(m_achieved)
        return;

    if(m_achievement_info->checkCompletion(this))
    {
        //show achievement
        // Note: the "name" variable is required, see issue #2068
        // calling _("...", info->getName()) is invalid because getName also calls
        // _() and thus the string it returns is mapped to a temporary buffer
        // in theory, it should return a copy of the string, but clang tries to
        // optimise away the copy
        core::stringw name = m_achievement_info->getName();
        core::stringw s = _("Completed achievement \"%s\".", name);
        MessageQueue::add(MessageQueue::MT_ACHIEVEMENT, s);

        // Sends a confirmation to the server that an achievement has been
        // completed, if a user is signed in.
        if (PlayerManager::isCurrentLoggedIn())
        {
            Online::HTTPRequest * request = new Online::HTTPRequest(true);
            PlayerManager::setUserDetails(request, "achieving");
            request->addParameter("achievementid", getID());
            request->queue();
        }

        m_achieved = true;
    }
}   // check

