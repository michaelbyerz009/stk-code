// Copyright (C) 2002-2011 Nikolaus Gebhardt
// Copyright (C) 2016-2017 Dawid Gan
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_IRR_DEVICE_ANDROID_H_INCLUDED__
#define __C_IRR_DEVICE_ANDROID_H_INCLUDED__

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_ANDROID_DEVICE_

#include <android/window.h>
#include <android/sensor.h>
#include "stk_android_native_app_glue.h"
#include "CIrrDeviceStub.h"
#include "IrrlichtDevice.h"
#include "IImagePresenter.h"
#include "ICursorControl.h"

#include <map>


namespace irr
{
    enum DeviceOrientation
    {
        ORIENTATION_UNKNOWN,
        ORIENTATION_PORTRAIT,
        ORIENTATION_LANDSCAPE
    };
    
    struct AndroidApplicationInfo
    {
        std::string native_lib_dir;
        std::string data_dir;
        bool initialized;
        
        AndroidApplicationInfo() : initialized(false) {};
    };
    
    class CIrrDeviceAndroid : public CIrrDeviceStub, video::IImagePresenter
    {
    public:
        //! constructor
        CIrrDeviceAndroid(const SIrrlichtCreationParameters& param);

        //! destructor
        virtual ~CIrrDeviceAndroid();

        virtual bool run();
        virtual void yield();
        virtual void sleep(u32 timeMs, bool pauseTimer=false);
        virtual void setWindowCaption(const wchar_t* text);
        virtual void setWindowClass(const char* text) {}
        virtual bool present(video::IImage* surface, void* windowId, core::rect<s32>* srcClip);
        virtual bool isWindowActive() const;
        virtual bool isWindowFocused() const;
        virtual bool isWindowMinimized() const;
        virtual void closeDevice();
        virtual void setResizable( bool resize=false );
        virtual void minimizeWindow();
        virtual void maximizeWindow();
        virtual void restoreWindow();
        virtual bool moveWindow(int x, int y);
        virtual bool getWindowPosition(int* x, int* y);
        virtual E_DEVICE_TYPE getType() const;
        virtual bool activateAccelerometer(float updateInterval);
        virtual bool deactivateAccelerometer();
        virtual bool isAccelerometerActive();
        virtual bool isAccelerometerAvailable();
        virtual bool activateGyroscope(float updateInterval);
        virtual bool deactivateGyroscope();
        virtual bool isGyroscopeActive();
        virtual bool isGyroscopeAvailable();
        virtual void setTextInputEnabled(bool enabled) {TextInputEnabled = enabled;}
        virtual void showKeyboard(bool show);
        
        class CCursorControl : public gui::ICursorControl
        {
        public:

            CCursorControl() : CursorPos(core::position2d<s32>(0, 0)) {}
            virtual void setVisible(bool visible) {}
            virtual bool isVisible() const {return false;}
            virtual void setPosition(const core::position2d<f32> &pos)
            {
                setPosition(pos.X, pos.Y);
            }
            virtual void setPosition(f32 x, f32 y) 
            {
                CursorPos.X = x;
                CursorPos.Y = y;
            }
            virtual void setPosition(const core::position2d<s32> &pos)
            {
                setPosition(pos.X, pos.Y);
            }
            virtual void setPosition(s32 x, s32 y)
            {
                CursorPos.X = x;
                CursorPos.Y = y;
            }
            virtual const core::position2d<s32>& getPosition() 
            {
                return CursorPos;
            }
            virtual core::position2d<f32> getRelativePosition()
            {
                return core::position2d<f32>(0, 0);
            }
            virtual void setReferenceRect(core::rect<s32>* rect=0) {}
        private:
            core::position2d<s32> CursorPos;
        };
        
        static void onCreate();
        static const AndroidApplicationInfo& getApplicationInfo(
                                                    ANativeActivity* activity);

    private:
        android_app* Android;
        ASensorManager* SensorManager;
        ASensorEventQueue* SensorEventQueue;
        const ASensor* Accelerometer;
        const ASensor* Gyroscope;
        bool AccelerometerActive;
        bool GyroscopeActive;
        bool TextInputEnabled;
        static AndroidApplicationInfo ApplicationInfo;

        static bool IsPaused;
        static bool IsFocused;
        static bool IsStarted;
        
        struct TouchEventData
        {
            int x;
            int y;
            ETOUCH_INPUT_EVENT event;
            
            TouchEventData() : x(0), y(0), event(ETIE_COUNT) {};
        };
        
        TouchEventData TouchEventsData[32];
        bool IsMousePressed;
        float GamepadAxisX;
        float GamepadAxisY;
        DeviceOrientation DefaultOrientation;

        video::SExposedVideoData ExposedVideoData;

        std::map<int, EKEY_CODE> KeyMap;
        
        void printConfig();
        void createDriver();
        void createKeyMap();
        void createVideoModeList();
        wchar_t getKeyChar(SEvent& event);
        wchar_t getUnicodeChar(AInputEvent* event);
        static void hideNavBar(ANativeActivity* activity);
        static void readApplicationInfo(ANativeActivity* activity);
        int getRotation();
        DeviceOrientation getDefaultOrientation();
        video::SExposedVideoData& getExposedVideoData();
        
        static void handleAndroidCommandDirect(ANativeActivity* activity, 
                                               int32_t cmd);
        static void handleAndroidCommand(android_app* app, int32_t cmd);
        static s32 handleInput(android_app* app, AInputEvent* event);

        s32 handleTouch(AInputEvent* androidEvent);
        s32 handleKeyboard(AInputEvent* androidEvent);
        s32 handleGamepad(AInputEvent* androidEvent);
    };

} // end namespace irr

#endif // _IRR_COMPILE_WITH_ANDROID_DEVICE_
#endif // __C_IRR_DEVICE_ANDROID_H_INCLUDED__
