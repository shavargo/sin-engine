//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/menu3d.c                                               $
// $Revision:: 8                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 9/27/01 1:11 PM                                                $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
//

#include <limits.h>
#include <ctype.h>
#include "client.h"
#include "keys.h"
#ifdef __USEA3D
#include "sin_a3d.h"
#include "A3DWrapperDP.h"
#endif

// Ref stuff

cvar_t *gl_mode;
cvar_t *gl_driver;
cvar_t *gl_picmip;
cvar_t *gl_ext_palettedtexture;
cvar_t *gl_finish;
static cvar_t *sw_mode;
static cvar_t *sw_stipplealpha;

float cl_inventory_scale=0;

#define REF_SOFT          0
#define REF_OPENGL     1
/*#define REF_3DFX	      2
 #define REF_POWERVR    3
 #define REF_VERITE     4
 #define REF_RIVA        5
 #define REF_ATIRAGEPRO  6
 #define REF_I740        7
 */

static void MENU_ForceLogon_f( void );

qboolean inventory_active = false;
static qboolean help_active = false;
static void *hires_helpdata;
static void *lores_helpdata;

//============================
// Multiplayer data
//============================

#define  MAX_LOCAL_SERVERS 8
static int m_num_servers;
char local_server_names[MAX_LOCAL_SERVERS][80];
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

//============================
// Save game data
//============================
char m_savestrings[MAX_SAVEGAMES][32];
qboolean m_savevalid[MAX_SAVEGAMES];

extern int VID_GetModeCount (void); /* FS */
extern const char *VID_GetModeDescription (int mode); /* FS */

//==================================================================
// UTILITIES
//==================================================================

//=========
//ClampCvar
//=========
static float ClampCvar( float min, float max, float value )
{
	if ( value < min )
		return min;
	if ( value > max )
		return max;

	return value;
}


//=====================
//MENU_GetStringForName
//
//Given the name of the menu, returns it's value as a string.
//Currently used to grab list items.
//=====================
static const char *MENU_GetStringForName( const char *name, console3d_t *p )
{
	int i,j;

	for (i=0; i<p->numlevels; i++)
	{
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i] || !p->menu3d[i]->item[j])
				continue;
			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			if (p->menu3d[i]->item[j]->type == LIST3D)
			{
				menulist3d_t *list = &p->menu3d[i]->item[j]->list;
				if (!strcmp( name,list->name ))
				{
					return (list->itemnames[list->curvalue]);
				}
			}
			else if (p->menu3d[i]->item[j]->type == FIELD3D)
			{
				menufield3d_t *field = &p->menu3d[i]->item[j]->field;
				if (!strcmp( name,field->name ))
				{
					return (field->buffer);
				}
			}

		}
	}
	return NULL;
}

//=======================
//MENU_UpdateValueForName
//
//Sets the menu item's value that is linked with the cvar
//=======================
static void MENU_UpdateValueForName( const char *name, float value, console3d_t *p )
{
	int i,j;

	for (i=0; i<p->numlevels; i++)
	{
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			if (p->menu3d[i]->item[j]->type == LIST3D)
			{
				const char *list_name = p->menu3d[i]->item[j]->list.name;
				if (!strcmp( name,list_name ))
				{
					p->menu3d[i]->item[j]->list.curvalue = (int)value;
					Cvar_SetValue ( name, value );
				}
			}
			else
			{
				if (p->menu3d[i]->item[j]->type == SLIDER3D)
				{
					const char *slider_name = p->menu3d[i]->item[j]->slider.name;
					if (!strcmp( name,slider_name ))
					{
						p->menu3d[i]->item[j]->slider.curvalue = value;
						Cvar_SetValue ( name, value );
					}
				}
			}
		}
	}
}

//======================
//MENU_UpdateBoundValues
//
//Updates menu items based on the cvars they are bound to.
//======================
static void MENU_UpdateBoundValues( console3d_t *p )
{
	int i,j,k;
	char        *s;
	const char  *token;
	char cvar[80];
	char cond[32];
	float condvalue;
	float value;
	const char  *stringvalue="";
	qboolean r;
	qboolean searchforitem=false;

	for (i=0; i<p->numlevels; i++)
	{
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			searchforitem = false;

			if (!p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			if (p->menu3d[i]->item[j]->localdata)
			{
				// Check to see if we need to set this menu item from the
				// specified cvar.
				s = p->menu3d[i]->item[j]->localdata;

				while (1)
				{
					token = COM_Parse((const char **)&s );
					if (!token[0])
						break;

					if (!strncmp( token,"cvar",4 ))
					{
						if (strstr( token,"divide" ))
						{
							float scale = Sinatof( COM_Parse((const char **)&s ));

							strncpy( cvar,COM_Parse((const char **)&s ),sizeof(cvar)-1 );
							// Scale the value before setting the menu
							value = Cvar_VariableValue( cvar ) * scale;
						}
						else if (strstr( token,"scale" ))
						{
							float scale = Sinatof( COM_Parse((const char **)&s ));

							strncpy( cvar,COM_Parse((const char **)&s ),sizeof(cvar)-1 );
							// Scale the value before setting the menu
							value = Cvar_VariableValue( cvar ) / scale;
						}
						else if (strstr( token,"clamp" ))
						{
							// Clamp the cvar before we set the menu's value
							float min,max;

							min = Sinatof( COM_Parse((const char **)&s ));
							max = Sinatof( COM_Parse((const char **)&s ));
							strncpy( cvar,COM_Parse((const char **)&s ),sizeof(cvar)-1 );
							Cvar_SetValue( cvar,ClampCvar( min,max,Cvar_VariableValue( cvar )));
							value = Cvar_VariableValue( cvar );
						}
						else if (strstr( token,"cond" ))
						{
							// Set this menuitem based on the outcome of the condition

							// Get the cvar
							strncpy( cvar,COM_Parse((const char **)&s ),sizeof(cvar)-1 );

							// Get the condition
							strncpy( cond,COM_Parse((const char **)&s ),sizeof(cond)-1 );

							condvalue = Sinatof( COM_Parse((const char **)&s ));

							// evaluate condition
							if (!strcmp( cond,"ifless" ))
								r = (Cvar_VariableValue( cvar ) < condvalue);
							else if (!strcmp( cond,"ifgreater" ))
								r = (Cvar_VariableValue( cvar ) > condvalue);
							else if (!strcmp( cond,"iflessequal" ))
								r = (Cvar_VariableValue( cvar ) <= condvalue);
							else if (!strcmp( cond,"ifgreaterequal" ))
								r = (Cvar_VariableValue( cvar ) >= condvalue);
							else if (!strcmp( cond,"ifequal" ))
								r = (Cvar_VariableValue( cvar ) == condvalue);
							else if (!strcmp( cond,"ifnequal" ))
								r = (Cvar_VariableValue( cvar ) != condvalue);
							else	r = false;			/* silence compiler */

							value = r;
						}
						else if (strstr( token,"item" ))
						{
							searchforitem = true;
							strncpy( cvar,COM_Parse((const char **)&s ),sizeof(cvar)-1 );
							stringvalue = Cvar_VariableString( cvar );
							value = 0;		/* silence compiler */
						}
						else
						{
							// Just get the cvar's name & value
							strncpy( cvar,COM_Parse((const char **)&s ),sizeof(cvar)-1 );
							value = Cvar_VariableValue( cvar );
						}

						// Set the menu's value
						if (p->menu3d[i]->item[j]->type == LIST3D)
						{
							menulist3d_t *list = &p->menu3d[i]->item[j]->list;

							// Search through the actionitems for the value to set the cvar to, actionitems don't exist
							// search through the nameditems
							if ( searchforitem )
							{
								if ( list->itemactions )
								{
									for (k=0; k<list->numitems; k++)
									{
										if (!strcmp( list->itemactions[k], stringvalue ))
										{
											list->curvalue = k;
											break;
										}
									}
								}
								else if ( list->itemnames )
								{
									for (k=0; k<list->numitems; k++)
									{
										if (!strcmp( list->itemnames[k], stringvalue ))
										{
											list->curvalue = k;
											break;
										}
									}
								}
							}
							else
							{
								list->curvalue = (int)value;
							}
						}
						else if (p->menu3d[i]->item[j]->type == SLIDER3D)
						{
							menuslider3d_t *slider = &p->menu3d[i]->item[j]->slider;
							slider->curvalue = value;
						}
						else if (p->menu3d[i]->item[j]->type == FIELD3D)
						{
							menufield3d_t *field = &p->menu3d[i]->item[j]->field;
							strncpy( field->buffer,Cvar_VariableString( cvar ),field->length-1 );
						}
					}
				}
			}
		}
	}
}

