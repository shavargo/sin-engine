#ifndef STEAM_SERVER_H
#define STEAM_SERVER_H

#include "../qcommon/qcommon.h"

#ifdef SIN_MP

/* Opaque SteamID storage (64-bit). */
typedef unsigned long long sin_steamid_t;

void	SinSteam_InitServer (void);
void	SinSteam_ShutdownServer (void);

/* Called from SVC_GetChallenge when client sends SteamID + ticket (argc>=3 extras). */
qboolean SinSteam_StoreChallengeTicket (int challenge_index, sin_steamid_t steamid,
	const byte *ticket, int ticket_len);

/* Parse Cmd_Argv SteamID (decimal) + hex ticket; fills challenge fields via out params. */
qboolean SinSteam_ParseChallengeArgs (sin_steamid_t *out_steamid, byte *ticket, int ticket_cap,
	int *out_ticket_len);

/* Called from SVC_DirectConnect after challenge validates. */
qboolean SinSteam_BeginAuthSession (sin_steamid_t steamid, const byte *ticket, int ticket_len);
qboolean SinSteam_BeginAuthSessionLocal (void);

/* Cvar sv_steamauth 0/1 — when 1, require ticket path (Steam servers). */
extern cvar_t *sv_steamauth;

#else

#define SinSteam_InitServer()
#define SinSteam_ShutdownServer()

#endif /* SIN_MP */

#endif
