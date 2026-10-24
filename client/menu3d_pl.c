//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/menu3d_pl.c                                            $
// $Revision:: 5                                                              $
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
//

#include "client.h"

static int framenum = 0;
static int oldframenum = 0;

// 2015 code
qboolean display_player_on_bike = false;

playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
char *s_pmnames[MAX_PLAYERMODELS];
int  s_numplayermodels = 0;

extern cvar_t	*skin;

// 2015 code
extern cvar_t *bikemodel;
extern cvar_t *bikeskin;

bikemodelinfo_s s_bikemi[MAX_BIKEMODELS];
char *s_bikemnames[MAX_BIKEMODELS];
int  s_numbikemodels = 0;

void MENU_DrawInventoryModel (float x, float y, float width, float height, float inventory_scale)
{
	refdef_t refdef;
	int current_model_index;

	memset(&refdef, 0, sizeof(refdef));

	refdef.x = viddef.width / 2 - 72;
	refdef.y = viddef.height / 2 - 72;
	refdef.width  = 144;
	refdef.height = 144;
	refdef.fov_x  = 40;
	refdef.fov_y  = CalcFov(refdef.fov_x, refdef.width, refdef.height);
	refdef.time   = cls.realtime*0.001;

	current_model_index = cl.frame.playerstate.stats[STAT_SELECTED_MODELINDEX];

	{
		static int yaw=0;
		static int direction=1;
		static int animnum = -1;
		static int framenum = 0;
		static struct model_t *currentmodel = NULL;
		static int lastframechange = 0;
		int defhandle;
		entity_t entity;
		vec3_t mins, maxs, centroid;

		memset(&entity, 0, sizeof(entity));

		entity.model = cl.model_draw[current_model_index];
		entity.flags = RF_FULLBRIGHT;

		defhandle = re.SINMDL_GetDefHandle(entity.model);
		if (defhandle == -1)
			return;

		entity.oldframe = framenum;
		entity.oldanim = animnum;

		if (lastframechange != (unsigned)(refdef.time * 10))
		{
			lastframechange = (unsigned)(refdef.time * 10);
			framenum++;
			if (animnum >= 0 && framenum >= SINMDL_Anim_NumFrames(defhandle, animnum))
				animnum = -1;
		}

		if (animnum < 0 || currentmodel != entity.model)
		{
			animnum = SINMDL_Anim_Random(defhandle, "pose");
			if (animnum < 0)
				animnum = 0;
			framenum = 0;
			lastframechange = (unsigned)(refdef.time * 10);
			if (currentmodel != entity.model || entity.oldframe < 0 || entity.oldanim < 0)
			{
				entity.oldframe = framenum;
				entity.oldanim = animnum;
			}
		}

		currentmodel     = entity.model;
		entity.frame     = framenum;
		entity.anim      = animnum;
		entity.backlerp  = (refdef.time * 10) - (float)(int)(refdef.time * 10);

		SINMDL_CalculateBounds(defhandle, 1.0, mins, maxs);
		entity.scale = 1 + inventory_scale;

		entity.origin[0] = 72;
		entity.origin[1] = 0;
		entity.origin[2] = 0;

		VectorAdd(mins,maxs,centroid);
		VectorScale(centroid, 0.5f, centroid);
		entity.origin[2] -= centroid[2] * entity.scale;
		VectorCopy(entity.origin, entity.oldorigin);

		entity.angles[1] = yaw;

		AnglesToMat(entity.angles, entity.mat);

		yaw += direction;
		if (yaw > 68)
			direction = -1;
		if (yaw < -68)
			direction = 1;

		refdef.areabits     = 0;
		refdef.num_entities = 1;
		refdef.entities     = &entity;
		refdef.lightstyles  = 0;
		refdef.rdflags      = RDF_NOWORLDMODEL | RDF_NOCLEAR;
		refdef.height       += 4;

		re.RenderFrame(&refdef);
	}
}

