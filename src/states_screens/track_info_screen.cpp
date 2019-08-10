//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2015 Marianne Gagnon
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

#include "states_screens/track_info_screen.hpp"

#include "challenges/unlock_manager.hpp"
#include "config/player_manager.hpp"
#include "config/user_config.hpp"
#include "graphics/material.hpp"
#include "graphics/stk_tex_manager.hpp"
#include "guiengine/CGUISpriteBank.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/screen.hpp"
#include "guiengine/widgets/button_widget.hpp"
#include "guiengine/widgets/check_box_widget.hpp"
#include "guiengine/widgets/icon_button_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "guiengine/widgets/spinner_widget.hpp"
#include "io/file_manager.hpp"
#include "karts/kart_properties.hpp"
#include "karts/kart_properties_manager.hpp"
#include "race/highscores.hpp"
#include "race/highscore_manager.hpp"
#include "race/race_manager.hpp"
#include "states_screens/state_manager.hpp"
#include "tracks/track.hpp"
#include "tracks/track_manager.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

#include <IGUIEnvironment.h>
#include <IGUIImage.h>
#include <IGUIStaticText.h>

using namespace irr::gui;
using namespace irr::video;
using namespace irr::core;
using namespace GUIEngine;

// ----------------------------------------------------------------------------
/** Constructor, which loads the corresponding track_info.stkgui file. */
TrackInfoScreen::TrackInfoScreen()
          : Screen("track_info.stkgui")
{
}   // TrackInfoScreen

// ----------------------------------------------------------------------------
/* Saves some often used pointers. */
void TrackInfoScreen::loadedFromFile()
{
    m_target_type_spinner   = getWidget<SpinnerWidget>("target-type-spinner");
    m_target_type_label     = getWidget <LabelWidget>("target-type-text");
    m_target_type_div       = getWidget<Widget>("target-type-div");
    m_target_value_spinner  = getWidget<SpinnerWidget>("target-value-spinner");
    m_target_value_label    = getWidget<LabelWidget>("target-value-text");
    m_ai_kart_spinner       = getWidget<SpinnerWidget>("ai-spinner");
    m_ai_kart_label         = getWidget<LabelWidget>("ai-text");
    m_option                = getWidget<CheckBoxWidget>("option");
    m_record_race           = getWidget<CheckBoxWidget>("record");
    m_option->setState(false);
    m_record_race->setState(false);
    
    m_icon_bank = new irr::gui::STKModifiedSpriteBank( GUIEngine::getGUIEnv());
    
    for (unsigned int i=0; i < kart_properties_manager->getNumberOfKarts(); i++)
    {
        const KartProperties* prop = kart_properties_manager->getKartById(i);
        m_icon_bank->addTextureAsSprite(prop->getIconMaterial()->getTexture());
    }
    
    video::ITexture* kart_not_found = irr_driver->getTexture(
              file_manager->getAsset(FileManager::GUI_ICON, "random_kart.png"));

    m_icon_unknown_kart = m_icon_bank->addTextureAsSprite(kart_not_found);

    m_highscore_label = getWidget<LabelWidget>("highscores");

    for (unsigned int i=0;i<HIGHSCORE_COUNT;i++)
    {
        m_highscore_entries = getWidget<ListWidget>("highscore_entries");
    }
    
    GUIEngine::IconButtonWidget* screenshot = getWidget<IconButtonWidget>("screenshot");
    screenshot->setFocusable(false);
    screenshot->m_tab_stop = false;

    m_is_soccer = false;
    m_show_ffa_spinner = false;
}   // loadedFromFile



void TrackInfoScreen::beforeAddingWidget()
{
    m_is_soccer = race_manager->isSoccerMode();
    m_show_ffa_spinner = race_manager->getMinorMode() == RaceManager::MINOR_MODE_3_STRIKES
                        || race_manager->getMinorMode() == RaceManager::MINOR_MODE_FREE_FOR_ALL;

    if (m_is_soccer || m_show_ffa_spinner)
        m_target_type_div->setCollapsed(false, this);
    else
        m_target_type_div->setCollapsed(true, this);
} // beforeAddingWidget

// ----------------------------------------------------------------------------
void TrackInfoScreen::setTrack(Track *track)
{
    m_track = track;
}   // setTrack

// ----------------------------------------------------------------------------
/** Initialised the display. The previous screen has to setup m_track before
 *  pushing this screen using TrackInfoScreen::getInstance()->setTrack().
 */
