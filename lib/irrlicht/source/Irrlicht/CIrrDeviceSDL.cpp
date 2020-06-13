// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_

#include "CIrrDeviceSDL.h"
#include "IEventReceiver.h"
#include "irrList.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include <stdio.h>
#include <stdlib.h>
#include "SIrrCreationParameters.h"
#include "COpenGLExtensionHandler.h"

extern bool GLContextDebugBit;

namespace irr
{
	namespace video
	{
		extern bool useCoreContext;
		IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
			io::IFileSystem* io, CIrrDeviceSDL* device);
		IVideoDriver* createOGLES2Driver(const SIrrlichtCreationParameters& params,
			io::IFileSystem* io, CIrrDeviceSDL* device, u32 default_fb);

	} // end namespace video

} // end namespace irr

extern "C" void init_objc(SDL_SysWMinfo* info, float* ns, float* top, float* bottom, float* left, float* right);

namespace irr
{

//! constructor
CIrrDeviceSDL::CIrrDeviceSDL(const SIrrlichtCreationParameters& param)
	: CIrrDeviceStub(param),
	Window(0), Context(0), NativeScale(1.0f),
	MouseX(0), MouseY(0), MouseButtonStates(0),
	Width(param.WindowSize.Width), Height(param.WindowSize.Height),
	TopPadding(0), BottomPadding(0), LeftPadding(0), RightPadding(0),
	WindowHasFocus(false), WindowMinimized(false), Resizable(false),
	AccelerometerIndex(-1), AccelerometerInstance(-1),
	GyroscopeIndex(-1), GyroscopeInstance(-1)
{
	#ifdef _DEBUG
	setDebugName("CIrrDeviceSDL");
	#endif

	Operator = 0;
	// Initialize SDL... Timer for sleep, video for the obvious, and
	// noparachute prevents SDL from catching fatal errors.
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
	u32 init_flags = SDL_INIT_TIMER| SDL_INIT_VIDEO| SDL_INIT_GAMECONTROLLER;
#if SDL_VERSION_ATLEAST(2, 0, 9)
	init_flags |= SDL_INIT_SENSOR;
#endif
	if (SDL_Init(init_flags) < 0)
	{
		os::Printer::log( "Unable to initialize SDL!", SDL_GetError());
		Close = true;
	}

	// create keymap
	createKeyMap();

	// create window
	if (CreationParams.DriverType != video::EDT_NULL)
	{
		// create the window, only if we do not use the null device
		if (!Close && createWindow())
		{
			SDL_VERSION(&Info.version);

			if (!SDL_GetWindowWMInfo(Window, &Info))
				return;
#ifdef IOS_STK
			init_objc(&Info, &NativeScale, &TopPadding, &BottomPadding, &LeftPadding, &RightPadding);
			Width *= NativeScale;
			Height *= NativeScale;
			CreationParams.WindowSize.Width = Width;
			CreationParams.WindowSize.Height = Height;
#endif
			core::stringc sdlversion = "SDL Version ";
			sdlversion += Info.version.major;
			sdlversion += ".";
			sdlversion += Info.version.minor;
			sdlversion += ".";
			sdlversion += Info.version.patch;

			Operator = new COSOperator(sdlversion);
			os::Printer::log(sdlversion.c_str(), ELL_INFORMATION);
#if SDL_VERSION_ATLEAST(2, 0, 9)
			for (int i = 0; i < SDL_NumSensors(); i++)
			{
				if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_ACCEL)
					AccelerometerIndex = i;
				else if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_GYRO)
					GyroscopeIndex = i;
			}
#endif
		}
		else
			return;
	}

	// create cursor control
	CursorControl = new CCursorControl(this);

	// create driver
	createDriver();

	if (VideoDriver)
		createGUIAndScene();
}


//! destructor
CIrrDeviceSDL::~CIrrDeviceSDL()
{
	if (VideoDriver)
	{
		VideoDriver->drop();
		VideoDriver = NULL;
	}
	if (Context)
		SDL_GL_DeleteContext(Context);
	if (Window)
		SDL_DestroyWindow(Window);
	SDL_Quit();
}


bool CIrrDeviceSDL::activateAccelerometer(float updateInterval)
{
#if SDL_VERSION_ATLEAST(2, 0, 9)
	if (AccelerometerInstance == -1 && AccelerometerIndex != -1)
	{
		SDL_Sensor* accel = SDL_SensorOpen(AccelerometerIndex);
		if (accel)
			AccelerometerInstance = SDL_SensorGetInstanceID(accel);
	}
#endif
	return AccelerometerInstance != -1;
}


