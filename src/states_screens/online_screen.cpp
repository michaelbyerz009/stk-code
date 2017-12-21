//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2015 Marianne Gagnon
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


#include "states_screens/main_menu_screen.hpp"

#include "config/player_manager.hpp"
#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/scalable_font.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/list_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "input/keyboard_device.hpp"
#include "io/file_manager.hpp"
#include "main_loop.hpp"
#include "network/network_config.hpp"
#include "online/request_manager.hpp"
#include "states_screens/online_lan.hpp"
#include "states_screens/online_profile_achievements.hpp"
#include "states_screens/online_profile_servers.hpp"
#include "states_screens/online_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "states_screens/user_screen.hpp"
#include "states_screens/dialogs/message_dialog.hpp"
#include "tracks/track_manager.hpp"
#include "utils/string_utils.hpp"


#include <string>


using namespace GUIEngine;
using namespace Online;

DEFINE_SCREEN_SINGLETON( OnlineScreen );

// ----------------------------------------------------------------------------

OnlineScreen::OnlineScreen() : Screen("online/online.stkgui")
{
    m_online_string = _("Your profile");
    //I18N: Used as a verb, appears on the main networking menu (login button)
    m_login_string = _("Login");
}   // OnlineScreen

// ----------------------------------------------------------------------------

void OnlineScreen::loadedFromFile()
{
}   // loadedFromFile

// ----------------------------------------------------------------------------

void OnlineScreen::beforeAddingWidget()
{
    bool is_logged_in = false;
    PlayerProfile *player = PlayerManager::getCurrentPlayer();
    if (PlayerManager::getCurrentOnlineState() == PlayerProfile::OS_GUEST ||
        PlayerManager::getCurrentOnlineState() == PlayerProfile::OS_SIGNED_IN)
    {
        is_logged_in = true;
    }

    IconButtonWidget* wan = getWidget<IconButtonWidget>("wan");
    wan->setActive(is_logged_in);
    wan->setVisible(is_logged_in);
} // beforeAddingWidget

// ----------------------------------------------------------------------------
//
void OnlineScreen::init()
{
    Screen::init();

    m_online = getWidget<IconButtonWidget>("online");

    if (!MainMenuScreen::m_enable_online)
        m_online->setActive(false);

    m_user_id = getWidget<ButtonWidget>("user-id");
    assert(m_user_id);

    RibbonWidget* r = getWidget<RibbonWidget>("menu_toprow");
    r->setFocusForPlayer(PLAYER_ID_GAME_MASTER);

}   // init

// ----------------------------------------------------------------------------

void OnlineScreen::onUpdate(float delta)
{
    PlayerProfile *player = PlayerManager::getCurrentPlayer();
    if (PlayerManager::getCurrentOnlineState() == PlayerProfile::OS_GUEST  ||
        PlayerManager::getCurrentOnlineState() == PlayerProfile::OS_SIGNED_IN)
    {
        m_online->setActive(true);
        m_online->setLabel(m_online_string);
        m_user_id->setText(player->getLastOnlineName() + "@stk");
    }
    else if (PlayerManager::getCurrentOnlineState() == PlayerProfile::OS_SIGNED_OUT)
    {
        m_online->setActive(true);
        m_online->setLabel(m_login_string);
        m_user_id->setText(player->getName());
    }
    else
    {
        // now must be either logging in or logging out
        m_online->setActive(false);
        m_user_id->setText(player->getName());
    }

    m_online->setLabel(PlayerManager::getCurrentOnlineId() ? m_online_string
                                                           : m_login_string);
}   // onUpdate

// ----------------------------------------------------------------------------

void OnlineScreen::eventCallback(Widget* widget, const std::string& name,
                                   const int playerID)
{
    if (name == "user-id")
    {
        UserScreen::getInstance()->push();
        return;
    }
    else if (name == "back")
    {
        StateManager::get()->popMenu();
        return;
    }

    RibbonWidget* ribbon = dynamic_cast<RibbonWidget*>(widget);
    if (ribbon == NULL) return; // what's that event??

    // ---- A ribbon icon was clicked
    std::string selection =
        ribbon->getSelectionIDString(PLAYER_ID_GAME_MASTER);

    if (selection == "lan")
    {
        OnlineLanScreen::getInstance()->push();
    }
    else if (selection == "wan")
    {
        OnlineProfileServers::getInstance()->push();
    }
    else if (selection == "online")
    {
        if (UserConfigParams::m_internet_status != RequestManager::IPERM_ALLOWED)
        {
            new MessageDialog(_("You can not play online without internet access. "
                                "If you want to play online, go to options, select "
                                " tab 'User Interface', and edit "
                                "\"Connect to the Internet\"."));
            return;
        }
        
        if (PlayerManager::getCurrentOnlineId())
        {
            ProfileManager::get()->setVisiting(PlayerManager::getCurrentOnlineId());
            TabOnlineProfileAchievements::getInstance()->push();
        }
        else
        {
            UserScreen::getInstance()->push();
        }
    }
}   // eventCallback

// ----------------------------------------------------------------------------

void OnlineScreen::tearDown()
{
}   // tearDown

// ----------------------------------------------------------------------------

void OnlineScreen::onDisabledItemClicked(const std::string& item)
{
}   // onDisabledItemClicked
