#ifndef SIN_CLIENT_H
#define SIN_CLIENT_H

#ifdef SIN_MP
/* Phase 2: SiN parse extras (CL_CMDS_*, console states) hook points. */
void	SinCL_Init (void);
void	SinCL_ParseExtras (void);
#else
#define SinCL_Init()
#define SinCL_ParseExtras()
#endif

#endif