bool CIrrDeviceSDL::deactivateAccelerometer()
{
#if SDL_VERSION_ATLEAST(2, 0, 9)
	if (AccelerometerInstance == -1)
		return false;
	SDL_Sensor* accel = SDL_SensorFromInstanceID(AccelerometerInstance);
	if (!accel)
		return false;
	SDL_SensorClose(accel);
	AccelerometerInstance = -1;
#endif
	return true;
}


bool CIrrDeviceSDL::isAccelerometerActive()
{
	return AccelerometerInstance != -1;
}


bool CIrrDeviceSDL::isAccelerometerAvailable()
{
	return AccelerometerIndex != -1;
}


bool CIrrDeviceSDL::activateGyroscope(float updateInterval)
{
#if SDL_VERSION_ATLEAST(2, 0, 9)
	if (GyroscopeInstance == -1 && GyroscopeIndex != -1)
	{
		SDL_Sensor* gyro = SDL_SensorOpen(GyroscopeIndex);
		if (gyro)
			GyroscopeInstance = SDL_SensorGetInstanceID(gyro);
	}
#endif
	return GyroscopeInstance != -1;
}


bool CIrrDeviceSDL::deactivateGyroscope()
{
#if SDL_VERSION_ATLEAST(2, 0, 9)
	if (GyroscopeInstance == -1)
		return false;
	SDL_Sensor* gyro = SDL_SensorFromInstanceID(GyroscopeInstance);
	if (!gyro)
		return false;
	SDL_SensorClose(gyro);
	GyroscopeInstance = -1;
#endif
	return true;
}


bool CIrrDeviceSDL::isGyroscopeActive()
{
	return GyroscopeInstance != -1;
}


bool CIrrDeviceSDL::isGyroscopeAvailable()
{
	return GyroscopeIndex != -1;
}


bool versionCorrect(int major, int minor)
{
#ifdef _IRR_COMPILE_WITH_OGLES2_
	return true;
#else
	int created_major = 2;
	int created_minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &created_major);
	glGetIntegerv(GL_MINOR_VERSION, &created_minor);
	if (created_major > major ||
		(created_major == major && created_minor >= minor))
		return true;
	return false;
#endif
}

bool CIrrDeviceSDL::createWindow()
{
	// Ignore alpha size here, this follow irr_driver.cpp:450
	// Try 32 and, upon failure, 24 then 16 bit per pixels
	if (CreationParams.Bits == 32)
	{
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	}
	else if (CreationParams.Bits == 24)
	{
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 3);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 3);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 2);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	}

	u32 flags = SDL_WINDOW_SHOWN;
	if (CreationParams.Fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;

	if (CreationParams.DriverType == video::EDT_OPENGL ||
		CreationParams.DriverType == video::EDT_OGLES2)
		flags |= SDL_WINDOW_OPENGL;

#ifdef MOBILE_STK
	flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;
#endif

	tryCreateOpenGLContext(flags);
	if (!Window || !Context)
	{
		os::Printer::log( "Could not initialize display!" );
		return false;
	}

	int swap_interval = CreationParams.SwapInterval;
	if (swap_interval > 1)
		swap_interval = 1;
	SDL_GL_SetSwapInterval(swap_interval);

	return true;
}


void CIrrDeviceSDL::tryCreateOpenGLContext(u32 flags)
{
start:
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, CreationParams.Doublebuffer);
	irr::video::useCoreContext = true;

	if (GLContextDebugBit)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	if (CreationParams.DriverType == video::EDT_OGLES2)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	if (CreationParams.ForceLegacyDevice)
		goto legacy;

#ifdef _IRR_COMPILE_WITH_OGLES2_
	if (Context)
	{
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window)
	{
		SDL_DestroyWindow(Window);
		Window = NULL;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	Window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, CreationParams.WindowSize.Width,
		CreationParams.WindowSize.Height, flags);
	if (Window)
	{
		Context = SDL_GL_CreateContext(Window);
		if (Context && versionCorrect(3, 0)) return;
	}

