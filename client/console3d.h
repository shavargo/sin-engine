//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/console3d.h                                            $
// $Revision:: 3                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 8/21/01 6:05 PM                                                $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// In game 3d consoles

#ifndef __CONSOLE3D_H__
#define __CONSOLE3D_H__

#ifndef __CL_CONSOLE_H__
#include "cl_console.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {MENU3D, CONSOLE3D} consolefocus_t;
#define TAG_CONSOLE_BASE 100

#define M3DF_NUMBERSONLY   (1<<0)

#define MAXPRINTMSG	            4096
#define CONSOLE_VIRTUAL_WIDTH    640.0f
#define CONSOLE_VIRTUAL_HEIGHT   480.0f
#define MAX_MENU3D_ITEMS         32
#define MAX_LOCAL_SERVERS        8
#define MAX_DISPLAYNAME          30
#define MAX_PLAYERMODELS         64
#define MAX_BIKEMODELS           16 
#define MAX_SAVEGAMES            15
#define MAX_DEATHMATCH_MAPS      128
#define NO_SERVER_STRING	      "<no server>"

#define CON_NORES 0
#define CON_HIRES 1
#define CON_LORES 2

typedef struct menu_func_s
   {
   const char *string;
   void (*callback)(void *self);
   } menu_func_t;

typedef struct key_func_s
   {
   const char *string;
   const char *(*callback)(int key);
   } key_func_t;