void TrackInfoScreen::init()
{
    m_record_this_race = false;

    const int max_arena_players = m_track->getMaxArenaPlayers();
    const bool has_laps         = race_manager->modeHasLaps();
    const bool has_highscores   = race_manager->modeHasHighscores();

    getWidget<LabelWidget>("name")->setText(m_track->getName(), false);

    //I18N: when showing who is the author of track '%s'
    //I18N: (place %s where the name of the author should appear)
    getWidget<LabelWidget>("author")->setText( _("Track by %s", m_track->getDesigner()),
                                               false );

    LabelWidget* max_players = getWidget<LabelWidget>("max-arena-players");
    max_players->setVisible(m_track->isArena());
    if (m_track->isArena())
    {
        //I18N: the max players supported by an arena.
        max_players->setText( _("Max players supported: %d", max_arena_players), false );
    }

    // ---- Track screenshot
    GUIEngine::IconButtonWidget* screenshot = getWidget<IconButtonWidget>("screenshot");

    // images are saved squared, but must be stretched to 4:

    // temporary icon, will replace it just after (but it will be shown if the given icon is not found)
    screenshot->m_properties[PROP_ICON] = "gui/icons/main_help.png";

    ITexture* image = STKTexManager::getInstance()
        ->getTexture(m_track->getScreenshotFile(),
        "While loading screenshot for track '%s':", m_track->getFilename());
    if(!image)
    {
        image = STKTexManager::getInstance()->getTexture("main_help.png",
            "While loading screenshot for track '%s':", m_track->getFilename());
    }
    if (image != NULL)
        screenshot->setImage(image);

    m_target_value_spinner->setVisible(false);
    m_target_value_label->setVisible(false);

    // Soccer options
    // -------------
    if (m_is_soccer)
    {
        m_target_type_label->setText(_("Soccer game type"), false);

        m_target_value_spinner->setVisible(true);
        m_target_value_label->setVisible(true);

        if (UserConfigParams::m_num_goals <= 0)
            UserConfigParams::m_num_goals = UserConfigParams::m_num_goals.getDefaultValue();

        if (UserConfigParams::m_soccer_time_limit <= 0)
            UserConfigParams::m_soccer_time_limit = UserConfigParams::m_soccer_time_limit.getDefaultValue();

        m_target_type_spinner->clearLabels();
        m_target_type_spinner->addLabel(_("Time limit"));
        m_target_type_spinner->addLabel(_("Goals limit"));
        m_target_type_spinner->setValue(UserConfigParams::m_soccer_use_time_limit ? 0 : 1);

        if (UserConfigParams::m_soccer_use_time_limit)
        {
            m_target_value_label->setText(_("Maximum time (min.)"), false);
            m_target_value_spinner->setValue(UserConfigParams::m_soccer_time_limit);
        }
        else
        {
            m_target_value_label->setText(_("Number of goals to win"), false);
            m_target_value_spinner->setValue(UserConfigParams::m_num_goals);
        }
    }

    // options for free-for-all and three strikes battle
    // -------------
    if (m_show_ffa_spinner)
    {
        m_target_type_label->setText(_("Game mode"), false);
        m_target_type_spinner->clearLabels();
        m_target_type_spinner->addLabel(_("3 Strikes Battle"));
        m_target_type_spinner->addLabel(_("Free-For-All"));
        m_target_type_spinner->setValue(UserConfigParams::m_use_ffa_mode ? 1 : 0);

        m_target_value_label->setText(_("Maximum time (min.)"), false);
        m_target_value_spinner->setValue(UserConfigParams::m_ffa_time_limit);

        m_target_value_label->setVisible(UserConfigParams::m_use_ffa_mode);
        m_target_value_spinner->setVisible(UserConfigParams::m_use_ffa_mode);
    }

    // Lap count m_lap_spinner
    // -----------------------
    if (has_laps)
    {
        m_target_value_spinner->setVisible(true);
        m_target_value_label->setVisible(true);

        if (UserConfigParams::m_artist_debug_mode)
            m_target_value_spinner->setMin(0);
        else
            m_target_value_spinner->setMin(1);
        m_target_value_spinner->setValue(m_track->getActualNumberOfLap());
        race_manager->setNumLaps(m_target_value_spinner->getValue());

        m_target_value_label->setText(_("Number of laps"), false);
    }

    // Number of AIs
    // -------------
    const int local_players = race_manager->getNumLocalPlayers();
    const bool has_AI =
        (race_manager->getMinorMode() == RaceManager::MINOR_MODE_3_STRIKES ||
         race_manager->getMinorMode() == RaceManager::MINOR_MODE_FREE_FOR_ALL ||
         race_manager->getMinorMode() == RaceManager::MINOR_MODE_SOCCER ?
         m_track->hasNavMesh() && (max_arena_players - local_players) > 0 :
         race_manager->hasAI());
    m_ai_kart_spinner->setVisible(has_AI);
    m_ai_kart_label->setVisible(has_AI);

    if (has_AI)
    {
        m_ai_kart_spinner->setActive(true);

        int num_ai = int(UserConfigParams::m_num_karts_per_gamemode
            [race_manager->getMinorMode()]) - local_players;

        // Avoid negative numbers (which can happen if e.g. the number of karts
        // in a previous race was lower than the number of players now.

        if (num_ai < 0) num_ai = 0;
        m_ai_kart_spinner->setValue(num_ai);

        race_manager->setNumKarts(num_ai + local_players);
        // Set the max karts supported based on the battle arena selected
        if(race_manager->getMinorMode()==RaceManager::MINOR_MODE_3_STRIKES ||
           race_manager->getMinorMode()==RaceManager::MINOR_MODE_SOCCER)
        {
            m_ai_kart_spinner->setMax(max_arena_players - local_players);
        }
        else
            m_ai_kart_spinner->setMax(stk_config->m_max_karts - local_players);
        // A ftl reace needs at least three karts to make any sense
        if(race_manager->getMinorMode()==RaceManager::MINOR_MODE_FOLLOW_LEADER)
        {
            m_ai_kart_spinner->setMin(std::max(0, 3 - local_players));
        }
        // Make sure in battle and soccer mode at least 1 ai for single player
        else if((race_manager->getMinorMode()==RaceManager::MINOR_MODE_3_STRIKES ||
            race_manager->getMinorMode()==RaceManager::MINOR_MODE_SOCCER) &&
            local_players == 1 &&
            !UserConfigParams::m_artist_debug_mode)
            m_ai_kart_spinner->setMin(1);
        else
            m_ai_kart_spinner->setMin(0);

    }   // has_AI
    else
        race_manager->setNumKarts(local_players);

    // Reverse track or random item in arena
    // -------------
    const bool reverse_available = m_track->reverseAvailable() &&
               race_manager->getMinorMode() != RaceManager::MINOR_MODE_EASTER_EGG;
    const bool random_item = m_track->hasNavMesh();

    m_option->setVisible(reverse_available || random_item);
    getWidget<LabelWidget>("option-text")->setVisible(reverse_available || random_item);
    if (reverse_available)
    {
        //I18N: In the track info screen
        getWidget<LabelWidget>("option-text")->setText(_("Drive in reverse"), false);
    }
    else if (random_item)
    {
        //I18N: In the track info screen
        getWidget<LabelWidget>("option-text")->setText(_("Random item location"), false);
    }

    if (reverse_available)
    {
        m_option->setState(race_manager->getReverseTrack());
    }
    else if (random_item)
    {
        m_option->setState(UserConfigParams::m_random_arena_item);
    }
    else
        m_option->setState(false);

    // Record race or not
    // -------------
    const bool record_available = (race_manager->isTimeTrialMode() || race_manager->isEggHuntMode());
    m_record_race->setVisible(record_available);
    getWidget<LabelWidget>("record-race-text")->setVisible(record_available);
    if (race_manager->isRecordingRace())
    {
        // isRecordingRace() is true when it's pre-set by ghost replay selection
        // which force record this race
        m_record_this_race = true;
        m_record_race->setState(true);
        m_record_race->setActive(false);
        m_ai_kart_spinner->setValue(0);
        m_ai_kart_spinner->setActive(false);
        race_manager->setNumKarts(race_manager->getNumLocalPlayers());
        
        UserConfigParams::m_num_karts_per_gamemode[race_manager->getMinorMode()] = race_manager->getNumLocalPlayers();
    }
    else if (record_available)
    {
        m_record_race->setActive(true);
        m_record_race->setState(false);
    }

    // ---- High Scores
    m_highscore_label->setVisible(has_highscores);
    
    int icon_height = GUIEngine::getFontHeight();
    int row_height = GUIEngine::getFontHeight() * 1.2f;
                                                        
    m_icon_bank->setScale(icon_height/128.0f);
    m_icon_bank->setTargetIconSize(128, 128);
    m_highscore_entries->setIcons(m_icon_bank, (int)row_height);
    m_highscore_entries->setVisible(has_highscores);
    
    RibbonWidget* bt_start = getWidget<GUIEngine::RibbonWidget>("buttons");
    bt_start->setFocusForPlayer(PLAYER_ID_GAME_MASTER);

    updateHighScores();
}   // init