#else
	if (Context)
	{
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window)
	{
		SDL_DestroyWindow(Window);
		Window = NULL;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	Window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, CreationParams.WindowSize.Width,
		CreationParams.WindowSize.Height, flags);
	if (Window)
	{
		Context = SDL_GL_CreateContext(Window);
		if (Context && versionCorrect(4, 3)) return;
	}

	if (Context)
	{
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window)
	{
		SDL_DestroyWindow(Window);
		Window = NULL;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	Window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, CreationParams.WindowSize.Width,
		CreationParams.WindowSize.Height, flags);
	if (Window)
	{
		Context = SDL_GL_CreateContext(Window);
		if (Context && versionCorrect(3, 3)) return;
	}

	if (Context)
	{
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window)
	{
		SDL_DestroyWindow(Window);
		Window = NULL;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	Window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, CreationParams.WindowSize.Width,
		CreationParams.WindowSize.Height, flags);
	if (Window)
	{
		Context = SDL_GL_CreateContext(Window);
		if (Context && versionCorrect(3, 1)) return;
	}
#endif

legacy:
	irr::video::useCoreContext = false;
	if (Context)
	{
		SDL_GL_DeleteContext(Context);
		Context = NULL;
	}
	if (Window)
	{
		SDL_DestroyWindow(Window);
		Window = NULL;
	}

#ifdef _IRR_COMPILE_WITH_OGLES2_
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
	if (CreationParams.DriverType == video::EDT_OGLES2)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
	Window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, CreationParams.WindowSize.Width,
		CreationParams.WindowSize.Height, flags);
	if (Window)
	{
		Context = SDL_GL_CreateContext(Window);
		if (Context) return;
	}

	if (CreationParams.Doublebuffer)
	{
		CreationParams.Doublebuffer = false;
		goto start;
	}
}

//! create the driver
void CIrrDeviceSDL::createDriver()
{
	switch(CreationParams.DriverType)
	{
	case video::EDT_OPENGL:
		#ifdef _IRR_COMPILE_WITH_OPENGL_
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, this);
		#else
		os::Printer::log("No OpenGL support compiled in.", ELL_ERROR);
		#endif
		break;

	case video::EDT_OGLES2:
	{
		#ifdef _IRR_COMPILE_WITH_OGLES2_
		u32 default_fb = 0;
		#ifdef MOBILE_STK
		default_fb = Info.info.uikit.framebuffer;
		#endif
		VideoDriver = video::createOGLES2Driver(CreationParams, FileSystem, this, default_fb);
		#else
		os::Printer::log("No OpenGL ES 2.0 support compiled in.", ELL_ERROR);
		#endif
		break;
	}

	case video::EDT_NULL:
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		break;

	default:
		os::Printer::log("Unable to create video driver of unknown type.", ELL_ERROR);
		break;
	}
}