void MENU_DrawPlayerModel(float x, float y, float width, float height)
{
	refdef_t refdef;
	char scratch[MAX_QPATH];
	int current_model_index;
	int current_skin_index;
	int num_ents = 1;

	memset(&refdef, 0, sizeof(refdef));

	refdef.x = x * viddef.width;
	refdef.y = y * viddef.height;
	refdef.width  = width;
	refdef.height = height;

	refdef.fov_x  = 20;
	refdef.fov_y  = CalcFov(refdef.fov_x, refdef.width, refdef.height);
	refdef.time   = cls.realtime*0.001;

	current_model_index = Cvar_VariableValue("con_playermodel");
	current_skin_index = Cvar_VariableValue("con_playerskin");

	if (s_pmi[current_model_index].SkinDisplayNamesPtr)
	{
		static int yaw;
		static int animnum = -1;
		static struct model_t *currentmodel = NULL;
		static int lastframechange = 0;
		int defhandle;
		entity_t entity[2];
		vec3_t mins, maxs, size;

		memset(&entity, 0, sizeof(entity));

		Com_sprintf(scratch, sizeof(scratch), "models/%s.def", s_pmi[current_model_index].displayname);
		entity[0].model = re.RegisterModel(scratch);

		Com_sprintf(scratch, sizeof(scratch), "%s%s.tga",s_pmi[current_model_index].directory, s_pmi[current_model_index].SkinDisplayNamesPtr[current_skin_index]);
		entity[0].skin = re.RegisterSkin (scratch);

		entity[0].skinnum = 0;
		entity[0].flags = RF_FULLBRIGHT;
		entity[0].origin[0] = 80;
		entity[0].origin[1] = 0;
		entity[0].origin[2] = -16;
		VectorCopy(entity[0].origin, entity[0].oldorigin);

		defhandle = re.SINMDL_GetDefHandle(entity[0].model);

		entity[0].oldanim = animnum;

		if (display_player_on_bike)
		{
			animnum = SINMDL_Anim_Random(defhandle, "ride");
			if (animnum < 0)
				animnum = 0;

			oldframenum = 0;
			framenum = 0;
			lastframechange = (unsigned)(refdef.time * 10);

			if (currentmodel != entity[0].model || entity[0].oldframe < 0 || entity[0].oldanim < 0)
			{
				entity[0].oldframe = framenum;
				entity[0].oldanim = animnum;
			}
		}
		else
		{
			if (lastframechange != (unsigned)(refdef.time * 10))
			{
				lastframechange = (unsigned)(refdef.time * 10);
				oldframenum = framenum;
				framenum++;
				if (animnum >= 0 && framenum >= SINMDL_Anim_NumFrames(defhandle, animnum))
					animnum = -1;
			}

			if (animnum < 0 || currentmodel != entity[0].model)
			{
				animnum = SINMDL_Anim_Random(defhandle, "pose");
				if (animnum < 0)
					animnum = 0;
				oldframenum = 0;
				framenum = 0;
				lastframechange = (unsigned)(refdef.time * 10);
				if (currentmodel != entity[0].model || entity[0].oldframe < 0 || entity[0].oldanim < 0)
				{
					entity[0].oldframe = framenum;
					entity[0].oldanim = animnum;
				}
			}
		}

		currentmodel       = entity[0].model;
		entity[0].frame    = framenum;
		entity[0].oldframe = oldframenum;
		entity[0].anim     = animnum;
		entity[0].backlerp = 1 - ((refdef.time * 10) - (float)(int)(refdef.time * 10));
		SINMDL_Anim_CalculateBounds(defhandle, animnum, 1.0, mins, maxs);
		VectorSubtract(maxs, mins, size);
		entity[0].scale = 32.0 / size[2];

		entity[0].angles[1] = yaw++;
		AnglesToMat(entity[0].angles, entity[0].mat);
		if (++yaw > 360)
			yaw -= 360;

		if (display_player_on_bike)
		{
			num_ents = 2;

			current_model_index = Cvar_VariableValue("con_bikemodel");
			current_skin_index = Cvar_VariableValue("con_bikeskin");
			if (s_bikemi[current_model_index].skindisplaynames)
			{
				// need to tack extension back on since I removed it
				Com_sprintf(scratch, sizeof(scratch), "models/%s.def", s_bikemi[current_model_index].displayname);
				entity[1].model = re.RegisterModel(scratch);

				// need to tack extension back on since I removed it
				//Com_sprintf(scratch, sizeof(scratch), "%s%s",s_pmi[current_model_index].directory, s_pmi[current_model_index].skindisplaynames[current_skin_index]);
				Com_sprintf(scratch, sizeof(scratch), "%s%s.tga",s_bikemi[current_model_index].directory, s_bikemi[current_model_index].skindisplaynames[current_skin_index]);
				entity[1].skin = re.RegisterSkin (scratch);

				entity[1].skinnum = 0;
				entity[1].flags = RF_FULLBRIGHT;
				entity[1].origin[0] = 80;
				entity[1].origin[1] = 0;
				//entity[1].origin[2] = -16;
				entity[1].origin[2] = 0;
				VectorCopy(entity[1].origin, entity[1].oldorigin);

				defhandle = re.SINMDL_GetDefHandle(entity[1].model);

				//entity.oldframe = framenum;
				entity[1].oldanim = 0;

				entity[1].oldframe = 0;
				entity[1].oldanim = 0;

				entity[1].frame    = 0;
				entity[1].oldframe = 0;
				entity[1].anim     = 0;
				//entity[1].backlerp  = 1 - ((refdef.time * 10) - (float)(int)(refdef.time * 10));
				SINMDL_Anim_CalculateBounds(defhandle, 0, 1.0, mins, maxs);
				VectorSubtract(maxs, mins, size);
				entity[1].scale = 26.0 / size[0];

				entity[1].angles[1] = entity[0].angles[1];
				AnglesToMat(entity[1].angles, entity[1].mat);

				// make the player's position and scale match the bike's
				entity[0].origin[2] = entity[1].origin[2];
				VectorCopy(entity[0].origin, entity[0].oldorigin);
				entity[0].scale = entity[1].scale;
			}
		}

		refdef.areabits     = 0;
		refdef.num_entities = num_ents;
		refdef.entities     = entity;
		refdef.lightstyles  = 0;
		refdef.rdflags      = RDF_NOWORLDMODEL | RDF_NOCLEAR;
		refdef.height       += 4;

		re.RenderFrame(&refdef);
	}
}

