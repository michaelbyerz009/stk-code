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



#ifndef HEADER_KART_STATS_HPP
#define HEADER_KART_STATS_HPP

#include <irrString.h>

#include "guiengine/widget.hpp"
#include "utils/leak_check.hpp"
#include "utils/ptr_vector.hpp"

#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/progress_bar_widget.hpp"
#include "guiengine/widgets/skill_level_widget.hpp"

class KartProperties;
enum HandicapLevel : uint8_t;

namespace GUIEngine
{
    /**
      * \brief A progress bar widget.
      * \ingroup widgetsgroup
      */
    class KartStatsWidget : public Widget
    {
    public:

        enum Stats
        {
            SKILL_MASS,
            SKILL_SPEED,
            SKILL_ACCELERATION,
            SKILL_NITRO_EFFICIENCY,
            SKILL_COUNT
        };

    private:
        /** When inferring widget size from its label length, this method will be called to
         * if/how much space must be added to the raw label's size for the widget to be large enough */
        virtual int getWidthNeededAroundLabel()  const { return 35; }

        /** When inferring widget size from its label length, this method will be called to
         * if/how much space must be added to the raw label's size for the widget to be large enough */
        virtual int getHeightNeededAroundLabel() const { return 4; }

        int m_player_id;

        std::vector<SkillLevelWidget*> m_skills;

        void setSkillValues(Stats skill_type, float value, const std::string icon_name,
                            const std::string skillbar_propID, const irr::core::stringw icon_tooltip);


    public:

        LEAK_CHECK()

        KartStatsWidget(const int player_id, std::string kart_group,
                        bool multiplayer, bool display_icons);
        virtual ~KartStatsWidget() {};

        // ------------------------------------------------------------------------
        /** Add the widgets to the current screen */
        virtual void add();

        /** Resize the widget */
        virtual void resize();

        // -------------------------------------------------------------------------
        /** Sets the size of the widget as a whole, and placed children widgets
         * inside itself */
        void setValues(const KartProperties* props, HandicapLevel h);

        void hideAll();

        /** Change the value of the widget, it must be a percent. */
        void setValue(Stats type, float value);

        /** Get the current values of the widget. */
        float getValue(Stats type);

        /** If the icons should be displayed. */
        void setDisplayIcons(bool display_icons);
    };
}

#endif