//=======================
//MENU_FindKeysForCommand
//
//Given a command, returns the keys that are bound to it
//=======================
void MENU_FindKeysForCommand( const char *command, int *twokeys )
{
	int count;
	int j;
	int l;
	char    *b;

	twokeys[0] = twokeys[1] = -1;
	l = (int)strlen( command );
	count = 0;

	for (j=0; j<256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp ( b, command, l ) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

//=============================
//MENU_FindDoubleKeysForCommand
//
//Given a command, returns the keys that are bound to it
//=============================
void MENU_FindDoubleKeysForCommand( const char *command, int *twokeys )
{
	int count;
	int j;
	int l;
	char    *b;

	twokeys[0] = twokeys[1] = -1;
	l = (int)strlen( command );
	count = 0;

	for (j=0; j<256; j++)
	{
		b = keydoublebindings[j];
		if (!b)
			continue;
		if (!strncmp ( b, command, l ) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

//==================
//MENU_UnbindCommand
//
//Give a command unbinds all keys that use it.
//==================
static void MENU_UnbindCommand( char *command )
{
	int j;
	int l;
	char       *b,*c;

	l = (int)strlen( command );

	for (j=0; j<256; j++)
	{
		b = keybindings[j];
		c = keydoublebindings[j];

		if ( b )
			if (!strncmp ( b, command, l ) )
				Key_SetBinding ( j, "" );
		if ( c )
			if (!strncmp ( c, command, l ) )
				Key_SetDoubleBinding ( j, "" );
	}
}

//===================
//MENU_KeyBindingFunc
//===================
static void MENU_KeyBindingFunc( menubind3d_t *b )
{
	int keys[2];

	MENU_FindKeysForCommand( b->command, keys );

	if ( keys[1] != -1 )
		MENU_UnbindCommand( b->command );

	b->bind_grab_active = true;
}



//===============================================================
// KEY HANDLERS
//===============================================================

//================
//MENU_Default_Key_f
//
//Default input key handler
//================
static const char *MENU_Default_Key_f( int key )
{
	console3d_t *p = cls.currentconsole;
	item3d_t    *item = p->menu3d[p->current_menu_level]->item[p->sel_menu_item];
	assert( p );

	if ( key == K_ESCAPE )
	{
		qboolean ret;
		ret = CONSOLE_PopMenu( p );
		if (!ret)
		{
			CL_RandomLocalSound( menu_out_sound );
			CONSOLE_Exit( p );
		}
	}

	if ( key == K_ENTER )
	{
		if ( item )
			CONSOLE_ProcessMenuAction( p,item->action );
		return NULL;
	}

	if ( key == K_RIGHTARROW )
	{
		if ( item )
			CONSOLE_SlideItem( p,1,item );
		return NULL;
	}

	if ( key == K_LEFTARROW )
	{
		if ( item )
			CONSOLE_SlideItem( p,-1,item );
		return NULL;
	}

	if ( ( key == K_UPARROW ) ||
	     ( ( key == 'p' ) && keydown[K_CTRL] ) )
	{
		CONSOLE_SelectItem( p,-1 );
	}

	if ( ( key == K_DOWNARROW ) ||
	     ( ( key == 'n' ) && keydown[K_CTRL] ) )
	{
		CONSOLE_SelectItem( p,1 );
	}
	return NULL;
}

//=============
//MENU_Bind_Key_f
//
//Input handler for binding keys
//=============

extern cvar_t   *k_doubleclicktime;

const char *MENU_Bind_Key_f( menubind3d_t *b, int key, unsigned time )
{
	static int last_key=0;
	static unsigned last_time=UINT_MAX;
	static char last_command[128];

	// grab is active, so get a single key click, we also need
	// to save this key's previous binding in case a double comes in
	// later, we will have to restore it back.
	if ( b->bind_grab_active )
	{
		if ( key != K_ESCAPE && key != '`' )
		{
			char cmd[1024];

			// Save off command and key in case we need to restore it
			if ( keybindings[key] && strlen( keybindings[key] ) )
				strcpy( last_command, keybindings[key] );
			else
				strcpy( last_command, b->command );

			last_key = key;
			last_time = time;

			Com_sprintf ( cmd, sizeof(cmd), "bind %s \"%s\"\n", Key_KeynumToString( key ), b->command );
			Cbuf_InsertText ( cmd );
		}



		b->bind_grab_active = false;
		return NULL;
	}

	// If this is a doublebind, single bind the last key to the saved command.
	if ( ( key == last_key ) && ( ( time - last_time ) < k_doubleclicktime->value ) )
	{
		char cmd[1024];
		Com_sprintf ( cmd, sizeof(cmd), "doublebind %s \"%s\"\n", Key_KeynumToString( key ), b->command );
		Cbuf_InsertText ( cmd );


		if ( strlen( last_command ) )
		{
			if ( !strcmp( b->command, last_command ) )
			{
				// If the last command is the same as this one, unbind this key
				Com_sprintf ( cmd, sizeof(cmd), "unbind %s\n", Key_KeynumToString( key ) );
			}
			else
			{
				// otherwise restore the last command
				Com_sprintf ( cmd, sizeof(cmd), "bind %s \"%s\"\n", Key_KeynumToString( key ), last_command );
			}
		}

		Cbuf_InsertText ( cmd );
		last_key = 0;
		last_time = UINT_MAX;
		last_command[0] = 0;
	}

	switch ( key )
	{
	case K_ENTER:
		MENU_KeyBindingFunc( b );
		break;
	case K_BACKSPACE:           // delete all bindings
	case K_DEL:
		MENU_UnbindCommand( b->command );
		break;
	default:
		return MENU_Default_Key_f( key );
	}
	return NULL;
}

//===============
//MENU_Quit_Key_f
//
//Quit menu input handler
//===============
static const char *MENU_Quit_Key_f( int key )
{
	switch (key) {
	case K_ESCAPE:
	case 'n':
	case 'N':
		CONSOLE_PopMenu( cls.mainconsole );
		break;

	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f ();
		break;

	default:
		break;
	}
	return NULL;
}


//============================
//MENU_Inventory_Key_f
//
//Inventory menu input handler
//============================
static const char *MENU_Inventory_Key_f( int key )
{
	switch (key)
	{
	case K_ESCAPE:
	{
		qboolean ret;
		ret = CONSOLE_PopMenu( cls.currentconsole );
		if (!ret)
		{
			CL_RandomLocalSound( menu_out_sound );
			CONSOLE_Exit( cls.currentconsole );
			inventory_active = false;
		}
		break;
	}
	case K_RIGHTARROW:
		cl_inventory_scale = 0;
		Cbuf_InsertText( "invnext\n" );
		break;
	case K_LEFTARROW:
		cl_inventory_scale = 0;
		Cbuf_InsertText( "invprev\n" );
		break;
	case K_UPARROW:
		cl_inventory_scale+=0.1f;
		break;
	case K_DOWNARROW:
		cl_inventory_scale-=0.1f;
		break;
	default:
		break;
	}

	if ( cl_inventory_scale < -0.3 )
		cl_inventory_scale = -0.3;
	else if ( cl_inventory_scale > 3 )
		cl_inventory_scale = 3;

	return NULL;
}


//===============
//MENU_Main_Key_f
//
//Main menu input handler, adds menu movement to left and right arrows
//===============
static const char *MENU_Main_Key_f( int key )
{
	console3d_t *p = cls.currentconsole;
	item3d_t    *item = p->menu3d[p->current_menu_level]->item[p->sel_menu_item];

	if ( item && ( item->type == FIELD3D ) )
	{
		if ( MENU_Field_Key_f( &item->field, key ) )
			return NULL;
	}

	if ( key == K_ESCAPE )
	{
		qboolean ret;
		ret = CONSOLE_PopMenu( p );
		if ( !ret )
		{
			CL_RandomLocalSound( menu_out_sound );
			CONSOLE_Exit( p );
		}
		return NULL;
	}

	if ( key == K_ENTER )
	{
		if ( item )
			CONSOLE_ProcessMenuAction( p, item->action );

		if ( item->type == FIELD3D )
		{
			CONSOLE_SelectItem( p, 1 );
		}

		return NULL;
	}

	if ( key == K_RIGHTARROW )
	{
		CONSOLE_SelectItem( p, 1 );
		return NULL;
	}

	if ( key == K_LEFTARROW )
	{
		CONSOLE_SelectItem( p, -1 );
		return NULL;
	}

	if ( ( key == K_UPARROW ) ||
	     ( ( key == 'p' ) && keydown[ K_CTRL ] ) )
	{
		CONSOLE_SelectItem( p, -1 );
		return NULL;
	}

	if ( ( key == K_DOWNARROW ) ||
	     ( ( key == 'n' ) && keydown[K_CTRL] ) )
	{
		CONSOLE_SelectItem( p, 1 );
		return NULL;
	}
	return NULL;
}

//====================
//MENU_No_Escape_Key_f
//====================
static const char *MENU_No_Escape_Key_f( int key )
{
	console3d_t *p = cls.currentconsole;
	item3d_t    *item = p->menu3d[p->current_menu_level]->item[p->sel_menu_item];

	if (item && item->type == FIELD3D)
	{
		if (MENU_Field_Key_f( &item->field, key ))
			return NULL;
	}

	if ( key == K_ENTER )
	{
		if ( item )
			CONSOLE_ProcessMenuAction( p,item->action );
		return NULL;
	}

	if ( key == K_RIGHTARROW )
	{
		CONSOLE_SelectItem( p,1 );
		return NULL;
	}

	if ( key == K_LEFTARROW )
	{
		CONSOLE_SelectItem( p,-1 );
		return NULL;
	}

	if ( ( key == K_UPARROW ) ||
	     ( ( key == 'p' ) && keydown[K_CTRL] ) )
	{
		CONSOLE_SelectItem( p,-1 );
		return NULL;
	}

	if ( ( key == K_DOWNARROW ) ||
	     ( ( key == 'n' ) && keydown[K_CTRL] ) )
	{
		CONSOLE_SelectItem( p,1 );
		return NULL;
	}
	return NULL;
}


//========================
//MENU_No_Escape_Any_Key_f
//========================
static const char *MENU_No_Escape_Any_Key_f( int key )
{
	console3d_t *p = cls.currentconsole;
	item3d_t    *item = p->menu3d[p->current_menu_level]->item[p->sel_menu_item];

	if ( item )
		CONSOLE_ProcessMenuAction( p,item->action );
	return NULL;
}

//======================
//MENU_Credits_No_Keys_f
//======================
static const char *MENU_Credits_No_Keys_f( int key )
{
	if ( key == K_ESCAPE )
		MENU_Main_f();
	return NULL;
}

//==============
//MENU_Credits_f
//==============
static const char *MENU_Credits_f( int key )
{
	console3d_t *p = cls.currentconsole;
	item3d_t    *item = p->menu3d[p->current_menu_level]->item[p->sel_menu_item];

	if ( key == K_ESCAPE )
		MENU_Main_f();
	else if ( item )
		CONSOLE_ProcessMenuAction( p,item->action );
	return NULL;
}

//================
//MENU_Field_Key_f
//
//Menu field input handler
//================
qboolean MENU_Field_Key_f( menufield3d_t *f, int key )
{

	if ( key > 127 )
	{
		switch ( key )
		{
		case K_DEL:
		default:
			return false;
		}
	}

	switch ( key )
	{
	case K_LEFTARROW:
	case K_BACKSPACE:
		if ( f->cursor > 0 )
		{
			memmove( &f->buffer[f->cursor-1], &f->buffer[f->cursor], strlen( &f->buffer[f->cursor] ) + 1 );
			f->cursor--;

			if ( f->visible_offset )
			{
				f->visible_offset--;
			}
		}
		break;

	case K_DEL:
		memmove( &f->buffer[f->cursor], &f->buffer[f->cursor+1], strlen( &f->buffer[f->cursor+1] ) + 1 );
		break;

	case K_ENTER:
	case K_ESCAPE:
	case K_TAB:
		return false;

	case K_SPACE:
	default:
		if ( !isdigit( key ) && ( f->flags & M3DF_NUMBERSONLY ) )
			return false;

		if ( f->cursor < f->length )
		{
			f->buffer[f->cursor++] = key;
			f->buffer[f->cursor] = 0;

			if ( f->cursor > f->visible_length )
			{
				f->visible_offset++;
			}
		}
	}
	return true;
}

//========================================================
// MULTIPLAYER MENUS
//========================================================

static void SearchInternetGames( void )
{
	int i;

	// Clear out server list
	m_num_servers = 0;
	for (i=0; i<MAX_LOCAL_SERVERS; i++)
	{
		strcpy ( local_server_names[i], NO_SERVER_STRING );
	}

	// Draw some sort of box here to display a waiting message.

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	// Get the info from Gamespy
	CL_RefreshServerList();
}

//================
//SearchLocalGames
//================
static void SearchLocalGames( void )
{
	int i;

	m_num_servers = 0;
	for (i=0; i<MAX_LOCAL_SERVERS; i++)
		strcpy ( local_server_names[i], NO_SERVER_STRING );

	// Draw some sort of box here to display a waiting message.

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	// send out info packets
	CL_PingServers_f();
}


#if defined(__GAMERANGER__)

//=====================
//MENU_PlayOnGameRanger
//=====================
static void MENU_PlayOnGameRanger( void *unused )
{
	LaunchingGameRanger = true;
	MENU_Main_f();
	CL_Quit_f();
}
#endif


//=====================
//MENU_SearchLocalGames
//=====================
static void MENU_SearchLocalGames( void *unused )
{
	SearchLocalGames();
}

//=====================
//MENU_SearchLocalGames
//=====================
static void MENU_SearchInternetGames( void *unused )
{
	SearchInternetGames();
}

//====================
//MENU_AddToServerList
//====================
void MENU_AddToServerList( netadr_t adr, const char *info )
{
	int i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
		return;
	while ( *info == ' ' )
		info++;

	// ignore if duplicated
	for (i=0; i<m_num_servers; i++)
		if (!strcmp( info, local_server_names[i] ))
			return;

	local_server_netadr[m_num_servers] = adr;
	strncpy ( local_server_names[m_num_servers], info, sizeof(local_server_names[0])-1 );
	m_num_servers++;
}

//===============
//MENU_JoinServer
//===============
static void MENU_JoinServer( void *self )
{
	char buffer[128];
	int index;

	item3d_t *item = (item3d_t *)self;

	if (item->localdata)
		index = atoi( item->localdata );
	else
		return;

	if ( Q_stricmp( local_server_names[index], NO_SERVER_STRING ) == 0 )
		return;

	if (index >= m_num_servers)
		return;

	Com_sprintf ( buffer, sizeof(buffer), "connect %s\n", NET_AdrToString ( local_server_netadr[index] ));
	Cbuf_AddText ( buffer );
	CONSOLE_Exit( cls.mainconsole );
}

//===========================
//MENU_StartMultiplayerServer
//===========================
static void MENU_StartMultiplayerServer( void *unused )
{
	char startmap[1024];
	console3d_t *p;
	// 2015 code
	int dmvalue;

	p = cls.mainconsole;

	Cvar_Set( "hostname",   MENU_GetStringForName( "hostname",p ));
	Cvar_Set( "maxclients", MENU_GetStringForName( "maxclients",p ));
	Cvar_Set( "timelimit",  MENU_GetStringForName( "timelimit",p ));
	Cvar_Set( "fraglimit",  MENU_GetStringForName( "fraglimit",p ));
	// 2015 code
	// now set by the menu file
	dmvalue = (int)Cvar_VariableValue( "deathmatch" );
	if(!dmvalue) // make sure that deathmatch is actually set
		Cvar_Set( "deathmatch", "1" );

	// Get the map from the menu
	strcpy( startmap, MENU_GetStringForName( "con_map",p ));
	Cbuf_AddText ( va( "map %s", startmap ));
	CONSOLE_Exit( cls.mainconsole );
}

//=========================
//MENU_MultiPlayerSetupInit
//=========================

// 2015 code

static void MENU_MultiPlayerSetupInit( void )
{
	int i,j,k;
	int index;
	console3d_t    *p=cls.mainconsole;
	char           *list_name;

	index = 0;

	for ( i=0; i<MAX_PLAYERMODELS; i++ )
	{
		if ( s_pmnames[ i ] )
		{
			if ( !strncmp( model->string, s_pmnames[ i ], strlen( model->string ) ) )
			{
				index = i;
				Cvar_Set( "con_playermodel", va( "%i",i ) );
				break;
			}
		}
	}

	// Based on the model, set the appropriate skinlist
	// Find the menu item that we need to update with the skins information.
	for (i=0; i<p->numlevels; i++)
	{
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			list_name = p->menu3d[i]->item[j]->list.name;

			if ((p->menu3d[i]->item[j]->type == LIST3D) &&
			    (!strcmp( "con_playerskin",list_name )))
			{
				p->menu3d[i]->item[j]->list.itemnames = s_pmi[index].SkinDisplayNamesPtr;
				p->menu3d[i]->item[j]->list.numitems  = s_pmi[index].nskins;
				p->menu3d[i]->item[j]->list.curvalue  = 0;

				// Set the skin based on the current value
				for (k=0; k<p->menu3d[i]->item[j]->list.numitems; k++)
				{
					if (!strcmp( skin->string, p->menu3d[i]->item[j]->list.itemnames[k] ))
					{
						p->menu3d[i]->item[j]->list.curvalue = k;
						Cvar_Set( "con_playerskin", va( "%i",k ));
						return;
					}
				}
				// Couldn't find the skin, setting it to the 0th index
				p->menu3d[i]->item[j]->list.curvalue = 0;
				Cvar_Set( "con_playerskin", "0" );
				return;
			}
		}
	}
}

// 2015 code
static void MENU_MultiPlayerSetupBikeInit( void )
{
	int i,j,k;
	int index;
	console3d_t    *p=cls.mainconsole;
	char           *list_name;

	index = 0;

	for ( i=0; i<MAX_BIKEMODELS; i++ )
	{
		if ( s_bikemnames[ i ] )
		{
			if(!strncmp( bikemodel->string, s_bikemnames[i], strlen( bikemodel->string )))
			{
				index = i;
				Cvar_Set( "con_bikemodel", va( "%i",i ) );
				break;
			}
		}
	}

	// Based on the model, set the appropriate skinlist
	// Find the menu item that we need to update with the skins information.
	for (i=0; i<p->numlevels; i++)
	{
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			list_name = p->menu3d[i]->item[j]->list.name;

			if ((p->menu3d[i]->item[j]->type == LIST3D) &&
			    (!strcmp( "con_bikeskin",list_name )))
			{
				p->menu3d[i]->item[j]->list.itemnames = s_bikemi[index].skindisplaynames;
				p->menu3d[i]->item[j]->list.numitems  = s_bikemi[index].nskins;
				p->menu3d[i]->item[j]->list.curvalue  = 0;

				// Set the skin based on the current value
				for (k=0; k<p->menu3d[i]->item[j]->list.numitems; k++)
				{
					if (!strcmp( bikeskin->string, p->menu3d[i]->item[j]->list.itemnames[k] ))
					{
						p->menu3d[i]->item[j]->list.curvalue = k;
						Cvar_Set( "con_bikeskin", va( "%i",k ));
						return;
					}
				}
				// Couldn't find the skin, setting it to the 0th index
				p->menu3d[i]->item[j]->list.curvalue = 0;
				Cvar_Set( "con_bikeskin", "0" );
				return;
			}
		}
	}
}


//=================================
//MENU_MultiPlayerSetupApplyChanges
//=================================
static void MENU_MultiPlayerSetupApplyChanges( void *unused )
{
	const char *newskin;
	const char *newmodel;
	int modelindex = Cvar_VariableValue( "con_playermodel" );
	int skinindex  = Cvar_VariableValue( "con_playerskin" );

	if((s_pmnames[modelindex]) && (s_pmi[ modelindex ].SkinDisplayNamesPtr[skinindex]))
	{
		newmodel = s_pmnames[ modelindex ];
		newskin  = s_pmi[ modelindex ].SkinDisplayNamesPtr[ skinindex ];

		Cvar_Set( "model", newmodel );
		Cvar_Set( "skin", newskin );
	}

	if ( game2015->value )
	{
		modelindex = Cvar_VariableValue( "con_bikemodel" );
		skinindex  = Cvar_VariableValue( "con_bikeskin" );

		if ((s_bikemnames[modelindex])
			&& (s_bikemi[modelindex].skindisplaynames)
			&& (s_bikemi[modelindex].skindisplaynames[skinindex]))
		{
			newmodel = s_bikemnames[modelindex];
			newskin  = s_bikemi[modelindex].skindisplaynames[skinindex];

			Cvar_Set( "bikemodel", newmodel );
			Cvar_Set( "bikeskin", newskin );
		}
	}

}

//====================
//MENU_MultiPlayerInit
//====================
static void MENU_MultiPlayerInit( void )
{
	console3d_t *p;
	int dmflags = (int)Cvar_VariableValue( "dmflags" );

	p = cls.mainconsole;

	MENU_UpdateValueForName( "df_falldam",((dmflags & DF_NO_FALLING)==0),p );
	MENU_UpdateValueForName( "df_weapstay",((dmflags & DF_WEAPONS_STAY)!=0),p );
	MENU_UpdateValueForName( "df_weapswitch",((dmflags & DF_FAST_WEAPONS)!=0),p );
	MENU_UpdateValueForName( "df_allowhealth",((dmflags & DF_NO_HEALTH)==0),p );
	MENU_UpdateValueForName( "df_allowarmor",((dmflags & DF_NO_ARMOR)==0),p );
	MENU_UpdateValueForName( "df_foreresp",((dmflags & DF_FORCE_RESPAWN)!=0),p );
	MENU_UpdateValueForName( "df_allowpowerups",((dmflags & DF_NO_POWERUPS)==0),p );
	MENU_UpdateValueForName( "df_instpowerups",((dmflags & DF_INSTANT_ITEMS)!=0),p );
	MENU_UpdateValueForName( "df_infiniteammo",((dmflags & DF_INFINITE_AMMO)!=0),p );
	MENU_UpdateValueForName( "df_spawnfar",((dmflags & DF_SPAWN_FARTHEST)!=0),p );
	MENU_UpdateValueForName( "df_fiendlyfire",((dmflags & DF_FRIENDLY_FIRE)!=0),p );
	MENU_UpdateValueForName( "df_fixedfov",((dmflags & DF_FIXED_FOV)!=0),p );
	MENU_UpdateValueForName( "df_samemap",((dmflags & DF_SAME_LEVEL)!=0),p );
	MENU_UpdateValueForName( "df_allowexit",((dmflags & DF_NOEXIT)!=0),p );
	MENU_UpdateValueForName( "df_weapondrop",((dmflags & DF_NO_DROP_WEAPONS)!=0),p );
	MENU_UpdateValueForName( "df_teamplay",0,p );
	if ( dmflags & DF_SKINTEAMS )
		MENU_UpdateValueForName( "df_teamplay",1,p );
	else if ( dmflags & DF_MODELTEAMS )
		MENU_UpdateValueForName( "df_teamplay",2,p );
	MENU_UpdateValueForName( "df_noweaponchange",((dmflags & DF_NO_WEAPON_CHANGE)==0),p );
}

//============================================================
// VIDEO MENU
//============================================================

//======================
//MENU_ApplyVideoChanges
//======================
static void MENU_ApplyVideoChanges( void *unused )
{
	int old_palettedtexture;
	int new_palettedtexture;
	float gamma;

	gamma = ( 0.8 - ( Cvar_VariableValue( "con_brightness" )/10.0 - 0.5 ) ) + 0.5;

	Cvar_SetValue( "vid_gamma", gamma );
	Cvar_SetValue( "vid_fullscreen", Cvar_VariableValue( "con_fullscreen" ));

	switch ( (int)Cvar_VariableValue( "con_driver" ) )
	{
	case REF_SOFT:
		old_palettedtexture = Cvar_VariableValue( "r_8bpp" );
		Cvar_SetValue( "r_8bpp", Cvar_VariableValue( "con_texmode" ));
		new_palettedtexture = Cvar_VariableValue( "r_8bpp" );

		if ((old_palettedtexture != new_palettedtexture)
			&& (( new_palettedtexture == 0 ) || ( old_palettedtexture == 0 )) )
		{
			vid_ref->modified = true;
		}
		Cvar_SetValue( "sw_mode", Cvar_VariableValue( "con_vidmode" ));
		Cvar_Set( "vid_ref", "soft" );
		break;
	case REF_OPENGL:
		old_palettedtexture = Cvar_VariableValue( "gl_ext_palettedtexture" );
		Cvar_SetValue( "gl_ext_palettedtexture", Cvar_VariableValue( "con_texmode" ));
		new_palettedtexture = Cvar_VariableValue( "gl_ext_palettedtexture" );

		Cvar_SetValue( "gl_mode", Cvar_VariableValue( "con_vidmode" ));
		Cvar_Set( "vid_ref", "gl" );
		#ifdef _WIN32
		Cvar_Set( "gl_driver", "opengl32" );
		#endif
		break;
	#ifdef _WIN32
	/*case REF_3DFX:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "3dfxgl" );
		break;
	case REF_POWERVR:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "pvrgl" );
		break;
	case REF_VERITE:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "opengl32" );
		break;
	case REF_RIVA:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "opengl32" );
		break;
	case REF_ATIRAGEPRO:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "opengl32" );
		break;
	case REF_I740:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "opengl32" );
		break; */
	#endif
	}

	/*
	** update appropriate stuff if we're running OpenGL and gamma
	** has been modified
	*/
	if ( Q_stricmp( vid_ref->string, "gl" ) == 0 )
	{
		if ( vid_gamma->modified )
		{
			vid_ref->modified = true;
			#ifdef _WIN32
			/*if (Q_stricmp( gl_driver->string, "3dfxgl", 6 ) == 0)
			{
				char envbuffer[1024];
				float g;

				vid_ref->modified = true;

				g = 2.00 * (0.8 - (vid_gamma->value - 0.5)) + 1.0F;
				Com_sprintf(envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g);
				putenv(envbuffer);
				Com_sprintf(envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g);
				putenv(envbuffer);

				vid_gamma->modified = false;
			}*/
			#endif
		}
		if ( gl_driver->modified ) {
			vid_ref->modified = true;
		}
	}

	// Force the menus to update.
	con_vidres = CON_NORES;
}

static void MENU_VideoResolutionGrab (console3d_t *p) /* FS: Don't rely on the menu.mnu for this */
{
	int nitems = 0;
	int i = 0;
	int j = 0;
	int level = -1;
	int item = -1;

	if (!p)
	{
		return;
	}

	for (i=0; i<p->numlevels; i++)
	{
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i] || !p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			if (p->menu3d[i]->item[j]->type == LIST3D)
			{
				menulist3d_t *list = &p->menu3d[i]->item[j]->list;

				if (!strcmp( "con_vidmode",list->name ))
				{
					level = i;
					item = j;
					break;
				}
			}
		}
	}

	if ((level < 0) || (item < 0))
	{
		return;
	}

	nitems = p->menu3d[level]->item[item]->list.numitems;

	for (i=0; i<nitems; i++)
	{
		if(p->menu3d[level]->item[item]->list.itemnames[i])
		{
			Z_Free((void *)p->menu3d[level]->item[item]->list.itemnames[i]);
			p->menu3d[level]->item[item]->list.itemnames[i] = NULL;
		}
	}

	Z_Free((void *)p->menu3d[level]->item[item]->list.itemnames);
	Z_Free((void *)p->menu3d[level]->item[item]->list.itemactions);

	p->menu3d[level]->item[item]->list.numitems = nitems = VID_GetModeCount();
	p->menu3d[level]->item[item]->list.itemactions = (char **)Z_TagMalloc( sizeof(char *) * nitems,TAG_CONSOLE_BASE+p->console_number );
	p->menu3d[level]->item[item]->list.itemnames = (char **)Z_TagMalloc( sizeof(char *) * nitems,TAG_CONSOLE_BASE+p->console_number );

	Z_Free((void *)p->menu3d[level]->item[item]->sel_menu_layout);
	Z_Free((void *)p->menu3d[level]->item[item]->menu_layout);

	if (viddef.width < 640)
	{
		p->menu3d[level]->item[item]->sel_menu_layout = CONSOLE_strdup("fc 1 1 1 1 oxl 40 oyt -130 cursor string \"Video Mode: \" oxl 144 oyt -130 drawlist \"numchars 18 red 0.25 green 0.25 blue 0.56\"", p);
		p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup("fc 0.7 0.7 0.7 0.7 oxl 40 oyt -130 string \"Video Mode: \" oxl 144 oyt -130 drawlist \"numchars 18 red 0.2 green 0.2 blue 0.2\"", p);
	}
	else
	{
		p->menu3d[level]->item[item]->sel_menu_layout = CONSOLE_strdup("fc 1 1 1 1 oxl 150 oyt -300 cursor string \"Video Mode: \" oxl 308 oyt -300 drawlist \"numchars 20 red 0.25 green 0.25 blue 0.56\"", p);
		p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup("fc 0.7 0.7 0.7 0.7 oxl 150 oyt -300 string \"Video Mode: \" oxl 308 oyt -300 drawlist \"numchars 20 red 0.2 green 0.2 blue 0.2\"", p);
	}

	for (i=0; i<nitems; i++)
	{
		p->menu3d[level]->item[item]->list.itemnames[i] = CONSOLE_strdup(VID_GetModeDescription(i), p);
	}
}

//==============
//MENU_VideoInit
//
//This initializes the video menu
//==============
static void MENU_VideoInit( void *unused )
{
	console3d_t *p;
	p = cls.mainconsole;

	MENU_VideoResolutionGrab(p); /* FS: Don't rely on the menu.mnu for this */

	if ( !gl_driver )
#ifdef SDL_CLIENT /* to load SDL's default opengl driver -- see qgl_linux.c */
		gl_driver = Cvar_Get( "gl_driver", "", 0 );
#elif defined(_WIN32)
		gl_driver = Cvar_Get( "gl_driver", "opengl32", 0 );
#elif defined(__DJGPP__) /* FS: Don't clash with win32's opengl32 */
		gl_driver = Cvar_Get( "gl_dosdriver", "gl.dxe", 0);
#else /* assume unixish */
		gl_driver = Cvar_Get( "gl_driver", "libGL.so.1", 0 );
#endif
	if ( !gl_picmip )
		gl_picmip = Cvar_Get( "gl_picmip", "0", 0 );
	if ( !gl_finish )
		gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE );
	if ( !gl_mode )
		gl_mode = Cvar_Get( "gl_mode", "0", 0 );
	if ( !sw_mode ) {
		sw_mode = Cvar_Get( "sw_mode", "0", 0 );
	}
	if ( !gl_ext_palettedtexture )
		gl_ext_palettedtexture = Cvar_Get( "gl_ext_palettedtexture", "2", CVAR_ARCHIVE );
	if ( !sw_stipplealpha )
		sw_stipplealpha = Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	if ( !scr_viewsize )
		scr_viewsize = Cvar_Get ( "viewsize", "100", CVAR_ARCHIVE );

	MENU_UpdateValueForName( "con_fullscreen",Cvar_VariableValue( "vid_fullscreen" ),p );
	MENU_UpdateValueForName( "con_screensize",Cvar_VariableValue( "viewsize" )/10,p );
	MENU_UpdateValueForName( "con_brightness",( 1.3 - vid_gamma->value + 0.5 ) * 10,p );

	if (!strcmp( vid_ref->string, "soft" ))
	{
		int swmode;
		swmode = Cvar_VariableValue( "sw_mode" );
		MENU_UpdateValueForName( "con_driver",REF_SOFT,p );
		MENU_UpdateValueForName( "con_vidmode",swmode,p );
		MENU_UpdateValueForName( "con_texmode",Cvar_VariableValue( "r_8bpp" ),p );
	}
	else if (!strcmp( vid_ref->string,"gl" ))
	{
		MENU_UpdateValueForName( "con_vidmode",Cvar_VariableValue( "gl_mode" ),p );
		MENU_UpdateValueForName( "con_texmode",Cvar_VariableValue( "gl_ext_palettedtexture" ),p );
		#ifdef _WIN32
		/*if (!strcmp(gl_driver->string,"3dfxgl"))
			MENU_UpdateValueForName("con_driver",REF_3DFX,p);
		else if (!strcmp(gl_driver->string,"pvrgl"))
			MENU_UpdateValueForName("con_driver",REF_POWERVR,p);
		else if (!strcmp(gl_driver->string,"opengl32"))
			MENU_UpdateValueForName("con_driver",REF_OPENGL,p);
		else */
		#endif
			MENU_UpdateValueForName( "con_driver",REF_OPENGL,p );
	}
}

