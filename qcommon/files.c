/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "qcommon.h"
#include "glob.h"

// define this to dissalow any data but the demo pak file
//#define	NO_ADDONS

// if a packfile directory differs from this, it is assumed to be hacked
// Full version
#define	PAK0_CHECKSUM	0x40e614e0
// Demo
//#define	PAK0_CHECKSUM	0xb2c6d7ea
// OEM
//#define	PAK0_CHECKSUM	0x78e135c

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/


//
// in memory
//

typedef struct
{
	char	name[SIN_PAK_NAME_LEN];	/* SPAK names up to 119 chars + NUL */
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
cvar_t	*fs_cddir;
cvar_t	*fs_gamedirvar;

typedef struct filelink_s
{
	struct filelink_s	*next;
	char	*from;
	int		fromlength;
	char	*to;
} filelink_t;

filelink_t	*fs_links;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*fs_searchpaths;
searchpath_t	*fs_base_searchpaths;	// without gamedirs


/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/


/*
================
FS_filelength
================
*/
int FS_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
#ifdef _WIN32
#include <windows.h>
static qboolean FS_IsDirectory(const char *path) {
	DWORD result = GetFileAttributesA(path);
	return (result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY));
}
#else
#include <sys/stat.h>
static qboolean FS_IsDirectory(const char *path) {
	struct stat st;
	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}
#endif

void	FS_CreatePath (char *path)
{
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = '/';
		}
	}
}


/*
==============
FS_FCloseFile

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile (FILE *f)
{
	fclose (f);
}


// RAFAEL
/*
	Developer_searchpath
*/
int	Developer_searchpath (int who)
{
	
	int		ch;
	// PMM - warning removal
//	char	*start;
	searchpath_t	*search;
	
	if (who == 1) // xatrix
		ch = 'x';
	else if (who == 2)
		ch = 'r';

	for (search = fs_searchpaths ; search ; search = search->next)
	{
		if (strstr (search->filename, "xatrix"))
			return 1;

		if (strstr (search->filename, "rogue"))
			return 2;
/*
		start = strchr (search->filename, ch);

		if (start == NULL)
			continue;

		if (strcmp (start ,"xatrix") == 0)
			return (1);
*/
	}
	return (0);

}


