//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2018
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


#ifndef HEADER_SPEEDRUN_MODE_DIALOG_HPP
#define HEADER_SPEEDRUN_MODE_DIALOG_HPP

#include "guiengine/modaldialog.hpp"
#include "utils/cpp2011.hpp"

/**
 * \brief Dialog shown after a resolution switch sot he user may confirm if
 *        the resolution works.
 * \ingroup states_screens
 */
class SpeedrunModeDialog : public GUIEngine::ModalDialog
{
public:

    SpeedrunModeDialog();
    void onEnterPressedInternal() OVERRIDE;
    GUIEngine::EventPropagation processEvent(const std::string& eventSource) OVERRIDE;
    
    virtual bool onEscapePressed() OVERRIDE;
};
  

#endif
