//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2014-2015 SuperTuxKart-Team
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

#include "states_screens/dialogs/multitouch_settings_dialog.hpp"

#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/widgets/check_box_widget.hpp"
#include "guiengine/widgets/spinner_widget.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "input/multitouch_device.hpp"
#include "utils/translation.hpp"

#ifdef ANDROID
#include "../../../lib/irrlicht/source/Irrlicht/CIrrDeviceAndroid.h"
#endif

#include <IGUIEnvironment.h>


using namespace GUIEngine;
using namespace irr;
using namespace irr::core;
using namespace irr::gui;

// -----------------------------------------------------------------------------

MultitouchSettingsDialog::MultitouchSettingsDialog(const float w, const float h)
        : ModalDialog(w, h)
{
    loadFromFile("android/multitouch_settings.stkgui");
}

// -----------------------------------------------------------------------------

MultitouchSettingsDialog::~MultitouchSettingsDialog()
{
}

// -----------------------------------------------------------------------------

void MultitouchSettingsDialog::beforeAddingWidgets()
{
    bool accelerometer_available = false;
    bool gyroscope_available = false;
    
#ifdef ANDROID
    CIrrDeviceAndroid* android_device = dynamic_cast<CIrrDeviceAndroid*>(
                                                    irr_driver->getDevice());
    assert(android_device != NULL);
    accelerometer_available = android_device->isAccelerometerAvailable();
    gyroscope_available = android_device->isGyroscopeAvailable() && accelerometer_available;
#endif

    if (!accelerometer_available)
    {
        CheckBoxWidget* accelerometer = getWidget<CheckBoxWidget>("accelerometer");
        assert(accelerometer != NULL);
        accelerometer->setActive(false);
    }

    if (!gyroscope_available)
    {
        CheckBoxWidget* gyroscope = getWidget<CheckBoxWidget>("gyroscope");
        assert(gyroscope != NULL);
        gyroscope->setActive(false);
    }

    updateValues();
}

// -----------------------------------------------------------------------------

GUIEngine::EventPropagation MultitouchSettingsDialog::processEvent(
                                                const std::string& eventSource)
{
    if (eventSource == "close")
    {
        SpinnerWidget* scale = getWidget<SpinnerWidget>("scale");
        assert(scale != NULL);
        UserConfigParams::m_multitouch_scale = (float)scale->getValue() / 100.0f;

        SpinnerWidget* sensitivity_x = getWidget<SpinnerWidget>("sensitivity_x");
        assert(sensitivity_x != NULL);
        UserConfigParams::m_multitouch_sensitivity_x =
                                    (float)sensitivity_x->getValue() / 100.0f;
                                    
        SpinnerWidget* sensitivity_y = getWidget<SpinnerWidget>("sensitivity_y");
        assert(sensitivity_y != NULL);
        UserConfigParams::m_multitouch_sensitivity_y =
                                    (float)sensitivity_y->getValue() / 100.0f;

        SpinnerWidget* deadzone = getWidget<SpinnerWidget>("deadzone");
        assert(deadzone != NULL);
        UserConfigParams::m_multitouch_deadzone =
                                    (float)deadzone->getValue() / 100.0f;

        CheckBoxWidget* buttons_en = getWidget<CheckBoxWidget>("buttons_enabled");
        assert(buttons_en != NULL);
        UserConfigParams::m_multitouch_mode = buttons_en->getState() ? 1 : 0;
        
        CheckBoxWidget* buttons_inv = getWidget<CheckBoxWidget>("buttons_inverted");
        assert(buttons_inv != NULL);
        UserConfigParams::m_multitouch_inverted = buttons_inv->getState();

        CheckBoxWidget* accelerometer = getWidget<CheckBoxWidget>("accelerometer");
        assert(accelerometer != NULL);

        CheckBoxWidget* gyroscope = getWidget<CheckBoxWidget>("gyroscope");
        assert(gyroscope != NULL);

        UserConfigParams::m_multitouch_controls = MULTITOUCH_CONTROLS_STEERING_WHEEL;

        if (accelerometer->getState())
        {
            UserConfigParams::m_multitouch_controls = MULTITOUCH_CONTROLS_ACCELEROMETER;
        }

        if (gyroscope->getState())
        {
            UserConfigParams::m_multitouch_controls = MULTITOUCH_CONTROLS_GYROSCOPE;
        }

        MultitouchDevice* touch_device = input_manager->getDeviceManager()->
                                                        getMultitouchDevice();

        if (touch_device != NULL)
        {
            touch_device->updateConfigParams();
        }

        user_config->saveConfig();

        ModalDialog::dismiss();
        return GUIEngine::EVENT_BLOCK;
    }
    else if (eventSource == "restore")
    {
        UserConfigParams::m_multitouch_sensitivity_x.revertToDefaults();
        UserConfigParams::m_multitouch_sensitivity_y.revertToDefaults();
        UserConfigParams::m_multitouch_deadzone.revertToDefaults();
        UserConfigParams::m_multitouch_mode.revertToDefaults();
        UserConfigParams::m_multitouch_inverted.revertToDefaults();
        UserConfigParams::m_multitouch_controls.revertToDefaults();
        
#ifdef ANDROID
        int32_t screen_size = AConfiguration_getScreenSize(
                                                    global_android_app->config);
        
        switch (screen_size)
        {
        case ACONFIGURATION_SCREENSIZE_SMALL:
        case ACONFIGURATION_SCREENSIZE_NORMAL:
            UserConfigParams::m_multitouch_scale = 1.3f;
            break;
        case ACONFIGURATION_SCREENSIZE_LARGE:
            UserConfigParams::m_multitouch_scale = 1.2f;
            break;
        case ACONFIGURATION_SCREENSIZE_XLARGE:
            UserConfigParams::m_multitouch_scale = 1.1f;
            break;
        default:
            UserConfigParams::m_multitouch_scale.revertToDefaults();
            break;
        }
#else
        UserConfigParams::m_multitouch_scale.revertToDefaults();
#endif

        updateValues();

        return GUIEngine::EVENT_BLOCK;
    }
    else if (eventSource == "accelerometer")
    {
        CheckBoxWidget* gyroscope = getWidget<CheckBoxWidget>("gyroscope");
        assert(gyroscope != NULL);
        gyroscope->setState(false);
    }
    else if (eventSource == "gyroscope")
    {
        CheckBoxWidget* accelerometer = getWidget<CheckBoxWidget>("accelerometer");
        assert(accelerometer != NULL);
        accelerometer->setState(false);
    }

    return GUIEngine::EVENT_LET;
}   // processEvent

