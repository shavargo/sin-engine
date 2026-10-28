/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2026 SiN MP Engine Contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.
*/

#include <SDL2/SDL.h>
#include <math.h>

#include "../client/client.h"
#include "../client/input.h"

static cvar_t *in_mouse;
static cvar_t *m_filter;
static cvar_t *in_joystick;

void VID_NewWindow( int width, int height );

static int mouse_x = 0;
static int mouse_y = 0;
static int old_mouse_x = 0;
static int old_mouse_y = 0;

static qboolean mouse_captured = false;
static qboolean in_app_active = true;

static SDL_GameController *controller = NULL;

static int MapSDLScancodeToKey( SDL_Scancode sc, SDL_Keycode sym )
{
	if ( sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z )
		return 'a' + ( sc - SDL_SCANCODE_A );

	if ( sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9 )
		return '1' + ( sc - SDL_SCANCODE_1 );
	if ( sc == SDL_SCANCODE_0 )
		return '0';

	switch ( sc )
	{
	case SDL_SCANCODE_RETURN:
		return K_ENTER;
	case SDL_SCANCODE_ESCAPE:
		return K_ESCAPE;
	case SDL_SCANCODE_BACKSPACE:
		return K_BACKSPACE;
	case SDL_SCANCODE_TAB:
		return K_TAB;
	case SDL_SCANCODE_SPACE:
		return K_SPACE;

	case SDL_SCANCODE_MINUS:
		return '-';
	case SDL_SCANCODE_EQUALS:
		return '=';
	case SDL_SCANCODE_LEFTBRACKET:
		return '[';
	case SDL_SCANCODE_RIGHTBRACKET:
		return ']';
	case SDL_SCANCODE_BACKSLASH:
		return '\\';
	case SDL_SCANCODE_SEMICOLON:
		return ';';
	case SDL_SCANCODE_APOSTROPHE:
		return '\'';
	case SDL_SCANCODE_GRAVE:
		return '`';
	case SDL_SCANCODE_COMMA:
		return ',';
	case SDL_SCANCODE_PERIOD:
		return '.';
	case SDL_SCANCODE_SLASH:
		return '/';

	case SDL_SCANCODE_F1:  return K_F1;
	case SDL_SCANCODE_F2:  return K_F2;
	case SDL_SCANCODE_F3:  return K_F3;
	case SDL_SCANCODE_F4:  return K_F4;
	case SDL_SCANCODE_F5:  return K_F5;
	case SDL_SCANCODE_F6:  return K_F6;
	case SDL_SCANCODE_F7:  return K_F7;
	case SDL_SCANCODE_F8:  return K_F8;
	case SDL_SCANCODE_F9:  return K_F9;
	case SDL_SCANCODE_F10: return K_F10;
	case SDL_SCANCODE_F11: return K_F11;
	case SDL_SCANCODE_F12: return K_F12;

	case SDL_SCANCODE_PAUSE:
		return K_PAUSE;
	case SDL_SCANCODE_INSERT:
		return K_INS;
	case SDL_SCANCODE_HOME:
		return K_HOME;
	case SDL_SCANCODE_PAGEUP:
		return K_PGUP;
	case SDL_SCANCODE_DELETE:
		return K_DEL;
	case SDL_SCANCODE_END:
		return K_END;
	case SDL_SCANCODE_PAGEDOWN:
		return K_PGDN;

	case SDL_SCANCODE_RIGHT:
		return K_RIGHTARROW;
	case SDL_SCANCODE_LEFT:
		return K_LEFTARROW;
	case SDL_SCANCODE_DOWN:
		return K_DOWNARROW;
	case SDL_SCANCODE_UP:
		return K_UPARROW;

	case SDL_SCANCODE_KP_DIVIDE:
		return K_KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY:
		return '*';
	case SDL_SCANCODE_KP_MINUS:
		return K_KP_MINUS;
	case SDL_SCANCODE_KP_PLUS:
		return K_KP_PLUS;
	case SDL_SCANCODE_KP_ENTER:
		return K_KP_ENTER;
	case SDL_SCANCODE_KP_1:
		return K_KP_END;
	case SDL_SCANCODE_KP_2:
		return K_KP_DOWNARROW;
	case SDL_SCANCODE_KP_3:
		return K_KP_PGDN;
	case SDL_SCANCODE_KP_4:
		return K_KP_LEFTARROW;
	case SDL_SCANCODE_KP_5:
		return K_KP_5;
	case SDL_SCANCODE_KP_6:
		return K_KP_RIGHTARROW;
	case SDL_SCANCODE_KP_7:
		return K_KP_HOME;
	case SDL_SCANCODE_KP_8:
		return K_KP_UPARROW;
	case SDL_SCANCODE_KP_9:
		return K_KP_PGUP;
	case SDL_SCANCODE_KP_0:
		return K_KP_INS;
	case SDL_SCANCODE_KP_PERIOD:
		return K_KP_DEL;

	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:
		return K_CTRL;
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT:
		return K_SHIFT;
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT:
		return K_ALT;

	default:
		if ( sym > 0 && sym < 128 )
			return (int)sym;
		return 0;
	}
}