//============
//VID_MenuInit
//
//This function inits the video cvars
//============
void VID_MenuInit( void )
{
	if ( !gl_driver ) {
#ifdef SDL_CLIENT /* to load SDL's default opengl driver -- see qgl_linux.c */
		gl_driver = Cvar_Get( "gl_driver", "", 0 );
#elif defined(_WIN32)
		gl_driver = Cvar_Get( "gl_driver", "opengl32", 0 );
#elif defined(__DJGPP__) /* FS: Don't clash with win32's opengl32 */
		gl_driver = Cvar_Get( "gl_dosdriver", "gl.dxe", 0);
#else /* assuming unix */
		gl_driver = Cvar_Get( "gl_driver", "libGL.so.1", 0 );
#endif
	}
	if ( !gl_picmip ) {
		gl_picmip = Cvar_Get( "gl_picmip", "0", 0 );
	}
	if ( !gl_finish ) {
		gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE );
	}
	if ( !gl_mode ) {
		gl_mode = Cvar_Get( "gl_mode", "0", 0 );
	}
	if ( !sw_mode ) {
		sw_mode = Cvar_Get( "sw_mode", "0", 0 );
	}
	if ( !gl_ext_palettedtexture ) {
		gl_ext_palettedtexture = Cvar_Get( "gl_ext_palettedtexture", "2", CVAR_ARCHIVE );
	}
	if ( !sw_stipplealpha ) {
		sw_stipplealpha = Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	}
	if ( !scr_viewsize ) {
		scr_viewsize = Cvar_Get ( "viewsize", "100", CVAR_ARCHIVE );
	}
}

