/*
 * Clean-room GPL implementation of SiN Game Module (API 14)
 * Core entry points, Init, Shutdown, RunFrame, GetGameAPI.
 */
#include "g_local.h"

game_locals_t game;
game_import_t gi;
static game_export_t globals;

void Com_Printf (char *msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start (argptr, msg);
	vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);

	if (gi.dprintf)
		gi.dprintf ("%s", text);
}

void G_Precache (void)
{
	/* Precache standard weapons and effects (only valid during ss_loading) */
	game.model_view_punch = gi.modelindex ("models/view_punch.def");
	game.model_view_magnum = gi.modelindex ("models/view_magnum.def");
	game.model_view_shotgun = gi.modelindex ("models/view_shotgun.def");
	game.model_view_asrifle = gi.modelindex ("models/view_asrifle.def");

	game.sound_magnum_fire = gi.soundindex ("weapons/magnum/fire3.wav");
	game.sound_shotgun_fire = gi.soundindex ("weapons/shotgun/shotfire.wav");
	game.sound_punch_swing = gi.soundindex ("weapons/punch/whsh1.wav");
	game.sound_asrifle_fire = gi.soundindex ("weapons/asrifle/6.wav");

	gi.soundindex ("environment/Doors/door1opn.wav");
	gi.soundindex ("environment/SWITCH/Sw1.wav");

	gi.imageindex ("w_magnum");
	gi.imageindex ("w_shotgun");
	gi.imageindex ("w_asrifle");
	gi.imageindex ("i_ammo_magnum");
	gi.imageindex ("i_ammo_shotgun");
}

static void InitGame (void)
{
	cvar_t *maxclients;

	gi.dprintf ("==== Clean-Room SiN Game Init (API %d) ====\n", GAME_API_VERSION);

	maxclients = gi.cvar ("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
	game.max_clients = maxclients ? (int)maxclients->value : 8;
	if (game.max_clients < 1)
		game.max_clients = 1;
	if (game.max_clients > 32)
		game.max_clients = 32;

	game.clients = (gclient_t *)calloc (game.max_clients, sizeof(gclient_t));
	G_InitEdicts ();

	globals.edicts = game.edicts;
	globals.edict_size = sizeof(edict_t);
	globals.num_edicts = game.num_edicts;
	globals.max_edicts = game.max_edicts;

	game.time = 0.0f;
	game.frametime = FRAMETIME;
}

static void ShutdownGame (void)
{
	gi.dprintf ("==== Clean-Room SiN Game Shutdown ====\n");
	if (game.edicts)
	{
		free (game.edicts);
		game.edicts = NULL;
	}
	if (game.clients)
	{
		free (game.clients);
		game.clients = NULL;
	}
	memset (&game, 0, sizeof(game));
}

static void G_RunFrame (void)
{
	game.time += game.frametime;
	globals.num_edicts = game.num_edicts;

	/* Process automated sliding doors */
	G_UpdateDoors (game.frametime);
}

static void ServerCommand (void)
{
	const char *cmd = gi.argv (1);
	if (!cmd)
		return;
	gi.dprintf ("ServerCommand: %s\n", cmd);
}

static void CreateSurfaces (csurface_t *surfaces, int count)
{
	(void)surfaces;
	(void)count;
}

static void WriteGame (const char *filename, qboolean autosave)
{
	(void)filename;
	(void)autosave;
}

static void ReadGame (const char *filename)
{
	(void)filename;
}

static void WriteLevel (const char *filename, qboolean autosave)
{
	(void)filename;
	(void)autosave;
}

static void ReadLevel (const char *filename)
{
	(void)filename;
}

game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	memset (&globals, 0, sizeof(globals));
	globals.apiversion = GAME_API_VERSION;

	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientConnect = ClientConnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientCommand = ClientCommand;
	globals.ClientThink = ClientThink;

	globals.RunFrame = G_RunFrame;
	globals.ServerCommand = ServerCommand;
	globals.CreateSurfaces = CreateSurfaces;

	globals.edicts = game.edicts;
	globals.edict_size = sizeof(edict_t);
	globals.num_edicts = 0;
	globals.max_edicts = MAX_EDICTS;

	return &globals;
}