void IN_Activate( qboolean active )
{
	in_app_active = active;
	if ( !active )
	{
		if ( mouse_captured )
		{
			SDL_SetRelativeMouseMode( SDL_FALSE );
			mouse_captured = false;
		}
	}
}

static void IN_UpdateMouseCapture( void )
{
	qboolean want_capture = false;

	if ( in_app_active && in_mouse && in_mouse->value )
	{
		// Capture mouse during active in-game play
		if ( cls.key_dest == key_game && cls.state == ca_active )
		{
			want_capture = true;
		}
	}

	if ( want_capture != mouse_captured )
	{
		mouse_captured = want_capture;
		SDL_SetRelativeMouseMode( mouse_captured ? SDL_TRUE : SDL_FALSE );
		if ( mouse_captured )
		{
			mouse_x = 0;
			mouse_y = 0;
		}
	}
}

void IN_Commands( void )
{
}

void IN_Frame( void )
{
	SDL_Event ev;

	IN_UpdateMouseCapture();

	while ( SDL_PollEvent( &ev ) )
	{
		switch ( ev.type )
		{
		case SDL_QUIT:
			Cbuf_AddText( "quit\n" );
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			int key = MapSDLScancodeToKey( ev.key.keysym.scancode, ev.key.keysym.sym );
			if ( key )
			{
				Key_Event( key, ( ev.type == SDL_KEYDOWN ), ev.key.timestamp );
			}
			break;
		}

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			int btn = 0;
			if ( ev.button.button == SDL_BUTTON_LEFT )
				btn = K_MOUSE1;
			else if ( ev.button.button == SDL_BUTTON_RIGHT )
				btn = K_MOUSE2;
			else if ( ev.button.button == SDL_BUTTON_MIDDLE )
				btn = K_MOUSE3;
			else if ( ev.button.button == SDL_BUTTON_X1 )
				btn = K_AUX1;
			else if ( ev.button.button == SDL_BUTTON_X2 )
				btn = K_AUX2;

			if ( btn )
			{
				Key_Event( btn, ( ev.button.state == SDL_PRESSED ), ev.button.timestamp );
			}
			break;
		}

		case SDL_MOUSEWHEEL:
		{
			if ( ev.wheel.y > 0 )
			{
				Key_Event( K_MWHEELUP, true, ev.wheel.timestamp );
				Key_Event( K_MWHEELUP, false, ev.wheel.timestamp );
			}
			else if ( ev.wheel.y < 0 )
			{
				Key_Event( K_MWHEELDOWN, true, ev.wheel.timestamp );
				Key_Event( K_MWHEELDOWN, false, ev.wheel.timestamp );
			}
			break;
		}

		case SDL_MOUSEMOTION:
		{
			if ( mouse_captured )
			{
				mouse_x += ev.motion.xrel;
				mouse_y += ev.motion.yrel;
			}
			break;
		}

		case SDL_WINDOWEVENT:
		{
			if ( ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED )
			{
				IN_Activate( true );
				if ( re.AppActivate )
					re.AppActivate( true );
			}
			else if ( ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST )
			{
				IN_Activate( false );
				if ( re.AppActivate )
					re.AppActivate( false );
			}
			else if ( ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED )
			{
				VID_NewWindow( ev.window.data1, ev.window.data2 );
			}
			break;
		}

		case SDL_CONTROLLERDEVICEADDED:
		{
			if ( !controller )
			{
				controller = SDL_GameControllerOpen( ev.cdevice.which );
				if ( controller )
					Com_Printf( "Controller connected: %s\n", SDL_GameControllerName( controller ) );
			}
			break;
		}

		case SDL_CONTROLLERDEVICEREMOVED:
		{
			if ( controller )
			{
				SDL_GameControllerClose( controller );
				controller = NULL;
				Com_Printf( "Controller disconnected\n" );
			}
			break;
		}

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
		{
			int joy_key = 0;
			switch ( ev.cbutton.button )
			{
			case SDL_CONTROLLER_BUTTON_A:             joy_key = K_JOY1; break;
			case SDL_CONTROLLER_BUTTON_B:             joy_key = K_JOY2; break;
			case SDL_CONTROLLER_BUTTON_X:             joy_key = K_JOY3; break;
			case SDL_CONTROLLER_BUTTON_Y:             joy_key = K_JOY4; break;
			case SDL_CONTROLLER_BUTTON_BACK:          joy_key = K_AUX3; break;
			case SDL_CONTROLLER_BUTTON_START:         joy_key = K_ESCAPE; break;
			case SDL_CONTROLLER_BUTTON_LEFTSTICK:     joy_key = K_AUX5; break;
			case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    joy_key = K_AUX6; break;
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  joy_key = K_AUX7; break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: joy_key = K_AUX8; break;
			case SDL_CONTROLLER_BUTTON_DPAD_UP:       joy_key = K_UPARROW; break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     joy_key = K_DOWNARROW; break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     joy_key = K_LEFTARROW; break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    joy_key = K_RIGHTARROW; break;
			}
			if ( joy_key )
			{
				Key_Event( joy_key, ( ev.cbutton.state == SDL_PRESSED ), ev.cbutton.timestamp );
			}
			break;
		}

		default:
			break;
		}
	}
}