//=============
//MENU_SetGamma
//=============
static void MENU_SetGamma( void *unused )
{
	if ( Q_stricmp( vid_ref->string, "soft" ) == 0 )
	{
		float gamma = ( 0.8 - ( Cvar_VariableValue( "con_brightness" )/10.0 - 0.5 ) ) + 0.5;
		Cvar_SetValue( "vid_gamma", gamma );
	}
}

//==============================================================
// DEATHMATCH STUFF
//==============================================================

int s_numdeathmatchmaps = 0;
char **s_deathmatchmaps = NULL;

static void MENU_ReleaseDeathmatchMapsConfig( void )
{
	if ( s_deathmatchmaps && s_numdeathmatchmaps ) {
		int i;
		for ( i = 0; i < s_numdeathmatchmaps; i++ ) {
			free( s_deathmatchmaps[ i ] );
		}
		free( s_deathmatchmaps );
	}
	s_numdeathmatchmaps = 0;
}


qboolean MENU_InitDeathmatchMapsConfig( void )
{
	char scratch[1024];
	int i,count;
	const char  **mapmenunames;
	int FileListSize;

	MENU_ReleaseDeathmatchMapsConfig();

	mapmenunames = FS_FindFiles( "maps/", "*.mnu", &FileListSize );

	if ( !mapmenunames )
		return false;

	s_numdeathmatchmaps = FileListSize;
	if ( s_numdeathmatchmaps > MAX_DEATHMATCH_MAPS ) {
		s_numdeathmatchmaps = MAX_DEATHMATCH_MAPS;
	}
	s_deathmatchmaps = (char **) calloc( 1, (sizeof( char * ) * ( s_numdeathmatchmaps + 1 )));
	count = 0;
	for ( i = 0; i < s_numdeathmatchmaps; i++ ) {
		// Skip the alt menus
		if ( strstr( mapmenunames[ i ], "_alt.mnu" ) )
			continue;
		COM_StripExtension( COM_SkipPath( mapmenunames[ i ] ), scratch );
		s_deathmatchmaps[ count++ ] = strdup( scratch );
	}
	s_numdeathmatchmaps = count;
	FS_FreeList( mapmenunames,FileListSize );
	return true;
}

