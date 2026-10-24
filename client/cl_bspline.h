//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/cl_bspline.h                                           $
// $Revision:: 2                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 12/27/00 7:48 PM                                               $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// Charlie Brown's uniform non-rational bspline class.
//

#ifndef __CL_BSPLINE_H__
#define __CL_BSPLINE_H__

typedef enum {
	SPLINE_NORMAL,
	SPLINE_LOOP,
	SPLINE_CLAMP
} splinetype_t;

typedef struct _BSpline {
	vec3_t *control_points;
	int num_control_points;
	splinetype_t curvetype;
} BSpline;

#define MAX_BSPLINES 64

#ifdef __cplusplus
extern "C" {
#endif
extern qboolean CL_CreateBSpline(int num, int numpoints, float *points);
extern void CL_GetBSplinePosition(int num, float u, float *x, float *y);
extern qboolean CL_BSplineExists(int num);
extern void CL_InitializeBSplines(void);
#ifdef __cplusplus
}
#endif

#endif /* __CL_BSPLINE_H__ */