void IN_Move( usercmd_t *cmd )
{
	if ( mouse_captured && ( mouse_x != 0 || mouse_y != 0 ) )
	{
		float speed = sensitivity ? sensitivity->value : 3.0f;

		if ( m_filter && m_filter->value )
		{
			mouse_x = ( mouse_x + old_mouse_x ) / 2;
			mouse_y = ( mouse_y + old_mouse_y ) / 2;
		}
		old_mouse_x = mouse_x;
		old_mouse_y = mouse_y;

		cl.viewangles[YAW] -= ( m_yaw ? m_yaw->value : 0.022f ) * speed * (float)mouse_x;

		if ( freelook && freelook->value )
		{
			cl.viewangles[PITCH] += ( m_pitch ? m_pitch->value : 0.022f ) * speed * (float)mouse_y;
		}

		mouse_x = 0;
		mouse_y = 0;
	}

	// Game controller analog stick movement
	if ( controller && in_joystick && in_joystick->value )
	{
		const Sint16 deadzone = 8000;
		Sint16 lx = SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_LEFTX );
		Sint16 ly = SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_LEFTY );
		Sint16 rx = SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_RIGHTX );
		Sint16 ry = SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_RIGHTY );

		if ( abs( lx ) > deadzone )
			cmd->sidemove += ( (float)lx / 32767.0f ) * ( m_side ? m_side->value : 0.8f ) * 400.0f;

		if ( abs( ly ) > deadzone )
			cmd->forwardmove -= ( (float)ly / 32767.0f ) * ( m_forward ? m_forward->value : 1.0f ) * 400.0f;

		if ( abs( rx ) > deadzone )
			cl.viewangles[YAW] -= ( (float)rx / 32767.0f ) * 3.5f;

		if ( abs( ry ) > deadzone )
			cl.viewangles[PITCH] += ( (float)ry / 32767.0f ) * 3.5f;
	}
}

void IN_Init( void )
{
	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	m_filter = Cvar_Get( "m_filter", "0", CVAR_ARCHIVE );
	sensitivity = Cvar_Get( "sensitivity", "3", CVAR_ARCHIVE );
	m_yaw = Cvar_Get( "m_yaw", "0.022", CVAR_ARCHIVE );
	m_pitch = Cvar_Get( "m_pitch", "0.022", CVAR_ARCHIVE );
	m_forward = Cvar_Get( "m_forward", "1", CVAR_ARCHIVE );
	m_side = Cvar_Get( "m_side", "0.8", CVAR_ARCHIVE );
	freelook = Cvar_Get( "freelook", "1", CVAR_ARCHIVE );
	in_joystick = Cvar_Get( "in_joystick", "1", CVAR_ARCHIVE );

	if ( SDL_WasInit( SDL_INIT_GAMECONTROLLER ) == 0 )
	{
		SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER );
	}

	if ( SDL_NumJoysticks() > 0 )
	{
		for ( int i = 0; i < SDL_NumJoysticks(); i++ )
		{
			if ( SDL_IsGameController( i ) )
			{
				controller = SDL_GameControllerOpen( i );
				if ( controller )
				{
					Com_Printf( "Initialized Game Controller: %s\n", SDL_GameControllerName( controller ) );
					break;
				}
			}
		}
	}
}

void IN_Shutdown( void )
{
	if ( mouse_captured )
	{
		SDL_SetRelativeMouseMode( SDL_FALSE );
		mouse_captured = false;
	}

	if ( controller )
	{
		SDL_GameControllerClose( controller );
		controller = NULL;
	}
}