//==============================================================
// LOAD AND SAVE GAMES
//==============================================================

//======================
//MENU_CreateSavestrings
//======================

static void MENU_CreateSavestrings( void )
{
	int i;
	FILE *f;
	char name[MAX_OSPATH];

	for (i=0; i<MAX_SAVEGAMES; i++) {
		Com_sprintf ( name,sizeof(name), "%s/save/save%i/server.ssv", FS_Playerdir(), i );
		f = fopen( name,"rb" );
		if (!f) {
			strcpy ( m_savestrings[i], "<EMPTY>" );
			m_savevalid[i] = false;
		} else {
			fread( m_savestrings[i], 1, sizeof(m_savestrings[i]), f );
			m_savestrings[i][31] = 0;
			fclose ( f );
			m_savevalid[i] = true;
		}
	}
}

//=============
//MENU_SaveGame
//=============
static void MENU_SaveGame( void *self )
{
	item3d_t *item = (item3d_t *)self;

	assert ( item );

	if ( item->localdata )
	{
		Cbuf_AddText ( va( "save save%i\n",atoi( item->localdata )));
	}

	CONSOLE_Exit( cls.mainconsole );
}

//=============
//MENU_LoadGame
//=============
static void MENU_LoadGame( void *self )
{
	item3d_t *item = (item3d_t *)self;

	assert ( item );

	if ( item->localdata )
	{
		if ( m_savevalid[ atoi( item->localdata ) ] )
		{
			Cbuf_AddText( va( "load save%i\n",atoi( item->localdata )));
		}
	}

	CONSOLE_Exit( cls.mainconsole );
}