/*
===========
FS_FOpenFile

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
int file_from_pak = 0;
#ifndef NO_ADDONS
int FS_FOpenFile (char *filename, FILE **file)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	int				i;
	filelink_t		*link;

	file_from_pak = 0;

	// Yamagi Q2 path traversal security check
	if (!filename || strstr (filename, "..") || strchr (filename, ':') || filename[0] == '/' || filename[0] == '\\')
	{
		Com_DPrintf ("FS_FOpenFile: refusing unsafe path: %s\n", filename ? filename : "NULL");
		*file = NULL;
		return -1;
	}

	// check for links first
	for (link = fs_links ; link ; link=link->next)
	{
		if (!strncmp (filename, link->from, link->fromlength))
		{
			Com_sprintf (netpath, sizeof(netpath), "%s%s",link->to, filename+link->fromlength);
			*file = fopen (netpath, "rb");
			if (*file)
			{		
				Com_DPrintf ("link file: %s\n",netpath);
				return FS_filelength (*file);
			}
			return -1;
		}
	}

//
// search through the path, one element at a time
//
	for (search = fs_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
				if (!Q_strcasecmp (pak->files[i].name, filename))
				{	// found it!
					file_from_pak = 1;
					Com_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);
				// open a new file on the pakfile
					*file = fopen (pak->filename, "rb");
					if (!*file)
						Com_Error (ERR_FATAL, "Couldn't reopen %s", pak->filename);	
					fseek (*file, pak->files[i].filepos, SEEK_SET);
					return pak->files[i].filelen;
				}
		}
		else
		{		
	// check a file in the directory tree
			
			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);
			
			*file = fopen (netpath, "rb");
			if (!*file)
				continue;
			
			Com_DPrintf ("FindFile: %s\n",netpath);

			return FS_filelength (*file);
		}
		
	}
	
	Com_DPrintf ("FindFile: can't find %s\n", filename);
	
	*file = NULL;
	return -1;
}

#else

// this is just for demos to prevent add on hacking

int FS_FOpenFile (char *filename, FILE **file)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	int				i;

	file_from_pak = 0;

	// get config from directory, everything else from pak
	if (!strcmp(filename, "config.cfg") || !strncmp(filename, "players/", 8))
	{
		Com_sprintf (netpath, sizeof(netpath), "%s/%s",FS_Gamedir(), filename);
		
		*file = fopen (netpath, "rb");
		if (!*file)
			return -1;
		
		Com_DPrintf ("FindFile: %s\n",netpath);

		return FS_filelength (*file);
	}

	for (search = fs_searchpaths ; search ; search = search->next)
		if (search->pack)
			break;
	if (!search)
	{
		*file = NULL;
		return -1;
	}

	pak = search->pack;
	for (i=0 ; i<pak->numfiles ; i++)
		if (!Q_strcasecmp (pak->files[i].name, filename))
		{	// found it!
			file_from_pak = 1;
			Com_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);
		// open a new file on the pakfile
			*file = fopen (pak->filename, "rb");
			if (!*file)
				Com_Error (ERR_FATAL, "Couldn't reopen %s", pak->filename);	
			fseek (*file, pak->files[i].filepos, SEEK_SET);
			return pak->files[i].filelen;
		}
	
	Com_DPrintf ("FindFile: can't find %s\n", filename);
	
	*file = NULL;
	return -1;
}

#endif


/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
void CDAudio_Stop(void);
#define	MAX_READ	0x10000		// read in blocks of 64k
void FS_Read (void *buffer, int len, FILE *f)
{
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;
	tries = 0;
	while (remaining)
	{
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread (buf, 1, block, f);
		if (read == 0)
		{
			// we might have been trying to read from a CD
			if (!tries)
			{
				tries = 1;
				CDAudio_Stop();
			}
			else
				Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
		}

		if (read == -1)
			Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");

		// do some progress bar thing here...

		remaining -= read;
		buf += read;
	}
}

/*
============
FS_LoadFile

Filename are reletive to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile (char *path, void **buffer)
{
	FILE	*h;
	byte	*buf;
	int		len;

	buf = NULL;	// quiet compiler warning

// look for it in the filesystem or pack files
	len = FS_FOpenFile (path, &h);
	if (!h)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}
	
	if (!buffer)
	{
		fclose (h);
		return len;
	}

	buf = Z_Malloc(len + 1);
	*buffer = buf;

	FS_Read (buf, len, h);

	fclose (h);

	buf[len] = 0;

	return len;
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile (void *buffer)
{
	if (!buffer)
		return;
	Z_Free (buffer);
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		*info;
	unsigned		checksum;

	packhandle = fopen(packfile, "rb");
	if (!packhandle)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	{
		int	ident = LittleLong(header.ident);
		/* PACK (Quake 2) or SPAK (SiN Steam .sin) */
		if (ident != IDPAKHEADER && ident != IDSINHEADER)
		{
			fclose (packhandle);
			Com_Printf ("%s is not a packfile\n", packfile);
			return NULL;
		}
		header.dirofs = LittleLong (header.dirofs);
		header.dirlen = LittleLong (header.dirlen);

		if (ident == IDSINHEADER)
		{
			/* SiN SPAK: 128-byte entries (name[120] + pos + len) */
			dsinpackfile_t	*sinfo;
			numpackfiles = header.dirlen / sizeof(dsinpackfile_t);
			if (numpackfiles > MAX_FILES_IN_PACK)
				Com_Error (ERR_FATAL, "%s has %i files", packfile, numpackfiles);
			newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));
			sinfo = malloc (header.dirlen);
			if (!sinfo)
				Com_Error (ERR_FATAL, "FS_LoadPackFile: out of memory");
			fseek (packhandle, header.dirofs, SEEK_SET);
			fread (sinfo, 1, header.dirlen, packhandle);
			checksum = Com_BlockChecksum ((void *)sinfo, header.dirlen);
			(void)checksum;
			for (i = 0; i < numpackfiles; i++)
			{
				memcpy (newfiles[i].name, sinfo[i].name, SIN_PAK_NAME_LEN);
				newfiles[i].name[SIN_PAK_NAME_LEN - 1] = 0;
				newfiles[i].filepos = LittleLong (sinfo[i].filepos);
				newfiles[i].filelen = LittleLong (sinfo[i].filelen);
			}
			free (sinfo);
			goto pack_ready;
		}
	}
	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Com_Error (ERR_FATAL, "%s has %i files", packfile, numpackfiles);

	newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));
	info = malloc (header.dirlen);
	if (!info)
		Com_Error (ERR_FATAL, "FS_LoadPackFile: out of memory");

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
	checksum = Com_BlockChecksum ((void *)info, header.dirlen);