// In input_manager.cpp
extern "C" void handle_joystick(SDL_Event& event);
// In CGUIEditBox.cpp
extern "C" void handle_textinput(SDL_Event& event);
//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceSDL::run()
{
	os::Timer::tick();

	SEvent irrevent;
	SDL_Event SDL_event;

	while ( !Close && SDL_PollEvent( &SDL_event ) )
	{
		switch ( SDL_event.type )
		{
#if SDL_VERSION_ATLEAST(2, 0, 9)
		case SDL_SENSORUPDATE:
			if (SDL_event.sensor.which == AccelerometerInstance)
			{
				irrevent.EventType = irr::EET_ACCELEROMETER_EVENT;
				irrevent.AccelerometerEvent.X = SDL_event.sensor.data[0];
				irrevent.AccelerometerEvent.Y = SDL_event.sensor.data[1];
				irrevent.AccelerometerEvent.Z = SDL_event.sensor.data[2];
				// Mobile STK specific
				if (irrevent.AccelerometerEvent.X < 0.0)
					irrevent.AccelerometerEvent.X *= -1.0;
				if (SDL_GetDisplayOrientation(0) == SDL_ORIENTATION_LANDSCAPE)
					irrevent.AccelerometerEvent.Y *= -1.0;
				postEventFromUser(irrevent);
			}
			else if (SDL_event.sensor.which == GyroscopeInstance)
			{
				irrevent.EventType = irr::EET_GYROSCOPE_EVENT;
				irrevent.GyroscopeEvent.X = SDL_event.sensor.data[0];
				irrevent.GyroscopeEvent.Y = SDL_event.sensor.data[1];
				irrevent.GyroscopeEvent.Z = SDL_event.sensor.data[2];
				postEventFromUser(irrevent);
			}
			break;
#endif
		case SDL_FINGERMOTION:
		case SDL_FINGERDOWN:
		case SDL_FINGERUP:
			irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = SDL_event.type == SDL_FINGERMOTION ? irr::ETIE_MOVED :
				SDL_event.type == SDL_FINGERDOWN ? irr::ETIE_PRESSED_DOWN : irr::ETIE_LEFT_UP;
			irrevent.TouchInput.ID = getTouchId(SDL_event.tfinger.fingerId);
			if (SDL_event.type == SDL_FINGERUP)
				removeTouchId(SDL_event.tfinger.fingerId);
			irrevent.TouchInput.X = SDL_event.tfinger.x * Width;
			irrevent.TouchInput.Y = SDL_event.tfinger.y * Height;
			postEventFromUser(irrevent);
			break;

		case SDL_MOUSEWHEEL:
			if (SDL_event.wheel.x > 0 || SDL_event.wheel.x < 0)
				break;
			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = irr::EMIE_MOUSE_WHEEL;
			irrevent.MouseInput.X = MouseX;
			irrevent.MouseInput.Y = MouseY;
			irrevent.MouseInput.ButtonStates = MouseButtonStates;
			irrevent.MouseInput.Wheel = SDL_event.wheel.y > 0 ? 1.0f : -1.0f;
			postEventFromUser(irrevent);
			break;

		case SDL_MOUSEMOTION:
			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;
			MouseX = irrevent.MouseInput.X = SDL_event.motion.x * NativeScale;
			MouseY = irrevent.MouseInput.Y = SDL_event.motion.y * NativeScale;
			irrevent.MouseInput.ButtonStates = MouseButtonStates;

			postEventFromUser(irrevent);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:

			irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.X = SDL_event.button.x * NativeScale;
			irrevent.MouseInput.Y = SDL_event.button.y * NativeScale;

			irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;

			switch(SDL_event.button.button)
			{
			case SDL_BUTTON_LEFT:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN)
				{
					irrevent.MouseInput.Event = irr::EMIE_LMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_LEFT;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_LMOUSE_LEFT_UP;
					MouseButtonStates &= ~irr::EMBSM_LEFT;
				}
				break;

			case SDL_BUTTON_RIGHT:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN)
				{
					irrevent.MouseInput.Event = irr::EMIE_RMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_RIGHT;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_RMOUSE_LEFT_UP;
					MouseButtonStates &= ~irr::EMBSM_RIGHT;
				}
				break;

			case SDL_BUTTON_MIDDLE:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN)
				{
					irrevent.MouseInput.Event = irr::EMIE_MMOUSE_PRESSED_DOWN;
					MouseButtonStates |= irr::EMBSM_MIDDLE;
				}
				else
				{
					irrevent.MouseInput.Event = irr::EMIE_MMOUSE_LEFT_UP;
					MouseButtonStates &= ~irr::EMBSM_MIDDLE;
				}
				break;
			}

			irrevent.MouseInput.ButtonStates = MouseButtonStates;

			if (irrevent.MouseInput.Event != irr::EMIE_MOUSE_MOVED)
			{
				postEventFromUser(irrevent);

				if ( irrevent.MouseInput.Event >= EMIE_LMOUSE_PRESSED_DOWN && irrevent.MouseInput.Event <= EMIE_MMOUSE_PRESSED_DOWN )
				{
					u32 clicks = checkSuccessiveClicks(irrevent.MouseInput.X, irrevent.MouseInput.Y, irrevent.MouseInput.Event);
					if ( clicks == 2 )
					{
						irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_DOUBLE_CLICK + irrevent.MouseInput.Event-EMIE_LMOUSE_PRESSED_DOWN);
						postEventFromUser(irrevent);
					}
					else if ( clicks == 3 )
					{
						irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_TRIPLE_CLICK + irrevent.MouseInput.Event-EMIE_LMOUSE_PRESSED_DOWN);
						postEventFromUser(irrevent);
					}
				}
			}
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				SKeyMap mp;
				mp.SDLKey = SDL_event.key.keysym.sym;
				s32 idx = KeyMap.binary_search(mp);

				EKEY_CODE key;
				if (idx == -1)
					key = (EKEY_CODE)0;
				else
					key = (EKEY_CODE)KeyMap[idx].Win32Key;

				irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
				irrevent.KeyInput.Char = 0;
				irrevent.KeyInput.Key = key;
				irrevent.KeyInput.PressedDown = (SDL_event.type == SDL_KEYDOWN);
				irrevent.KeyInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;
				irrevent.KeyInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL ) != 0;
				postEventFromUser(irrevent);
			}
			break;

		case SDL_QUIT:
			Close = true;
			break;

		case SDL_WINDOWEVENT:
			{
				u32 new_width = SDL_event.window.data1 * NativeScale;
				u32 new_height = SDL_event.window.data2 * NativeScale;
				if (SDL_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
					((new_width != Width) || (new_height != Height)))
				{
					Width = new_width;
					Height = new_height;
					if (VideoDriver)
						VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
				}
				else if (SDL_event.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					WindowMinimized = true;
				}
				else if (SDL_event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
				{
					WindowMinimized = false;
				}
				else if (SDL_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					WindowHasFocus = true;
				}
				else if (SDL_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					WindowHasFocus = false;
				}
			}
			break;

		default:
			handle_joystick(SDL_event);
			handle_textinput(SDL_event);
			break;
		} // end switch

	} // end while

	return !Close;
}

