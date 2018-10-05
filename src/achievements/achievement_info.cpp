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
#include "achievements/achievement_info.hpp"
#include "utils/log.hpp"

#include <sstream>
#include <stdlib.h>
#include <assert.h>

/** The constructor reads the dat from the xml information.
 *  \param input XML node for this achievement info.
 */
AchievementInfo::AchievementInfo(const XMLNode * input)
{
    m_id               = 0;
    m_name            = "";
    m_description      = "";
    m_is_secret        = false;
    bool all;
    all = input->get("id",               &m_id              ) &&
          input->get("name",             &m_name            ) &&
          input->get("description",      &m_description     );
    if (!all)
    {
        Log::error("AchievementInfo",
                   "Not all necessary values for achievement defined.");
        Log::error("AchievementInfo",
                   "ID %d name '%s' description '%s'", m_id, m_name.c_str(),
                                                        m_description.c_str());
    }

    input->get("secret", &m_is_secret);

    m_goal_tree.type      = "AND";
    m_goal_tree.value     = -1;
    m_goal_tree.operation = OP_NONE;

    parseGoals(input, m_goal_tree);
}   // AchievementInfo

// ----------------------------------------------------------------------------
/** Parses recursively the list of goals, to construct the tree of goals */
void AchievementInfo::parseGoals(const XMLNode * input, goalTree &parent)
{
    // Now load the goal nodes
    for (unsigned int n = 0; n < input->getNumNodes(); n++)
    {
        const XMLNode *node = input->getNode(n);
        if (node->getName() != "goal")
            continue; // ignore incorrect node

        std::string type;
        if(!node->get("type", &type))
            continue; // missing type, ignore node

        int value;
        if (!node->get("value", &value))
            value = -1;

        std::string operation;
        if (!node->get("operation", &operation))
            operation = "none";

        goalTree child;
        child.type = type;
        child.value = value;
        if (operation == "none")
            child.operation = OP_NONE;
        else if (operation == "+")
            child.operation = OP_ADD;
        else if (operation == "-")
            child.operation = OP_SUBSTRACT;
        else
            continue; // incorrect operation type, ignore node

        if (type=="AND" || type=="AND-AT-ONCE" || type=="OR" || type=="SUM")
        {
            if (type == "SUM")
            {
                if (value <= 0)
                    continue; // SUM nodes need a strictly positive value
            }
            else
            {
                // Logical operators don't have a value or operation defined
                if (value != -1)
                    continue;
                if (child.operation != OP_NONE)
                    continue;
                if (parent.type == "SUM")
                    continue;
            }

            parseGoals(node, child);

            if (child.children.size() == 0)
                continue;
        }
        else
        {
            if (value <= 0)
                continue; // Leaf nodes need a strictly positive value

            if (parent.type == "SUM" && child.operation == OP_NONE)
                continue; // Leaf nodes of a SUM node need an operator
        }

        parent.children.push_back(child);
    }
    if (parent.children.size() != input->getNumNodes())
        Log::error("AchievementInfo",
                  "Incorrect goals for the entries of achievement \"%s\".", m_name.c_str());
} // parseGoals

// ----------------------------------------------------------------------------
/** Copy a goal tree to an EMPTY goal tree by recursion. */
void AchievementInfo::copyGoalTree(goalTree &copy, goalTree &model, bool set_values_to_zero)
{
    copy.type = model.type;
    copy.value = (set_values_to_zero) ? 0 : model.value;
    copy.operation = model.operation;

    for (unsigned int i=0;i<model.children.size();i++)
    {
        goalTree copy_child;
        copyGoalTree(copy_child, model.children[i],set_values_to_zero);
        copy.children.push_back(copy_child);
    }
} // copyGoalTree

// ----------------------------------------------------------------------------
/** Returns a string with a numerical value to display the progress of
 *  this achievement.
 *  If it has multiple goal, it returns the number of goals. If it has
 *  only one (it can be a sum), it returns the required value for that goal.
 * FIXME : don't work well for "all tracks" goals.
 */
irr::core::stringw AchievementInfo::toString()
{
    return StringUtils::toWString(recursiveGoalCount(m_goal_tree));
}   // toString

// ----------------------------------------------------------------------------
int AchievementInfo::recursiveGoalCount(goalTree &parent)
{
    if (parent.children.size() != 1)
        return m_goal_tree.children.size();
    else if (parent.children[0].type == "AND" ||
             parent.children[0].type == "AND-AT-ONCE" || 
             parent.children[0].type == "OR")
        return recursiveGoalCount(parent.children[0]);
    else
        return parent.children[0].value;
} // recursiveGoalCount