// -----------------------------------------------------------------------------

void MultitouchSettingsDialog::updateValues()
{
    SpinnerWidget* scale = getWidget<SpinnerWidget>("scale");
    assert(scale != NULL);
    scale->setValue((int)(UserConfigParams::m_multitouch_scale * 100.0f));

    SpinnerWidget* sensitivity_x = getWidget<SpinnerWidget>("sensitivity_x");
    assert(sensitivity_x != NULL);
    sensitivity_x->setValue(
                (int)(UserConfigParams::m_multitouch_sensitivity_x * 100.0f));
                
    SpinnerWidget* sensitivity_y = getWidget<SpinnerWidget>("sensitivity_y");
    assert(sensitivity_y != NULL);
    sensitivity_y->setValue(
                (int)(UserConfigParams::m_multitouch_sensitivity_y * 100.0f));

    SpinnerWidget* deadzone = getWidget<SpinnerWidget>("deadzone");
    assert(deadzone != NULL);
    deadzone->setValue(
                (int)(UserConfigParams::m_multitouch_deadzone * 100.0f));

    CheckBoxWidget* buttons_en = getWidget<CheckBoxWidget>("buttons_enabled");
    assert(buttons_en != NULL);
    buttons_en->setState(UserConfigParams::m_multitouch_mode != 0);
    
    CheckBoxWidget* buttons_inv = getWidget<CheckBoxWidget>("buttons_inverted");
    assert(buttons_inv != NULL);
    buttons_inv->setState(UserConfigParams::m_multitouch_inverted);

    CheckBoxWidget* accelerometer = getWidget<CheckBoxWidget>("accelerometer");
    assert(accelerometer != NULL);
    accelerometer->setState(UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_ACCELEROMETER);

    CheckBoxWidget* gyroscope = getWidget<CheckBoxWidget>("gyroscope");
    assert(gyroscope != NULL);
    gyroscope->setState(UserConfigParams::m_multitouch_controls == MULTITOUCH_CONTROLS_GYROSCOPE);
}

// -----------------------------------------------------------------------------