// ----------------------------------------------------------------------------
TrackInfoScreen::~TrackInfoScreen()
{
}   // ~TrackInfoScreen


// ----------------------------------------------------------------------------
void TrackInfoScreen::tearDown()
{
    m_highscore_entries->setIcons(NULL);
}

// ----------------------------------------------------------------------------
void TrackInfoScreen::unloaded()
{
    delete m_icon_bank;
    m_icon_bank = NULL;
}   // unloaded

// ----------------------------------------------------------------------------
void TrackInfoScreen::updateHighScores()
{
    if (!race_manager->modeHasHighscores())
        return;

    std::string game_mode_ident = RaceManager::getIdentOf( race_manager->getMinorMode() );
    const Highscores::HighscoreType type = "HST_" + game_mode_ident;

    Highscores* highscores =
        highscore_manager->getHighscores(type,
                                         race_manager->getNumberOfKarts(),
                                         race_manager->getDifficulty(),
                                         m_track->getIdent(),
                                         race_manager->getNumLaps(),
                                         race_manager->getReverseTrack()  );
    const int amount = highscores->getNumberEntries();

    std::string kart_name;
    core::stringw name;
    float time;

    int time_precision = race_manager->currentModeTimePrecision();

    m_highscore_entries->clear();
    
    // fill highscore entries
    for (int n=0; n<HIGHSCORE_COUNT; n++)
    {
        irr::core::stringw line;
        int icon = -1;

        // Check if this entry is filled or still empty
        if (n < amount)
        {
            highscores->getEntry(n, kart_name, name, &time);

            std::string time_string = StringUtils::timeToString(time, time_precision);

            for(unsigned int i=0; i<kart_properties_manager->getNumberOfKarts(); i++)
            {
                const KartProperties* prop = kart_properties_manager->getKartById(i);
                if (kart_name == prop->getIdent())
                {
                    icon = i;
                    break;
                }
            }
        
            line = name + "    " + core::stringw(time_string.c_str());
        }
        else
        {
            //I18N: for empty highscores entries
            line = _("(Empty)");
        }

        if (icon == -1)
        {
            icon = m_icon_unknown_kart;
        }

        std::vector<GUIEngine::ListWidget::ListCell> row;
        
        row.push_back(GUIEngine::ListWidget::ListCell(line.c_str(), icon, 5, false));
        m_highscore_entries->addItem(StringUtils::toString(n), row);
    }
}   // updateHighScores