//===============
//MENU_LoadPlayer
//===============
static void MENU_LoadPlayer( void *self )
{
	char path[MAX_QPATH];

	item3d_t *item = (item3d_t *)self;
	Cvar_Set ( "player", item->field.buffer );
	FS_InitPlayerdir();
	Com_sprintf ( path, sizeof(path),"%s/%s",FS_RelativePlayerdir(), cfg_default->string );
	Cbuf_AddText ( "exec default.cfg\n" );
	Cbuf_AddText ( va( "\nexec %s\n vid_restart\n", path ) );
	CONSOLE_Exit( cls.mainconsole );
}

//============
//MENU_Relogon
//============
static void MENU_Relogon( void *unused )
{
	char cfgName[MAX_QPATH];

	COM_StripExtension( cfg_default->string, cfgName );
	CL_WriteConfiguration( cfgName );

	MENU_ForceLogon_f();
}

menu_func_t functions[] = {
	{"apply_vid_changes",  MENU_ApplyVideoChanges},
	{"apply_player_setup", MENU_MultiPlayerSetupApplyChanges},
	{"vid_reset",          MENU_VideoInit},
	{"setgamma",           MENU_SetGamma},
	{"startserver",        MENU_StartMultiplayerServer},
	{"refreshservers",     MENU_SearchLocalGames},
	{"refreshnetservers",  MENU_SearchInternetGames},
	{"joinserver",         MENU_JoinServer},
	{"update_skinlist",    MENU_UpdateSkinList},
	{"update_bikeskinlist",MENU_UpdateBikeSkinList},
	{"loadgame",           MENU_LoadGame},
	{"savegame",           MENU_SaveGame},
	{"loadplayer",         MENU_LoadPlayer},
	{"relogon",            MENU_Relogon},
	#if defined(__GAMERANGER__)
	{"playongameranger",   MENU_PlayOnGameRanger},
	#endif
	{NULL, NULL}
};