typedef struct {
	int		nskins;
	char	**SkinDisplayNamesPtr;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

typedef struct
   { 
   int      nskins;
   char  **skindisplaynames;
   char  displayname[MAX_DISPLAYNAME];
   char  directory[MAX_QPATH];
} bikemodelinfo_s;

extern menu_func_t functions[];

typedef enum {ACTION3D, FIELD3D, LIST3D, SLIDER3D, TOGGLE3D, HEADER3D, BIND3D} menu3dtype;

typedef struct acursor_s 
   {
   char *name;
   int numframes;
   int height, width;
   } acursor_t;

typedef struct
   {
   int curvalue;
   int numitems;
   char *name;
	char **itemnames;
   char **itemactions;
   } menulist3d_t;

typedef struct
   {
   char *name;
   char *command;
   qboolean bind_grab_active;
   } menubind3d_t;

typedef struct 
   {
	float minvalue;
	float maxvalue;
	float curvalue;
   int   length;
	float range;
   char  *name;
   } menuslider3d_t;

typedef struct 
   {
   char        *name;
   char        *buffer;
	int			cursor;
	int			length;
	int			visible_length;
	int			visible_offset;
   int         flags;
   } menufield3d_t;

typedef struct
   {
   char           *menu_layout;
   char           *sel_menu_layout;
   char           *cursor_layout;
   char           *action;
   char           *helpdata;
   char           *localdata;
   menulist3d_t   list;
   menuslider3d_t slider;
   menufield3d_t  field;
   menubind3d_t   bind;
   menu3dtype     type;
   float          helptime;
   float          fade;
   float          fade_delay;
   } item3d_t;

typedef struct
   {
	item3d_t    *item[MAX_MENU3D_ITEMS];
   acursor_t   cursor;
   char        *menu_name;
   char        *menuaction;
   int         num_menu_items;
   const char *(*keyfunc)(int key);
   } menuitem3d_t;

typedef struct
   {
   float w;
   float h;
   float value;
   float min;
   float max;
   float red,green,blue,alpha;
   } sbar_t;


#define	CON3D_TEXTSIZE	4096
typedef struct
{
	qboolean	initialized;
	char     *text;
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
	int		ormask;			// high bit mask for colored characters
	int 	   linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback
	float	   cursorspeed;
	int		vislines;
	float	   times[NUM_CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines
} con3d_t;

typedef struct console3d_s {
	char           console_name[MAX_CONSOLE_NAME];
	int            console_number;
	qboolean       playsound;                 // Play console sounds
	qboolean       active;                    // Console is active
	qboolean       scrollcon;                 // Scrolling text
	qboolean       menucon;                   // Menu
	qboolean       drawmenu;                  // Draw the menu
	char           *LayoutPtr;                // Main layout string
	float          virtual_width;             // User defined width
	float          virtual_height;            // User defined height
	float          oovh;                      // 1.0f / virtual_width
	float          oovw;                      // 1.0f / virtual_height
	float          offsetx;                   // offset in screen space for main menu
	float          offsety;                   // offset in screen space for main menu
	float          fraction;                  // Fraction of console to draw
	float          r,g,b,a;                   // Foreground colors
	float          autotrigger_time;          // Amount of time to wait before auto triggering the item
	int            columns;                   // Used to determine font height
	int            rows;                      // Used to determine font width
	int            blinkrate;                 // Rate of blinking text
	int            typerate;                  // Rate of typing text
	int            type_index;                // Index of the current character to type.
	char           *KeyLinesPtr[32];            // History buffer for console
	int            key_linepos;               // Cursor  position of edit line
	int            old_key_linepos;           // Cursor  position of edit line in the last frame
	int		      edit_line;                 // Current line editing in history buffer
	int            old_edit_line;             // Previous line edited in the last frame
	int		      history_line;              // history line
	int            current_menu_level;        // Current menu level
	int            sel_menu_item;             // Currently selected item
	int            numlevels;                 // Number of menu levels
	consolefocus_t focus;                     // Focus of input (console or menu)
	int            *prev_menu_level;          // List of previous menu levels 
	int            *prev_sel_item;            // List of previously selected items
	char           menu_file[MAX_OSPATH];     // The menufile for this console
	menuitem3d_t   **menu3d;                  // The menu data structure
	char           *globlayout;               // Global layout for the menus
	con3d_t	      con;                       // The console data structure
	sbar_t         *SBarPtr;                  // The status bars data structure
	struct         console3d_s *next;         // Pointer to the next console
} console3d_t;

extern int con_vidres;
extern void CON_Init(void);
extern void CON_Destroy(void);
void        CONSOLE_Process( const char *surface_name );
qboolean    CONSOLE_PopMenu( console3d_t *p );
console3d_t *CONSOLE_Create( const char *s, qboolean scroll, qboolean menu );
console3d_t *CONSOLE_FindConsoleByNumber( int number );
void        CONSOLE_ProcessMenuAction( console3d_t *p, const char *s );
void        CONSOLE_Destroy( console3d_t *p );
void        CONSOLE_ClearConsoles( void );
void        CONSOLE_SlideItem( console3d_t *p, int dir, item3d_t *item );
void        CONSOLE_SelectItem( console3d_t *p, int dir );
qboolean    CONSOLE_LoadMenuFromFile( console3d_t *p, const char *path );
qboolean    CONSOLE_LoadMenu( const char *s );
void        CONSOLE_Init( void );
void        CONSOLE_UpdateVariable( const char *var_name, const char *msg, console3d_t *p, float value, menu3dtype t);
void        CONSOLE_DrawHUD( void );
void        CONSOLE_DrawStatusStuff( void );
void        CONSOLE_DrawMissionComputer( void );
void        CONSOLE_SetFocusByType(console3d_t *p, consolefocus_t focus);
void        CONSOLE_Exit(console3d_t *p);
void        CONSOLE_LoadLayoutFromFile(console3d_t *p, const char *path);
void        CONSOLE_ConPrint(console3d_t *p, const char *txt);
void        CONSOLE_ConClear(console3d_t *p);
void        CONSOLE_CreateBase(int number);
void        CONSOLE_SetType(console3d_t *p, int spawnflags);
void        CONSOLE_SendCharacterToServer(console3d_t *p, int key);
void        CONSOLE_ResetTimer(console3d_t *p);
void        CONSOLE_ResetTypeTimer(console3d_t *p);
void        CONSOLE_SetKeyFunction(console3d_t *p);
void        CONSOLE_PushMenu(console3d_t *p, int level);
void        CONSOLE_CheckResize(void);
void        CONSOLE_SkipHeader(console3d_t *p);
void        CONSOLE_CenterPrint( const char *string );
void        CONSOLE_ScreenPrint( const char *string, float time );
void        CONSOLE_DrawGlobalLayout( void );
void        CONSOLE_DrawSpectator ( void );
void        CONSOLE_DrawScores ( void );
void        CONSOLE_DrawLoadingStatus( void );
void        CONSOLE_DialogPrint( const char *string );
void        CONSOLE_ClearFade_f( void );
void        CONSOLE_ClearFadeExtra( void );
void        CONSOLE_DrawFades( void );
void        CONSOLE_DrawDialog( void );
void        CONSOLE_DrawGenericOverlay( void );
void        CONSOLE_Resize( console3d_t *p );
void        CONSOLE_ClearMenu( console3d_t *p );
void        CONSOLE_ClearIntermissionData( void );
void        CONSOLE_SetupDefaultIntermissionLayoutFile( const char *s );
void        CONSOLE_FadeIn( const char *s );
void        CONSOLE_ClearIntermissionData_f( void );
char		*CONSOLE_strdup( const char *str, console3d_t *p );
void			MENU_Reload_f( void );
void        MENU_AnyKeyToContinue_f( void );
void        MENU_PushMenu( const char * );
qboolean    MENU_InitDeathmatchMapsConfig( void );
void        MENU_Init(void);
extern void MENU_Release(void);
const char *MENU_FindNameForCommand(const char *name);
void        MENU_FindKeysForCommand (const char *command, int *twokeys);
void        MENU_FindDoubleKeysForCommand (const char *command, int *twokeys);
const char *MENU_Bind_Key_f(menubind3d_t *b, int key, unsigned time);
qboolean    MENU_Field_Key_f(menufield3d_t *f, int key);
void        MENU_Main_f(void);
void        MENU_Draw(void);
void        MENU_DrawGeneric(void);
void MENU_UpdateSkinList(void *self);
void MENU_UpdateBikeSkinList(void *self);
void        MENU_DrawPlayerModel(float x, float y, float width, float height);
void        MENU_DrawInventoryModel(float x, float y, float width, float height, float scale);
void        MENU_InitPlayerConfig(void);
void        MENU_ReleasePlayerConfig(void);
void        MENU_AddToServerList (netadr_t adr, const char *info);

/* In CL_Hud.c */

extern void HUD_Restart_f( void );
extern void HUD_Init( int res );
extern void HUD_Destroy(void);

extern void DrawLayout3D(const char *s, console3d_t *p, int itemnum);
extern console3d_t *active_consoles;
extern const char *menu_out_sound;
extern const char *menu_in_sound;
extern const char *menu_move_sound;
extern float con_autotrigger_time;
extern char		m_savestrings[MAX_SAVEGAMES][32];
extern qboolean	m_savevalid[MAX_SAVEGAMES];
extern char     local_server_names[MAX_LOCAL_SERVERS][80];
extern float  cl_inventory_scale;
extern qboolean display_player_on_bike;
extern int  s_numdeathmatchmaps;
extern char **s_deathmatchmaps;
extern playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
extern cvar_t *skin;
extern int s_numplayermodels;
extern char *s_pmnames[MAX_PLAYERMODELS];
extern cvar_t	*model;
extern bikemodelinfo_s s_bikemi[MAX_BIKEMODELS];
extern cvar_t  *bikemodel;
extern cvar_t  *bikeskin;
extern cvar_t  *precache;
extern int s_numbikemodels;
extern char *s_bikemnames[MAX_BIKEMODELS];
extern cvar_t		*scr_centertime;
extern cvar_t *precache;

#ifdef __cplusplus
}
#endif

#endif