#ifdef NO_ADDONS
	if (checksum != PAK0_CHECKSUM)
		return NULL;
#endif
	(void)checksum;
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}
	free (info);
pack_ready:

	pack = Z_Malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	
	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void FS_AddGameDirectory (char *dir)
{
	int				i;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];

	strcpy (fs_gamedir, dir);

	//
	// add the directory to the search path
	//
	search = Z_Malloc (sizeof(searchpath_t));
	strcpy (search->filename, dir);
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	// also SiN Steam: pak0.sin pak1.sin, ...
	//
	for (i=0; i<10; i++)
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = FS_LoadPackFile (pakfile);
		if (!pak)
		{
			Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.sin", dir, i);
			pak = FS_LoadPackFile (pakfile);
		}
		if (!pak)
			continue;
		search = Z_Malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;		
	}
#if defined(_WIN32) || defined(__linux__)
	/* Support SiN Steam / localized assets: dir/locale/en */
	Com_sprintf (pakfile, sizeof(pakfile), "%s/locale/en", dir);
	if (FS_IsDirectory(pakfile)) {
		search = (searchpath_t *) Z_Malloc(sizeof(searchpath_t));
		strncpy (search->filename, pakfile, sizeof(search->filename)-1);
		search->filename[sizeof(search->filename)-1] = 0;
		search->next = fs_searchpaths;
		fs_searchpaths = search;
	}
#endif
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir (void)
{
	return fs_gamedir;
}

/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec (void)
{
	char *dir;
	char name [MAX_OSPATH];

	dir = Cvar_VariableString("gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir); 
	else
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASEDIRNAME); 
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText ("exec autoexec.cfg\n");
	Sys_FindClose();
}


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack)
		{
			fclose (fs_searchpaths->pack->handle);
			Z_Free (fs_searchpaths->pack->files);
			Z_Free (fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Z_Free (fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->value)
		Cbuf_AddText ("vid_restart\nsnd_restart\n");

	Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir,BASEDIRNAME) || (*dir == 0))
	{
		Cvar_FullSet ("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet ("gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);
		if (fs_cddir->string[0])
			FS_AddGameDirectory (va("%s/%s", fs_cddir->string, dir) );
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
	}
}


/*
================
FS_Link_f

Creates a filelink_t
================
*/
void FS_Link_f (void)
{
	filelink_t	*l, **prev;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("USAGE: link <from> <to>\n");
		return;
	}

	// see if the link already exists
	prev = &fs_links;
	for (l=fs_links ; l ; l=l->next)
	{
		if (!strcmp (l->from, Cmd_Argv(1)))
		{
			Z_Free (l->to);
			if (!strlen(Cmd_Argv(2)))
			{	// delete it
				*prev = l->next;
				Z_Free (l->from);
				Z_Free (l);
				return;
			}
			l->to = CopyString (Cmd_Argv(2));
			return;
		}
		prev = &l->next;
	}

	// create a new link
	l = Z_Malloc(sizeof(*l));
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1));
	l->fromlength = strlen(l->from);
	l->to = CopyString(Cmd_Argv(2));
}

/*
** FS_ListFiles
*/
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = malloc( sizeof( char * ) * nfiles );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = strdup( s );
#ifdef _WIN32
			strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
** FS_Dir_f
*/
void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		strcpy( wildcard, Cmd_Argv( 1 ) );
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				free( dirnames[i] );
			}
			free( dirnames );
		}
		Com_Printf( "\n" );
	};
}

/*
============
FS_Path_f

============
*/
void FS_Path_f (void)
{
	searchpath_t	*s;
	filelink_t		*l;

	Com_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}

	Com_Printf ("\nLinks:\n");
	for (l=fs_links ; l ; l=l->next)
		Com_Printf ("%s : %s\n", l->from, l->to);
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return fs_gamedir;

	prev = fs_gamedir;
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
			continue;
		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}


/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem (void)
{
	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("link", FS_Link_f);
	Cmd_AddCommand ("dir", FS_Dir_f );

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get ("basedir", ".", CVAR_NOSET);

	//
	// cddir <path>
	// Logically concatenates the cddir after the basedir for 
	// allows the game to run from outside the data tree
	//
	fs_cddir = Cvar_Get ("cddir", "", CVAR_NOSET);
	if (fs_cddir->string[0])
		FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_cddir->string) );

	//
	// start up with baseq2 by default
	//
	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_basedir->string) );

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	if (fs_gamedirvar->string[0])
		FS_SetGamedir (fs_gamedirvar->string);

	FS_InitPlayerdir();
}





