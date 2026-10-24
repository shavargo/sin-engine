//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/cl_hud.c                                               $
// $Revision:: 3                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 8/21/01 6:04 PM                                                $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// Client side hud

#include "client.h"

cvar_t *cl_hudfile;

void HUD_Restart_f ( void )
{
	int  ret;
	char hudfile[ MAX_OSPATH ];

	COM_StripExtension( Cvar_VariableString("cl_hudfile"),hudfile );

	if ( con_vidres == CON_LORES )
		COM_DefaultExtension( hudfile,".hdl" );
	else
		COM_DefaultExtension( hudfile,".hdh" );

	Cvar_Set( "cl_hudfile", hudfile );

	FS_FreeFile( cls.hud );
	ret = FS_LoadFile( va( "huds/%s",cl_hudfile->string ),(void **)&cls.hud);

	if ( ret == -1 ) {
		Com_Printf( "HUD_Restart_f: HUD huds/%s not found\n",cl_hudfile->string );
		cls.hud = 0;
	}
}

void HUD_Init( int res )
{
	char hudfile[ MAX_OSPATH ];

	cl_hudfile = Cvar_Get( "cl_hudfile", "hud01", CVAR_ARCHIVE );

	COM_StripExtension( Cvar_VariableString("cl_hudfile"),hudfile );

	if ( res == CON_LORES )
		COM_DefaultExtension( hudfile,".hdl" );
	else if ( res == CON_HIRES )
		COM_DefaultExtension( hudfile,".hdh" );

	Cvar_Set( "cl_hudfile", hudfile );
	Cmd_AddCommand( "hud_restart", HUD_Restart_f );
	cls.hud = 0;
	HUD_Restart_f();
}

void HUD_Destroy(void)
{
	FS_FreeFile( cls.hud );
	cls.hud = 0;
}