#if 0
static qboolean IconOfSkinExists(const char *skin, char **tgafiles, int ntgafiles)
{
	int i;
	char scratch[1024];

	strcpy(scratch, skin);
	*strrchr(scratch, '.') = 0;
	strcat(scratch, ".tga");

	for (i = 0; i < ntgafiles; i++)
	{
		if (strcmp(tgafiles[i], scratch) == 0)
			return true;
	}

	return false;
}
#endif

static void PlayerConfigRelease(void)
{
	if (s_numplayermodels) {
		int i, j;
		for (i = 0; i < s_numplayermodels; i++) {
			for (j = 0; j < s_pmi[i].nskins; j++) {
				free(s_pmi[i].SkinDisplayNamesPtr[j]);
			}
			free(s_pmi[i].SkinDisplayNamesPtr);
		}
	}
	s_numplayermodels = 0;
}

static qboolean PlayerConfig_ScanDirectories(void)
{
	char scratch[1024];
	int ndirs = 0, npms = 0;
	int n = 0;
	const char **dirnames;
	const char **tgafiles;
	int i;

	PlayerConfigRelease();

	dirnames = FS_FindFiles("models/", "pl_*.def", &ndirs);
	if (!dirnames)
		return false;

	/*
	** go through the subdirectories
	*/
	npms = ndirs;
	if (npms > MAX_PLAYERMODELS)
		npms = MAX_PLAYERMODELS;

	for (i = 0; i < npms; i++) {
		int k;
		char *a, *b, *c;
		char **skinnames;
		int nskins = 0;
		if (dirnames[i] == 0)
			continue;

		// Fix the path.
		Com_sprintf(scratch, sizeof(scratch), "models/%s", COM_SkipPath(dirnames[i]));

		COM_StripExtension(COM_SkipPath(dirnames[i]), scratch);

		Com_sprintf(s_pmi[s_numplayermodels].directory, sizeof(s_pmi[s_numplayermodels].directory), "models/%s/", scratch);
		tgafiles = FS_FindFiles(s_pmi[s_numplayermodels].directory, "*.tga", &nskins);

		if (!nskins)
			continue;

		skinnames = (char **) calloc(1, (sizeof(char *) * (nskins + 1)));

		n = 0;
		for (k = 0; k< nskins; k++) {
			if (!strstr(tgafiles[k],"_dam.tga") && !strstr(tgafiles[k],"_ico.tga")) {
				COM_StripExtension(COM_SkipPath(tgafiles[k]), scratch);
				skinnames[n++] = strdup(scratch);
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = n;
		s_pmi[s_numplayermodels].SkinDisplayNamesPtr = skinnames;

		// make short name for the model
		a = (char *) strrchr(dirnames[i], '/');
		b = (char *) strrchr(dirnames[i], '\\');
		if (a > b)
			c = a;
		else
			c = b;

		COM_StripExtension(c + 1, scratch);
		strncpy(s_pmi[s_numplayermodels].displayname, scratch, MAX_DISPLAYNAME-1);

		s_numplayermodels++;

		if (tgafiles) {
			FS_FreeList(tgafiles, nskins);
		}
	}

	if (dirnames) {
		FS_FreeList(dirnames, ndirs);
	}
	return true;
}

void MENU_UpdateSkinList(void *self)
{
	int i, j, k;
	console3d_t *p = cls.mainconsole;
	item3d_t *item = (item3d_t *)self;
	char *list_name;
	char token[MAX_TOKEN_CHARS];
	char *s;

	s = item->localdata;
	strcpy(token, COM_Parse((const char **) &s));

#ifdef SIN_DEMO
	if (!strstr(item->list.itemnames[item->list.curvalue], "pl_blade"))
		CONSOLE_CenterPrint("jcx jcy boxtext \"\n\nNot Available in Demo\n\n\"");
	else
		CONSOLE_CenterPrint("");
#endif

	// Find the menu item that we need to update with the skins information.
	for (i = 0; i < p->numlevels; i++)
	{
		for (j = 0; j < MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			list_name = p->menu3d[i]->item[j]->list.name;

			if (p->menu3d[i]->item[j]->type == LIST3D && !strcmp(token,list_name))
			{
				p->menu3d[i]->item[j]->list.itemnames = s_pmi[item->list.curvalue].SkinDisplayNamesPtr;
				p->menu3d[i]->item[j]->list.numitems  = s_pmi[item->list.curvalue].nskins;
				p->menu3d[i]->item[j]->list.curvalue  = 0;

				Cvar_Set("con_playerskin", "0");

				// Set the skin based on the current value
				for (k = 0; k < p->menu3d[i]->item[j]->list.numitems; k++)
				{
					if (!strcmp(skin->string, p->menu3d[i]->item[j]->list.itemnames[k]))
					{
						p->menu3d[i]->item[j]->list.curvalue = k;
						Cvar_Set("con_playerskin", va("%i",k));
						return;
					}
				}
				return;
			}
		}
	}
}

static int pmicmpfnc(const void *_a, const void *_b)
{
	const playermodelinfo_s *a = (const playermodelinfo_s *) _a;
	const playermodelinfo_s *b = (const playermodelinfo_s *) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if (strcmp(a->directory, "male") == 0)
		return -1;
	else if (strcmp(b->directory, "male") == 0)
		return 1;

	if (strcmp(a->directory, "female") == 0)
		return -1;
	else if (strcmp(b->directory, "female") == 0)
		return 1;

	return strcmp(a->directory, b->directory);
}

// 2015 code
static void PlayerConfigBikesRelease(void)
{
	if (s_numbikemodels) {
		int i, j;
		for (i = 0; i < s_numbikemodels; i++) {
			for (j = 0; j < s_bikemi[i].nskins; j++) {
				free(s_bikemi[i].skindisplaynames[j]);
			}
			free(s_bikemi[i].skindisplaynames);
		}
	}
	s_numbikemodels = 0;
}

static qboolean PlayerConfig_ScanBikes(void)
{
	char scratch[1024];
	int ndirs = 0, npms = 0;
	int n = 0;
	const char **dirnames;
	const char **tgafiles;
	int i;

	PlayerConfigBikesRelease();
	dirnames = FS_FindFiles("models/", "bike_*.def", &ndirs);
	if (!dirnames)
		return false;

	/*
	** go through the subdirectories
	*/
	npms = ndirs;
	if (npms > MAX_BIKEMODELS)
		npms = MAX_BIKEMODELS;

	for (i = 0; i < npms; i++)
	{
		int k;
		char *a, *b, *c;
		char **skinnames;
		int nskins = 0;
		//int defhandle;

		if (dirnames[i] == 0)
			continue;

		// Fix the path.
		Com_sprintf(scratch, sizeof(scratch), "models/%s", COM_SkipPath(dirnames[i]));

		//defhandle = SINMDL_RegisterDef(scratch);
		//if (defhandle < 0)
		//	continue;

		COM_StripExtension(COM_SkipPath(dirnames[i]), scratch);

		Com_sprintf(s_bikemi[s_numbikemodels].directory, sizeof(s_bikemi[s_numbikemodels].directory), "models/%s/", scratch);
		tgafiles = FS_FindFiles(s_bikemi[s_numbikemodels].directory, "*.tga", &nskins);
		if (!nskins)
			continue;

		skinnames = (char **) calloc(1, (sizeof(char *) * (nskins + 1)));

		n = 0;
		for (k = 0; k < nskins; k++)
		{
			COM_StripExtension(COM_SkipPath(tgafiles[k]), scratch);
			skinnames[n++] = strdup(scratch);
		}

		// at this point we have a valid bike model
		s_bikemi[s_numbikemodels].nskins = n;
		s_bikemi[s_numbikemodels].skindisplaynames = skinnames;

		// make short name for the model
		a = (char *) strrchr(dirnames[i], '/');
		b = (char *) strrchr(dirnames[i], '\\');
		if (a > b)
			c = a;
		else
			c = b;

		COM_StripExtension(c + 1, scratch);
		strncpy(s_bikemi[s_numbikemodels].displayname, scratch, MAX_DISPLAYNAME-1);

		s_numbikemodels++;

		if (tgafiles)
			FS_FreeList(tgafiles, nskins);
	}

	if (dirnames)
		FS_FreeList(dirnames, ndirs);
	return true;
}

void MENU_UpdateBikeSkinList(void *self)
{
	int i, j, k;
	console3d_t *p = cls.mainconsole;
	item3d_t *item = (item3d_t *)self;
	char *list_name;
	char token[MAX_TOKEN_CHARS];
	char *s;

	s = item->localdata;
	strcpy(token, COM_Parse((const char **)&s));

	// Find the menu item that we need to update with the skins information.
	for (i = 0; i < p->numlevels; i++)
	{
		for (j = 0; j < MAX_MENU3D_ITEMS; j++)
		{
			if (!p->menu3d[i]->item[j])
				continue;

			if (!p->menu3d[i]->item[j]->menu_layout)
				continue;

			list_name = p->menu3d[i]->item[j]->list.name;

			if (p->menu3d[i]->item[j]->type == LIST3D && !strcmp(token,list_name))
			{
				p->menu3d[i]->item[j]->list.itemnames = s_bikemi[item->list.curvalue].skindisplaynames;
				p->menu3d[i]->item[j]->list.numitems  = s_bikemi[item->list.curvalue].nskins;
				p->menu3d[i]->item[j]->list.curvalue  = 0;

				Cvar_Set("con_bikeskin", "0");

				// Set the skin based on the current value
				for (k=0;k<p->menu3d[i]->item[j]->list.numitems;k++)
				{
					if (!strcmp(bikeskin->string, p->menu3d[i]->item[j]->list.itemnames[k]))
					{
						p->menu3d[i]->item[j]->list.curvalue = k;
						Cvar_Set("con_bikeskin", va("%i",k));
						return;
					}
				}
				return;
			}
		}
	}
}

static int bikemicmpfnc(const void *_a, const void *_b)
{
	const bikemodelinfo_s *a = (const bikemodelinfo_s *) _a;
	const bikemodelinfo_s *b = (const bikemodelinfo_s *) _b;

	// make sure our hoverbike model is first in the list
	if (!strcmp(a->directory, "bike_prototype"))
		return 1;
	else if (!strcmp(b->directory, "bike_prototype"))
		return -1;

	return strcmp(a->directory, b->directory);
}

void MENU_InitPlayerConfig(void)
{
	int i = 0;
#if 0
	char currentdirectory[1024];
	char currentskin[1024];
	int currentdirectoryindex = 0;
	int currentskinindex = 0;
#endif

	// Scan directory for models and skins
	PlayerConfig_ScanDirectories();

	qsort(s_pmi, s_numplayermodels, sizeof(s_pmi[0]), pmicmpfnc);

	memset(s_pmnames, 0, sizeof(s_pmnames));

	for (i = 0; i < s_numplayermodels; i++) {
		s_pmnames[i] = s_pmi[i].displayname;
		#if 0
		if (strcmp(s_pmi[i].directory, currentdirectory) == 0) {
			int j;
			currentdirectoryindex = i;
			for (j = 0; j < s_pmi[i].nskins; j++) {
				if (strcmp(s_pmi[i].SkinDisplayNamesPtr[j], currentskin) == 0) {
					currentskinindex = j;
					break;
				}
			}
		}
		#endif
	}

	// setup hover bikes
	PlayerConfig_ScanBikes();

	qsort(s_bikemi, s_numbikemodels, sizeof(s_bikemi[0]), bikemicmpfnc);

	memset(s_bikemnames, 0, sizeof(s_bikemnames));

	#if 0
	currentdirectoryindex = 0;
	currentskinindex = 0;
	#endif
	for (i = 0; i < s_numplayermodels; i++) {
		s_bikemnames[i] = s_bikemi[i].displayname;
		#if 0
		if (strcmp(s_bikemi[i].directory, currentdirectory) == 0) {
			int j;
			currentdirectoryindex = i;
			for(j = 0; j < s_bikemi[i].nskins; j++) {
				if(strcmp(s_bikemi[i].skindisplaynames[j], currentskin) == 0) {
					currentskinindex = j;
					break;
				}
			}
		}
		#endif
	}
}

void MENU_ReleasePlayerConfig(void)
{
	PlayerConfigRelease();
	PlayerConfigBikesRelease();
}
