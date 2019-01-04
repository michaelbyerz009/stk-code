//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013-2015 Glenn De Jonghe
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

#include "states_screens/dialogs/server_info_dialog.hpp"

#include "guiengine/engine.hpp"
#include "guiengine/widgets/icon_button_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/list_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "guiengine/widgets/text_box_widget.hpp"
#include "network/server.hpp"
#include "network/server_config.hpp"
#include "network/stk_host.hpp"
#include "states_screens/online/networking_lobby.hpp"
#include "states_screens/state_manager.hpp"
#include "tracks/track.hpp"
#include "utils/string_utils.hpp"

#include <IGUIEnvironment.h>

using namespace GUIEngine;
using namespace irr;
using namespace irr::gui;
using namespace Online;

// -----------------------------------------------------------------------------
/** Dialog constructor. 
 *  \param server_id ID of the server of which to display the info.
 *  \param host_id ID of the host.
 *  \param from_server_creation: true if the dialog shows the data of this
 *         server (i.e. while it is being created).
 */
ServerInfoDialog::ServerInfoDialog(std::shared_ptr<Server> server)
                : ModalDialog(0.8f,0.8f), m_server(server), m_password(NULL)
{
    Log::info("ServerInfoDialog", "Server id is %d, owner is %d",
       server->getServerId(), server->getServerOwner());
    m_self_destroy = false;

    loadFromFile("online/server_info_dialog.stkgui");
    getWidget<LabelWidget>("title")->setText(server->getName(), true);

    m_options_widget = getWidget<RibbonWidget>("options");
    assert(m_options_widget != NULL);
    m_join_widget = getWidget<IconButtonWidget>("join");
    assert(m_join_widget != NULL);
    m_cancel_widget = getWidget<IconButtonWidget>("cancel");
    assert(m_cancel_widget != NULL);
    m_options_widget->setFocusForPlayer(PLAYER_ID_GAME_MASTER);

    if (m_server->isPasswordProtected())
    {
        m_password = getWidget<TextBoxWidget>("password");
        m_password->setPasswordBox(true, L'*');
        assert(m_password != NULL);
    }
    else
    {
        getWidget("label_password")->setVisible(false);
        getWidget("password")->setVisible(false);
    }

    auto& players = m_server->getPlayers();
    core::stringw server_info;
    core::stringw difficulty = race_manager->getDifficultyName(
        server->getDifficulty());

    core::stringw each_line = _("Difficulty: %s", difficulty);
    //I18N: In server info dialog
    server_info += each_line;
    server_info += L"\n";

    core::stringw mode = ServerConfig::getModeName(server->getServerMode());
    //I18N: In server info dialog
    each_line = _("Game mode: %s", mode);
    server_info += each_line;
    server_info += L"\n";

    Track* t = server->getCurrentTrack();
    if (t)
    {
        core::stringw track_name = t->getName();
        //I18N: In server info dialog, showing the current track playing in
        //server
        each_line = _("Current track: %s", track_name);
        server_info += each_line;
        server_info += L"\n";
    }

    if (!players.empty())
    {
        // I18N: Show above the player list in server info dialog to
        // indicate the row meanings
        ListWidget* player_list = getWidget<ListWidget>("player-list");
        std::vector<ListWidget::ListCell> row;
        row.push_back(ListWidget::ListCell(_("Rank"), -1, 1, true));
        // I18N: Show above the player list in server info dialog, tell
        // the user name on server
        row.push_back(ListWidget::ListCell(_("Player"), -1, 2, true));
        // I18N: Show above the player list in server info dialog, tell
        // the scores of user calculated by player rankings
        row.push_back(ListWidget::ListCell(_("Scores"), -1, 1, true));
        // I18N: Show above the player list in server info dialog, tell
        // the user time played on server
        row.push_back(ListWidget::ListCell(_("Time played"),
            -1, 1, true));
        player_list->addItem("player", row);
        for (auto& r : players)
        {
            row.clear();
            row.push_back(ListWidget::ListCell(
                std::get<0>(r) == -1 ? L"-" :
                StringUtils::toWString(std::get<0>(r)), -1, 1, true));
            row.push_back(ListWidget::ListCell(std::get<1>(r), -1,
                2, true));
            row.push_back(ListWidget::ListCell(
                std::get<0>(r) == -1 ? L"-" :
                StringUtils::toWString(std::get<2>(r)), -1, 1, true));
            row.push_back(ListWidget::ListCell(
                StringUtils::toWString(std::get<3>(r)), -1, 1, true));
            player_list->addItem("player", row);
        }
    }
    else
    {
        getWidget("player-list")->setVisible(false);
    }
    getWidget("server-info")->setVisible(true);
    getWidget<LabelWidget>("server-info")->setText(server_info, true);

}   // ServerInfoDialog

// -----------------------------------------------------------------------------
ServerInfoDialog::~ServerInfoDialog()
{
}   // ~ServerInfoDialog

// -----------------------------------------------------------------------------
void ServerInfoDialog::requestJoin()
{
    if (m_server->isPasswordProtected())
    {
        assert(m_password != NULL);
        if (m_password->getText().empty())
            return;
        ServerConfig::m_private_server_password =
            StringUtils::wideToUtf8(m_password->getText());
    }
    else
    {
        ServerConfig::m_private_server_password = "";
    }
    STKHost::create();
    NetworkingLobby::getInstance()->setJoinedServer(m_server);
    ModalDialog::dismiss();
    NetworkingLobby::getInstance()->push();
}   // requestJoin

// -----------------------------------------------------------------------------
GUIEngine::EventPropagation
                 ServerInfoDialog::processEvent(const std::string& eventSource)
{
    if (eventSource == m_options_widget->m_properties[PROP_ID])
    {
        const std::string& selection =
                 m_options_widget->getSelectionIDString(PLAYER_ID_GAME_MASTER);
        if (selection == m_cancel_widget->m_properties[PROP_ID])
        {
            m_self_destroy = true;
            return GUIEngine::EVENT_BLOCK;
        }
        else if(selection == m_join_widget->m_properties[PROP_ID])
        {
            requestJoin();
            return GUIEngine::EVENT_BLOCK;
        }
    }
    return GUIEngine::EVENT_LET;
}   // processEvent

// -----------------------------------------------------------------------------
/** When the player pressed enter, select 'join' as default.
 */
void ServerInfoDialog::onEnterPressedInternal()
{
    // If enter was pressed while none of the buttons was focused interpret
    // as join event
    const int playerID = PLAYER_ID_GAME_MASTER;
    if (GUIEngine::isFocusedForPlayer(m_options_widget, playerID))
        return;
    requestJoin();
}   // onEnterPressedInternal

// -----------------------------------------------------------------------------

bool ServerInfoDialog::onEscapePressed()
{
    if (m_cancel_widget->isActivated())
        m_self_destroy = true;
    return false;
}   // onEscapePressed

// -----------------------------------------------------------------------------
void ServerInfoDialog::onUpdate(float dt)
{
    if (m_password && m_password->getText().empty())
        m_join_widget->setActive(false);
    else if (!m_join_widget->isActivated())
        m_join_widget->setActive(true);

    // It's unsafe to delete from inside the event handler so we do it here
    if (m_self_destroy)
    {
        ModalDialog::dismiss();
        return;
    }
}   // onUpdate