static cvar_t *cl_player;
static char fs_relativeplayerdir[MAX_OSPATH];
static char fs_playerdir[MAX_OSPATH];

void FS_InitPlayerdir (void)
{
	char path[MAX_OSPATH];
	char playerpath[MAX_OSPATH];
	int i;

	cl_player = Cvar_Get ("player", "blade", CVAR_ARCHIVE);
	strcpy (playerpath, cl_player->string);

	for (i = 0; i < (int)strlen(playerpath); i++)
	{
		if (playerpath[i] == ' ')
			playerpath[i] = '_';
	}

	Com_sprintf (fs_relativeplayerdir, sizeof(fs_relativeplayerdir), "players/%s", playerpath);
	Com_sprintf (fs_playerdir, sizeof(fs_playerdir), "%s/%s", FS_Gamedir(), fs_relativeplayerdir);
	Com_sprintf (path, sizeof(path), "%s/", fs_playerdir);
	FS_CreatePath (path);
}

const char *FS_RelativePlayerdir (void)
{
	return fs_relativeplayerdir;
}

const char *FS_Playerdir (void)
{
	return fs_playerdir;
}

void FS_FreeList (const char **list, int n)
{
	int i;
	if (list) {
		for (i = 0; i < n; i++) {
			if (list[i]) {
				free((void *)list[i]);
				list[i] = 0;
			}
		}
		free((void *)list);
	}
}

#define MAX_FIND_FILES 1024

const char **FS_FindFiles (const char *path, const char *pattern, int *numfiles)
{
	const char **list = NULL;
	searchpath_t *search;
	const char *ptr;
	pack_t *pak;
	int i, j, len, pathlen;
	qboolean found;
	char searchpath[MAX_OSPATH];
	char globpattern[32];

	if (!path || !pattern || !numfiles)
		return NULL;

	*numfiles = 0;

	len = (int)strlen( path );
	for (i = 0; i < len; i++)
	{
		if (path[i] == '\\')
			searchpath[i] = '/';
		else
			searchpath[i] = path[i];
	}

	if (i > 0 && searchpath[i-1] != '/')
	{
		searchpath[i] = '/';
		i++;
	}

	searchpath[i] = '\0';
	pathlen = i;

	strncpy(globpattern, pattern, sizeof(globpattern)-1);
	globpattern[sizeof(globpattern)-1] = 0;
	if (!strcmp(globpattern, "*.*"))
		strcpy(globpattern, "*");

	list = (const char **) malloc(sizeof(char *) * MAX_FIND_FILES);
	if (!list)
	{
		Com_Printf("Could not allocate list for FS_FindFiles\n");
		return NULL;
	}

	memset((void *)list, 0, sizeof(char *) * MAX_FIND_FILES);

	for (search = fs_searchpaths; search; search = search->next)
	{
		if (search->pack)
		{
			pak = search->pack;
			for (i = 0; i < pak->numfiles ; i++)
			{
				if (!strnicmp(searchpath, pak->files[i].name, pathlen))
				{
					ptr = pak->files[i].name + pathlen;

					if (glob_match(globpattern, ptr))
					{
						found = false;
						for (j = 0; j < *numfiles; j++)
						{
							if (!stricmp(pak->files[i].name, list[j]))
							{
								found = true;
								break;
							}
						}

						if (!found && *numfiles < MAX_FIND_FILES)
						{
							list[*numfiles] = _strdup(pak->files[i].name);
							*numfiles = *numfiles + 1;
						}
					}
				}
			}
		}
		else
		{
			char foundname[MAX_OSPATH];
			char findname[MAX_OSPATH];
			char ** files;
			int numf;

			Com_sprintf(findname, sizeof(findname), "%s/%s%s", search->filename, searchpath, pattern);
			files = FS_ListFiles(findname, &numf, 0, 0);

			if (files)
			{
				for (i = 0; i < (numf - 1); i++)
				{
					ptr = strstr(files[i], searchpath);
					if (ptr)
					{
						strcpy (foundname, ptr);
						found = false;

						for (j = 0; j < *numfiles; j++)
						{
							if (!stricmp(list[j], foundname))
							{
								found = true;
								break;
							}
						}

						if (!found && *numfiles < MAX_FIND_FILES)
						{
							list[*numfiles] = _strdup(foundname);
							*numfiles = *numfiles + 1;
						}
					}

					if (files[i])
					{
						free(files[i]);
					}
				}

				free(files);
			}
		}
	}

	return list;
}