//! Activate any joysticks, and generate events for them.
bool CIrrDeviceSDL::activateJoysticks(core::array<SJoystickInfo> & joystickInfo)
{
	return false;
}



//! pause execution temporarily
void CIrrDeviceSDL::yield()
{
	SDL_Delay(0);
}


//! pause execution for a specified time
void CIrrDeviceSDL::sleep(u32 timeMs, bool pauseTimer)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;
	if (pauseTimer && !wasStopped)
		Timer->stop();

	SDL_Delay(timeMs);

	if (pauseTimer && !wasStopped)
		Timer->start();
}


//! sets the caption of the window
void CIrrDeviceSDL::setWindowCaption(const wchar_t* text)
{
	core::stringc textc = text;
	SDL_SetWindowTitle(Window, textc.c_str());
}


//! presents a surface in the client area
bool CIrrDeviceSDL::present(video::IImage* surface, void* windowId, core::rect<s32>* srcClip)
{
	return false;
}


//! notifies the device that it should close itself
void CIrrDeviceSDL::closeDevice()
{
	Close = true;
}


//! \return Pointer to a list with all video modes supported
video::IVideoModeList* CIrrDeviceSDL::getVideoModeList()
{
	if (!VideoModeList.getVideoModeCount())
	{
		// enumerate video modes.
		int display_count = 0;
		if ((display_count = SDL_GetNumVideoDisplays()) < 1)
		{
			os::Printer::log("No display created: ", SDL_GetError(), ELL_ERROR);
			return &VideoModeList;
		}

		int mode_count = 0;
		if ((mode_count = SDL_GetNumDisplayModes(0)) < 1)
		{
			os::Printer::log("No display modes available: ", SDL_GetError(), ELL_ERROR);
			return &VideoModeList;
		}

		SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
		if (SDL_GetDesktopDisplayMode(0, &mode) == 0)
		{
			VideoModeList.setDesktop(SDL_BITSPERPIXEL(mode.format),
				core::dimension2d<u32>(mode.w * NativeScale, mode.h * NativeScale));
		}

		for (int i = 0; i < mode_count; i++)
		{
			if (SDL_GetDisplayMode(0, i, &mode) == 0)
			{
				VideoModeList.addMode(core::dimension2d<u32>(mode.w * NativeScale, mode.h * NativeScale),
					SDL_BITSPERPIXEL(mode.format));
			}
		}
	}

	return &VideoModeList;
}


//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceSDL::setResizable(bool resize)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
	if (CreationParams.Fullscreen)
		return;
	SDL_SetWindowResizable(Window, resize ? SDL_TRUE : SDL_FALSE);
	Resizable = resize;
#endif
}


bool CIrrDeviceSDL::isResizable() const
{
	if (CreationParams.Fullscreen)
		return false;
	return Resizable;
}


//! Minimizes window if possible
void CIrrDeviceSDL::minimizeWindow()
{
	// do nothing
}


//! Maximize window
void CIrrDeviceSDL::maximizeWindow()
{
	// do nothing
}


//! Restore original window size
void CIrrDeviceSDL::restoreWindow()
{
	// do nothing
}


//! Move window to requested position
bool CIrrDeviceSDL::moveWindow(int x, int y)
{
	if (Window)
	{
		SDL_SetWindowPosition(Window, x, y);
		return true;
	}
	return false;
}


//! Get current window position.
bool CIrrDeviceSDL::getWindowPosition(int* x, int* y)
{
	if (Window)
	{
		SDL_GetWindowPosition(Window, x, y);
		return true;
	}
	return false;
}


//! returns if window is active. if not, nothing need to be drawn
bool CIrrDeviceSDL::isWindowActive() const
{
	return (WindowHasFocus && !WindowMinimized);
}