key_func_t key_functions[] = {
	{"quit",           MENU_Quit_Key_f},
	{"inventory",      MENU_Inventory_Key_f},
	{"main",           MENU_Main_Key_f},
	{"game_selection", MENU_Main_Key_f},
	{"multiplayer",    MENU_Main_Key_f},
	{"controls",       MENU_Main_Key_f},
	{"logon",          MENU_No_Escape_Key_f},
	{"sin",            MENU_No_Escape_Any_Key_f},
	{"ritual",         MENU_No_Escape_Any_Key_f},
	{"anykey",         MENU_No_Escape_Any_Key_f},
	{"nopress",        MENU_Credits_No_Keys_f},
	{"credit",         MENU_Credits_f},
	{NULL, NULL}
};


//==============
//MENU_Generic_f
//==============
static void MENU_Generic_f( void )
{
	if ( !cls.genconsole || !cls.genconsole->menu3d )
	{
		return;
	}

	cls.currentconsole = cls.genconsole;
	cls.key_dest = key_menu3d;
	CONSOLE_ResetTimer( cls.genconsole );
	CONSOLE_ResetTypeTimer( cls.genconsole );
	if (cls.genconsole->playsound)
		CL_RandomLocalSound( menu_in_sound );
	cls.genconsole->current_menu_level = 0;
	cls.genconsole->sel_menu_item = 0;
	CONSOLE_SkipHeader( cls.genconsole );
	CONSOLE_SetKeyFunction( cls.genconsole );
	CONSOLE_ProcessMenuAction( cls.genconsole,cls.genconsole->menu3d[cls.genconsole->current_menu_level]->menuaction );
}

//===========
//MENU_Main_f
//===========
void MENU_Main_f( void )
{
#ifdef __USEA3D
	// turn off under water effect
	if (snd_isa3d)
	{
		hA3D_SetEnvironment ( A3D_ENV_AIR );
	}
#endif

	cls.currentconsole                  = cls.mainconsole;
	cls.key_dest                        = key_menu3d;
	cls.mainconsole->current_menu_level = 0;
	cls.mainconsole->sel_menu_item      = 0;
	cls.mainconsole->LayoutPtr[0] = 0;

	CONSOLE_ResetTimer( cls.mainconsole );
	MENU_VideoInit( NULL );
	MENU_MultiPlayerInit();
	MENU_UpdateBoundValues( cls.mainconsole );
	MENU_CreateSavestrings();
	MENU_MultiPlayerSetupInit();
	MENU_MultiPlayerSetupBikeInit();
	CONSOLE_SetKeyFunction( cls.mainconsole );
	if ( !cl_paused->value )
		CL_Pause_f();
	CL_RandomLocalSound( menu_in_sound );
	CONSOLE_SkipHeader( cls.mainconsole );
	if (!cls.mainconsole->menu3d)
		Sys_Error("cls.mainconsole->menu3d == NULL");
	CONSOLE_ProcessMenuAction( cls.mainconsole,cls.mainconsole->menu3d[cls.mainconsole->current_menu_level]->menuaction );
	SCR_EndLoadingPlaque();
}

