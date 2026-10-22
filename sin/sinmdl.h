#ifndef SINMDL_H
#define SINMDL_H

/*
 * SINMDL — SiN skeletal model path (engine + ref_gl).
 * Server: .def / .sam bounds for gi.IsModel / CalculateBounds / Anim_*.
 * Client/ref: .def registers as mod_sinmdl; AABB draw until skeletal.
 */
#ifdef SIN_MP

#include "../game/q_shared.h"
#include "sin_game_types.h"

void	SINMDL_Init (void);
void	SINMDL_Shutdown (void);

/* Called when game registers a models/*.def via modelindex */
void	SINMDL_RegisterDefForIndex (int modelindex, const char *defpath);

qboolean	SINMDL_IsModel (int modelindex);
void	SINMDL_CalculateBounds (int modelindex, float scale, vec3_t mins, vec3_t maxs);

int		SINMDL_NumAnims (int modelindex);
int		SINMDL_NumSkins (int modelindex);
int		SINMDL_NumGroups (int modelindex);
sinmdl_cmd_t	*SINMDL_InitCommands (int modelindex);
sinmdl_cmd_t	*SINMDL_Frame_Commands (int modelindex, int animnum, int framenum);
float		SINMDL_Frame_Time (int modelindex, int animnum, int framenum);
void		SINMDL_Frame_Delta (int modelindex, int animnum, int framenum, vec3_t delta);

const char	*SINMDL_Anim_NameForNum (int modelindex, int animnum);
int		SINMDL_Anim_NumForName (int modelindex, const char *name);
int		SINMDL_Anim_Random (int modelindex, const char *name);
int		SINMDL_Anim_NumFrames (int modelindex, int animnum);
float	SINMDL_Anim_Time (int modelindex, int animnum);
void	SINMDL_Anim_Delta (int modelindex, int animnum, vec3_t delta);

qboolean	SINMDL_GetBoneInfo (int modelindex, const char *bonename,
				int *groupindex, int *tri_num, vec3_t orientation);
const char	*SINMDL_GetBoneGroupName (int modelindex, const char *bonename);
qboolean	SINMDL_GetBoneTransform (int modelindex, int groupindex, int tri_num,
				vec3_t orientation, int anim, int frame, float scale,
				vec3_t trans[3], vec3_t pos);
int		SINMDL_Group_NameToNum (int modelindex, const char *name);
const char	*SINMDL_Group_NumToName (int modelindex, int num);

#else

#define SINMDL_Init()
#define SINMDL_Shutdown()

#endif

#endif