//! returns if window has focus.
bool CIrrDeviceSDL::isWindowFocused() const
{
	return WindowHasFocus;
}


//! returns if window is minimized.
bool CIrrDeviceSDL::isWindowMinimized() const
{
	return WindowMinimized;
}


//! Set the current Gamma Value for the Display
bool CIrrDeviceSDL::setGammaRamp( f32 red, f32 green, f32 blue, f32 brightness, f32 contrast )
{
	/*
	// todo: Gamma in SDL takes ints, what does Irrlicht use?
	return (SDL_SetGamma(red, green, blue) != -1);
	*/
	return false;
}

//! Get the current Gamma Value for the Display
bool CIrrDeviceSDL::getGammaRamp( f32 &red, f32 &green, f32 &blue, f32 &brightness, f32 &contrast )
{
/*	brightness = 0.f;
	contrast = 0.f;
	return (SDL_GetGamma(&red, &green, &blue) != -1);*/
	return false;
}

void CIrrDeviceSDL::setWindowMinimumSize(u32 width, u32 height)
{
	if (Window)
		SDL_SetWindowMinimumSize(Window, width, height);
}

//! returns color format of the window.
video::ECOLOR_FORMAT CIrrDeviceSDL::getColorFormat() const
{
	if (Window)
	{
		u32 pixel_format = SDL_GetWindowPixelFormat(Window);
		if (SDL_BITSPERPIXEL(pixel_format) == 16)
		{
			if (SDL_ISPIXELFORMAT_ALPHA(pixel_format))
				return video::ECF_A1R5G5B5;
			else
				return video::ECF_R5G6B5;
		}
		else
		{
			if (SDL_ISPIXELFORMAT_ALPHA(pixel_format))
				return video::ECF_A8R8G8B8;
			else
				return video::ECF_R8G8B8;
		}
	}
	else
		return CIrrDeviceStub::getColorFormat();
}