//=============
//MENU_PushMenu
//=============
void MENU_PushMenu( const char *menu_name )
{
	int i;
	console3d_t *p = cls.mainconsole;

	if (!p)
		return;

	// Search for the menu name to get the level number
	for (i=0; i<p->numlevels; i++)
	{
		if (!p->menu3d[i]->menu_name)
			continue;
		if (!strcmp( menu_name,p->menu3d[i]->menu_name ))
		{
			CONSOLE_PushMenu( cls.mainconsole,i );
			break;
		}
	}
}

//===============
//MENU_LoadGame_f
//===============
static void MENU_LoadGame_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "loadgame" );
}

//===============
//MENU_SaveGame_f
//===============
static void MENU_SaveGame_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "savegame" );
}

//============
//MENU_Video_f
//============
static void MENU_Video_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "video" );
}

//================
//MENU_Adv_Video_f
//================
static void MENU_Adv_Video_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "advanced_video" );
}

//==================
//MENU_Multiplayer_f
//==================
static void MENU_Multiplayer_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "playersetup" );
}

//===========
//MENU_Keys_f
//===========
static void MENU_Keys_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "bindings" );
}

//============
//MENU_Logon_f
//============
static void MENU_Logon_f( void )
{
	MENU_Main_f();

	if (!((int)cl_skiplogon->value) && !Q_stricmp( "blade", Cvar_VariableString( "player" ))) /* FS: Skip this if we want to */
	{
		MENU_PushMenu( "logon" );
	}
}

//=================
//MENU_ForceLogon_f
//=================
static void MENU_ForceLogon_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "logon" );
}

//===========
//MENU_Quit_f
//===========
static void MENU_Quit_f( void )
{
	MENU_Main_f();
	MENU_PushMenu( "quit" );
}

//============
//MENU_Close_f
//============
static void MENU_Close_f( void )
{
	CONSOLE_Exit( cls.mainconsole );
}

//===========
//MENU_Load_f
//===========
static void MENU_Load_f( void )
{
	const char  *menu_name;
	char scratch[256];
	char temp[256];
	int c;

	c = Cmd_Argc();
	if (c == 2)
	{
		menu_name = Cmd_Argv( 1 );
		COM_StripExtension( menu_name, temp );
		menu_name = temp;

		if ((viddef.width < 640) && (viddef.height < 480))
		{
			sprintf( scratch,"menus/%s_alt", menu_name );
		}
		else
		{
			sprintf( scratch,"menus/%s", menu_name );
		}
		COM_DefaultExtension( scratch,".mnu" );
		CONSOLE_LoadMenu( va( "gencon %s",scratch ) );
	}
	else
	{
		Com_Printf( "usage: menu_load <menufile>\n" );
	}
}


//=======================
//MENU_AnyKeyToContinue_f
//=======================
void MENU_AnyKeyToContinue_f( void )
{
	if (strcmp( cl.configstrings[CS_STATUSBAR], "DEATHMATCH" ))
	{
		if ( cls.mainconsole )
		{
			DrawLayout3D( "jcx jcy picn anykey", cls.mainconsole, -1 );
			re.EndFrame();
		}

		Key_GetKey();
	}
	CL_Begin_f();
	CONSOLE_ClearIntermissionData_f();
	CONSOLE_BeginLevel_f();
	if (cls.key_dest == key_console)
	{
		Con_ToggleConsole_f();
	}
}

//================
//MENU_Inventory_f
//================
static void MENU_Inventory_f( void )
{
	if (cls.state != ca_active || !cl.refresh_prepped || cl_paused->value)
		return;

	if ( inventory_active )
	{
		CL_RandomLocalSound( menu_out_sound );
		CONSOLE_Exit( cls.currentconsole );
		inventory_active = false;
	}
	else
	{
		if ( con_vidres == CON_HIRES )
			CONSOLE_LoadMenu( "gencon menus/inven.mnu" );
		else
			CONSOLE_LoadMenu( "gencon menus/inven_alt.mnu" );

		MENU_Generic_f();
		inventory_active = true;
		cl_inventory_scale = 0;
	}
}

//===========
//MENU_Help_f
//===========
static void MENU_Help_f( void )
{
	if (cls.state != ca_active || !cl.refresh_prepped || cl_paused->value)
		return;

	help_active = ~help_active;
}

//===========
//MENU_Update_f
//===========
static void MENU_Update_f( void )
{
	MENU_UpdateBoundValues( cls.mainconsole );
}

//===========
//MENU_Init_f
//===========
void MENU_Init( void )
{
	MENU_VideoInit( NULL );
	MENU_MultiPlayerInit();
	MENU_UpdateBoundValues( cls.mainconsole );

	Cmd_AddCommand( "menu_main",     MENU_Main_f );
	Cmd_AddCommand( "menu_generic",  MENU_Generic_f );
	Cmd_AddCommand( "menu_loadgame", MENU_LoadGame_f );
	Cmd_AddCommand( "menu_savegame", MENU_SaveGame_f );
	Cmd_AddCommand( "menu_multiplayer", MENU_Multiplayer_f );
	Cmd_AddCommand( "menu_video",    MENU_Video_f );
	Cmd_AddCommand( "menu_adv_video",MENU_Adv_Video_f );
	Cmd_AddCommand( "menu_keys",     MENU_Keys_f );
	Cmd_AddCommand( "menu_logon",    MENU_Logon_f );
	Cmd_AddCommand( "menu_quit",     MENU_Quit_f );
	Cmd_AddCommand( "menu_close",    MENU_Close_f );
	Cmd_AddCommand( "menu_load",     MENU_Load_f );
	Cmd_AddCommand( "invshow",       MENU_Inventory_f );
	Cmd_AddCommand( "showhelp",      MENU_Help_f );
	Cmd_AddCommand( "menu_update",   MENU_Update_f );

	FS_LoadFile( "layouts/help.hdh", &hires_helpdata );
	FS_LoadFile( "layouts/help.hdl", &lores_helpdata );
}

void MENU_Release( void )
{
	MENU_ReleaseDeathmatchMapsConfig();
	FS_FreeFile( lores_helpdata );
	lores_helpdata = NULL;
	FS_FreeFile( hires_helpdata );
	hires_helpdata = NULL;
}

//=========
//MENU_Draw
//=========
void MENU_Draw( void )
{
	// Check to see if the mode has changed
	CONSOLE_CheckResize();

	if ( help_active )
	{
		if ( con_vidres == CON_HIRES )
			DrawLayout3D( (char *) hires_helpdata, cls.mainconsole, -1 );
		else
			DrawLayout3D( (char *) lores_helpdata, cls.mainconsole, -1 );
	}

	if (cls.currentconsole != cls.mainconsole)
	{
		cl.refdef.rdflags &= ~RDF_NOWORLDMODEL;
		cl.refdef.rdflags &= ~RDF_NOCLEAR;
		cl.refdef.rdflags &= ~RDF_NOENTITIES;
		return;
	}

	// Don't draw the world
	cl.refdef.rdflags = RDF_NOWORLDMODEL | RDF_NOCLEAR | RDF_NOENTITIES;

	// repaint everything next frame
	SCR_DirtyScreen ();

	// black out the background
	re.DrawFill ( 0,0,viddef.width, viddef.height, 0, 0, 0 );

	CONSOLE_Process( MAIN_CONSOLE );
}

//================
//MENU_DrawGeneric
//================
void MENU_DrawGeneric( void )
{
	if (cls.currentconsole != cls.genconsole)
		return;

	// repaint everything next frame
	SCR_DirtyScreen ();

	CONSOLE_Resize( cls.genconsole );
	CONSOLE_Process( GENERIC_CONSOLE );
}