// ----------------------------------------------------------------------------

void TrackInfoScreen::onEnterPressedInternal()
{
    race_manager->setRecordRace(m_record_this_race);
    // Create a copy of member variables we still need, since they will
    // not be accessible after dismiss:
    const int num_laps = race_manager->modeHasLaps() ? m_target_value_spinner->getValue()
                                                     : -1;
    const bool option_state = m_option == NULL ? false
                                               : m_option->getState();
    // Avoid negative lap numbers (after e.g. easter egg mode).
    if(num_laps>=0)
        m_track->setActualNumberOfLaps(num_laps);

    if(m_track->hasNavMesh())
        UserConfigParams::m_random_arena_item = option_state;
    else
        race_manager->setReverseTrack(option_state);

    // Avoid invaild Ai karts number during switching game modes
    const int max_arena_players = m_track->getMaxArenaPlayers();
    const int local_players = race_manager->getNumLocalPlayers();
    const bool has_AI =
        (race_manager->getMinorMode() == RaceManager::MINOR_MODE_3_STRIKES ||
         race_manager->getMinorMode() == RaceManager::MINOR_MODE_FREE_FOR_ALL ||
         race_manager->getMinorMode() == RaceManager::MINOR_MODE_SOCCER ?
         m_track->hasNavMesh() && (max_arena_players - local_players) > 0 :
         race_manager->hasAI());

    int num_ai = 0;
    if (has_AI)
       num_ai = m_ai_kart_spinner->getValue();

    const int selected_target_type = m_target_type_spinner->getValue();
    const int selected_target_value = m_target_value_spinner->getValue();

    const bool enable_ffa = m_show_ffa_spinner && selected_target_type != 0;

    if (enable_ffa)
    {
        race_manager->setMinorMode(RaceManager::MINOR_MODE_FREE_FOR_ALL);
        race_manager->setHitCaptureTime(0, static_cast<float>(selected_target_value) * 60);
    }

    if (m_is_soccer)
    {
        if (selected_target_type == 0)
            race_manager->setTimeTarget(static_cast<float>(selected_target_value) * 60);
        else
            race_manager->setMaxGoal(selected_target_value);
    }

    if (UserConfigParams::m_num_karts_per_gamemode
        [race_manager->getMinorMode()] != unsigned(local_players + num_ai))
    {
        UserConfigParams::m_num_karts_per_gamemode[race_manager->getMinorMode()] = local_players + num_ai;
    }

    // Disable accidentally unlocking of a challenge
    PlayerManager::getCurrentPlayer()->setCurrentChallenge("");

    race_manager->setNumKarts(num_ai + local_players);
    race_manager->startSingleRace(m_track->getIdent(), num_laps, false);
}   // onEnterPressedInternal