void CIrrDeviceSDL::createKeyMap()
{
	// I don't know if this is the best method  to create
	// the lookuptable, but I'll leave it like that until
	// I find a better version.

	KeyMap.reallocate(105);

	// buttons missing

	KeyMap.push_back(SKeyMap(SDLK_BACKSPACE, IRR_KEY_BACK));
	KeyMap.push_back(SKeyMap(SDLK_TAB, IRR_KEY_TAB));
	KeyMap.push_back(SKeyMap(SDLK_CLEAR, IRR_KEY_CLEAR));
	KeyMap.push_back(SKeyMap(SDLK_RETURN, IRR_KEY_RETURN));

	// combined modifiers missing

	KeyMap.push_back(SKeyMap(SDLK_PAUSE, IRR_KEY_PAUSE));
	KeyMap.push_back(SKeyMap(SDLK_CAPSLOCK, IRR_KEY_CAPITAL));

	// asian letter keys missing

	KeyMap.push_back(SKeyMap(SDLK_ESCAPE, IRR_KEY_ESCAPE));

	// asian letter keys missing

	KeyMap.push_back(SKeyMap(SDLK_SPACE, IRR_KEY_SPACE));
	KeyMap.push_back(SKeyMap(SDLK_PAGEUP, IRR_KEY_PRIOR));
	KeyMap.push_back(SKeyMap(SDLK_PAGEDOWN, IRR_KEY_NEXT));
	KeyMap.push_back(SKeyMap(SDLK_END, IRR_KEY_END));
	KeyMap.push_back(SKeyMap(SDLK_HOME, IRR_KEY_HOME));
	KeyMap.push_back(SKeyMap(SDLK_LEFT, IRR_KEY_LEFT));
	KeyMap.push_back(SKeyMap(SDLK_UP, IRR_KEY_UP));
	KeyMap.push_back(SKeyMap(SDLK_RIGHT, IRR_KEY_RIGHT));
	KeyMap.push_back(SKeyMap(SDLK_DOWN, IRR_KEY_DOWN));

	// select missing
	KeyMap.push_back(SKeyMap(SDLK_PRINTSCREEN, IRR_KEY_PRINT));
	// execute missing
	KeyMap.push_back(SKeyMap(SDLK_PRINTSCREEN, IRR_KEY_SNAPSHOT));

	KeyMap.push_back(SKeyMap(SDLK_INSERT, IRR_KEY_INSERT));
	KeyMap.push_back(SKeyMap(SDLK_DELETE, IRR_KEY_DELETE));
	KeyMap.push_back(SKeyMap(SDLK_HELP, IRR_KEY_HELP));

	KeyMap.push_back(SKeyMap(SDLK_0, IRR_KEY_0));
	KeyMap.push_back(SKeyMap(SDLK_1, IRR_KEY_1));
	KeyMap.push_back(SKeyMap(SDLK_2, IRR_KEY_2));
	KeyMap.push_back(SKeyMap(SDLK_3, IRR_KEY_3));
	KeyMap.push_back(SKeyMap(SDLK_4, IRR_KEY_4));
	KeyMap.push_back(SKeyMap(SDLK_5, IRR_KEY_5));
	KeyMap.push_back(SKeyMap(SDLK_6, IRR_KEY_6));
	KeyMap.push_back(SKeyMap(SDLK_7, IRR_KEY_7));
	KeyMap.push_back(SKeyMap(SDLK_8, IRR_KEY_8));
	KeyMap.push_back(SKeyMap(SDLK_9, IRR_KEY_9));

	KeyMap.push_back(SKeyMap(SDLK_a, IRR_KEY_A));
	KeyMap.push_back(SKeyMap(SDLK_b, IRR_KEY_B));
	KeyMap.push_back(SKeyMap(SDLK_c, IRR_KEY_C));
	KeyMap.push_back(SKeyMap(SDLK_d, IRR_KEY_D));
	KeyMap.push_back(SKeyMap(SDLK_e, IRR_KEY_E));
	KeyMap.push_back(SKeyMap(SDLK_f, IRR_KEY_F));
	KeyMap.push_back(SKeyMap(SDLK_g, IRR_KEY_G));
	KeyMap.push_back(SKeyMap(SDLK_h, IRR_KEY_H));
	KeyMap.push_back(SKeyMap(SDLK_i, IRR_KEY_I));
	KeyMap.push_back(SKeyMap(SDLK_j, IRR_KEY_J));
	KeyMap.push_back(SKeyMap(SDLK_k, IRR_KEY_K));
	KeyMap.push_back(SKeyMap(SDLK_l, IRR_KEY_L));
	KeyMap.push_back(SKeyMap(SDLK_m, IRR_KEY_M));
	KeyMap.push_back(SKeyMap(SDLK_n, IRR_KEY_N));
	KeyMap.push_back(SKeyMap(SDLK_o, IRR_KEY_O));
	KeyMap.push_back(SKeyMap(SDLK_p, IRR_KEY_P));
	KeyMap.push_back(SKeyMap(SDLK_q, IRR_KEY_Q));
	KeyMap.push_back(SKeyMap(SDLK_r, IRR_KEY_R));
	KeyMap.push_back(SKeyMap(SDLK_s, IRR_KEY_S));
	KeyMap.push_back(SKeyMap(SDLK_t, IRR_KEY_T));
	KeyMap.push_back(SKeyMap(SDLK_u, IRR_KEY_U));
	KeyMap.push_back(SKeyMap(SDLK_v, IRR_KEY_V));
	KeyMap.push_back(SKeyMap(SDLK_w, IRR_KEY_W));
	KeyMap.push_back(SKeyMap(SDLK_x, IRR_KEY_X));
	KeyMap.push_back(SKeyMap(SDLK_y, IRR_KEY_Y));
	KeyMap.push_back(SKeyMap(SDLK_z, IRR_KEY_Z));

	KeyMap.push_back(SKeyMap(SDLK_LGUI, IRR_KEY_LWIN));
	KeyMap.push_back(SKeyMap(SDLK_RGUI, IRR_KEY_RWIN));
	// apps missing
	KeyMap.push_back(SKeyMap(SDLK_POWER, IRR_KEY_SLEEP)); //??

	KeyMap.push_back(SKeyMap(SDLK_KP_0, IRR_KEY_NUMPAD0));
	KeyMap.push_back(SKeyMap(SDLK_KP_1, IRR_KEY_NUMPAD1));
	KeyMap.push_back(SKeyMap(SDLK_KP_2, IRR_KEY_NUMPAD2));
	KeyMap.push_back(SKeyMap(SDLK_KP_3, IRR_KEY_NUMPAD3));
	KeyMap.push_back(SKeyMap(SDLK_KP_4, IRR_KEY_NUMPAD4));
	KeyMap.push_back(SKeyMap(SDLK_KP_5, IRR_KEY_NUMPAD5));
	KeyMap.push_back(SKeyMap(SDLK_KP_6, IRR_KEY_NUMPAD6));
	KeyMap.push_back(SKeyMap(SDLK_KP_7, IRR_KEY_NUMPAD7));
	KeyMap.push_back(SKeyMap(SDLK_KP_8, IRR_KEY_NUMPAD8));
	KeyMap.push_back(SKeyMap(SDLK_KP_9, IRR_KEY_NUMPAD9));
	KeyMap.push_back(SKeyMap(SDLK_KP_MULTIPLY, IRR_KEY_MULTIPLY));
	KeyMap.push_back(SKeyMap(SDLK_KP_PLUS, IRR_KEY_ADD));
//	KeyMap.push_back(SKeyMap(SDLK_KP_, IRR_KEY_SEPARATOR));
	KeyMap.push_back(SKeyMap(SDLK_KP_MINUS, IRR_KEY_SUBTRACT));
	KeyMap.push_back(SKeyMap(SDLK_KP_PERIOD, IRR_KEY_DECIMAL));
	KeyMap.push_back(SKeyMap(SDLK_KP_DIVIDE, IRR_KEY_DIVIDE));

	KeyMap.push_back(SKeyMap(SDLK_F1,  IRR_KEY_F1));
	KeyMap.push_back(SKeyMap(SDLK_F2,  IRR_KEY_F2));
	KeyMap.push_back(SKeyMap(SDLK_F3,  IRR_KEY_F3));
	KeyMap.push_back(SKeyMap(SDLK_F4,  IRR_KEY_F4));
	KeyMap.push_back(SKeyMap(SDLK_F5,  IRR_KEY_F5));
	KeyMap.push_back(SKeyMap(SDLK_F6,  IRR_KEY_F6));
	KeyMap.push_back(SKeyMap(SDLK_F7,  IRR_KEY_F7));
	KeyMap.push_back(SKeyMap(SDLK_F8,  IRR_KEY_F8));
	KeyMap.push_back(SKeyMap(SDLK_F9,  IRR_KEY_F9));
	KeyMap.push_back(SKeyMap(SDLK_F10, IRR_KEY_F10));
	KeyMap.push_back(SKeyMap(SDLK_F11, IRR_KEY_F11));
	KeyMap.push_back(SKeyMap(SDLK_F12, IRR_KEY_F12));
	KeyMap.push_back(SKeyMap(SDLK_F13, IRR_KEY_F13));
	KeyMap.push_back(SKeyMap(SDLK_F14, IRR_KEY_F14));
	KeyMap.push_back(SKeyMap(SDLK_F15, IRR_KEY_F15));
	// no higher F-keys

	KeyMap.push_back(SKeyMap(SDLK_NUMLOCKCLEAR, IRR_KEY_NUMLOCK));
	KeyMap.push_back(SKeyMap(SDLK_SCROLLLOCK, IRR_KEY_SCROLL));
	KeyMap.push_back(SKeyMap(SDLK_LSHIFT, IRR_KEY_LSHIFT));
	KeyMap.push_back(SKeyMap(SDLK_RSHIFT, IRR_KEY_RSHIFT));
	KeyMap.push_back(SKeyMap(SDLK_LCTRL,  IRR_KEY_LCONTROL));
	KeyMap.push_back(SKeyMap(SDLK_RCTRL,  IRR_KEY_RCONTROL));
	KeyMap.push_back(SKeyMap(SDLK_LALT,  IRR_KEY_LMENU));
	KeyMap.push_back(SKeyMap(SDLK_RALT,  IRR_KEY_RMENU));

	KeyMap.push_back(SKeyMap(SDLK_PLUS,   IRR_KEY_PLUS));
	KeyMap.push_back(SKeyMap(SDLK_COMMA,  IRR_KEY_COMMA));
	KeyMap.push_back(SKeyMap(SDLK_MINUS,  IRR_KEY_MINUS));
	KeyMap.push_back(SKeyMap(SDLK_PERIOD, IRR_KEY_PERIOD));

	// some special keys missing

	KeyMap.sort();
}


bool CIrrDeviceSDL::supportsTouchDevice() const
{
	return SDL_GetNumTouchDevices() > 0;
}


bool CIrrDeviceSDL::hasOnScreenKeyboard() const
{
	return SDL_HasScreenKeyboardSupport() == SDL_TRUE;
}


bool CIrrDeviceSDL::hasHardwareKeyboard() const
{
#ifdef MOBILE_STK
	return SDL_HasHardwareKeyboardConnected() == SDL_TRUE;
#else
	return true;
#endif
}

} // end namespace irr

#endif // _IRR_COMPILE_WITH_SDL_DEVICE_

