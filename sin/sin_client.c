/*
 * SiN MP client extras (Phase 2).
 * REA: CL_CMDS_ProcessCommands, CL_ParseConsoleStates, etc.
 */
#include "../qcommon/qcommon.h"
#include "sin_client.h"

#ifdef SIN_MP

void SinCL_Init (void)
{
	Com_Printf ("SinCL: client extras stub (Phase 2)\n");
}

void SinCL_ParseExtras (void)
{
	/* Hooked from CL_ParseServerMessage when SiN svc types appear. */
}

#endif