// ----------------------------------------------------------------------------
void TrackInfoScreen::eventCallback(Widget* widget, const std::string& name,
                                   const int playerID)
{
    if (name == "buttons")
    {
        const std::string &button = getWidget<GUIEngine::RibbonWidget>("buttons")
                                  ->getSelectionIDString(PLAYER_ID_GAME_MASTER);
        if(button=="start")
            onEnterPressedInternal();
        else if(button=="back")
            StateManager::get()->escapePressed();
    }
    else if (name == "back")
    {
        StateManager::get()->escapePressed();
    }
    else if (name == "target-type-spinner")
    {
        const bool target_value = m_target_type_spinner->getValue();
        if (m_is_soccer)
        {
            const bool timed = target_value == 0;
            UserConfigParams::m_soccer_use_time_limit = timed;

            if (timed)
            {
                m_target_value_label->setText(_("Maximum time (min.)"), false);
                m_target_value_spinner->setValue(UserConfigParams::m_soccer_time_limit);
            }
            else
            {
                m_target_value_label->setText(_("Number of goals to win"), false);
                m_target_value_spinner->setValue(UserConfigParams::m_num_goals);
            }
        }
        else if (m_show_ffa_spinner)
        {
            const bool enable_ffa = target_value != 0;
            UserConfigParams::m_use_ffa_mode = enable_ffa;

            m_target_value_label->setVisible(enable_ffa);
            m_target_value_spinner->setVisible(enable_ffa);
        }
    }
    else if (name == "target-value-spinner")
    {
        if (m_is_soccer)
        {
            const bool timed = m_target_type_spinner->getValue() == 0;
            if (timed)
                UserConfigParams::m_soccer_time_limit = m_target_value_spinner->getValue();
            else
                UserConfigParams::m_num_goals = m_target_value_spinner->getValue();
        }
        else if (m_show_ffa_spinner)
        {
            const bool enable_ffa = m_target_type_spinner->getValue() != 0;

            if (enable_ffa)
                UserConfigParams::m_ffa_time_limit = m_target_value_spinner->getValue();
        }
        else
        {
            assert(race_manager->modeHasLaps());
            const int num_laps = m_target_value_spinner->getValue();
            race_manager->setNumLaps(num_laps);
            UserConfigParams::m_num_laps = num_laps;
            updateHighScores();
        }
    }
    else if (name == "option")
    {
        if (m_track->hasNavMesh())
        {
            UserConfigParams::m_random_arena_item = m_option->getState();
        }
        else
        {
            race_manager->setReverseTrack(m_option->getState());
            // Makes sure the highscores get swapped when clicking the 'reverse'
            // checkbox.
            updateHighScores();
        }
    }
    else if (name == "record")
    {
        const bool record = m_record_race->getState();
        m_record_this_race = record;
        m_ai_kart_spinner->setValue(0);
        // Disable AI when recording ghost race
        if (record)
        {
            m_ai_kart_spinner->setActive(false);
            race_manager->setNumKarts(race_manager->getNumLocalPlayers());
            UserConfigParams::m_num_karts_per_gamemode[race_manager->getMinorMode()] = race_manager->getNumLocalPlayers();
        }
        else
        {
            m_ai_kart_spinner->setActive(true);
        }
    }
    else if (name=="ai-spinner")
    {
        const int num_ai = m_ai_kart_spinner->getValue();
        UserConfigParams::m_num_karts_per_gamemode[race_manager->getMinorMode()] = race_manager->getNumLocalPlayers() + num_ai;
        updateHighScores();
    }
}   // eventCallback

// ----------------------------------------------------------------------------

