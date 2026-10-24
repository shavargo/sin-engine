//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/cl_bspline.cpp                                         $
// $Revision:: 3                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 2/5/02 7:36 PM                                                 $
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

#include "client.h"
#include "cl_bspline.h"

static __inline BSpline *BSpline_New (void)
{
	BSpline *bspline = (BSpline *) malloc(sizeof(BSpline));
	assert(bspline);
	bspline->control_points = NULL;
	bspline->num_control_points = 0;
	bspline->curvetype = SPLINE_NORMAL;
	return bspline;
}

static __inline void BSpline_Clear (BSpline *bspline)
{
	if (bspline->control_points) {
		free (bspline->control_points);
		bspline->control_points = NULL;
	}
	bspline->num_control_points = 0;
}

static __inline void BSpline_Set (BSpline *bspline, vec3_t *control_points, int num_control_points, splinetype_t type)
{
	BSpline_Clear (bspline);
	bspline->control_points = control_points;
	bspline->num_control_points = num_control_points;
	bspline->curvetype = type;
}

#if 0 /* these are not used */
static __inline void BSpline_AppendControlPoint (BSpline *bspline, const vec3_t new_control_point)
{
	int idx; size_t sz;
	idx = bspline->num_control_points;
	bspline->num_control_points++;
	sz = bspline->num_control_points * sizeof(vec3_t);
	bspline->control_points = (vec3_t *) realloc(bspline->control_points, sz);
	assert(bspline->control_points);
	VectorCopy(new_control_point, bspline->control_points[idx]);
}

static __inline void BSpline_GetControlPoint (const BSpline *bspline, int id, vec3_t out)
{
	VectorCopy(bspline->control_points[id], out);
}

static __inline void BSpline_SetControlPoint (BSpline *bspline, int id, const vec3_t new_control_point)
{
	assert(id < bspline->num_control_points);
	VectorCopy(new_control_point, bspline->control_points[id]);
}

static __inline float BSpline_EndPoint (const BSpline *bspline)
{
	return bspline->num_control_points - 4.0f;
}
#endif /* #if 0 */

static void BSpline_EvalNormal (const BSpline *bspline, float u, vec3_t Output)
{
	int segment_id;
	float B[4];
	float tmp;
	float u_2;
	float u_3;

	segment_id = (int) u;
	if (segment_id < 0) {
		segment_id = 0;
	}
	if (segment_id > bspline->num_control_points - 4) {
		segment_id = bspline->num_control_points - 4;
	}
	u -= (float) segment_id;

	u_2 = u * u;
	u_3 = u * u_2;

	tmp = 1 - u;
	B[0] = (tmp * tmp * tmp) * (1.0f / 6.0f);
	B[1] = ( 3.0f * u_3 - 6.0f * u_2 + 4.0f) * (1.0f / 6.0f);
	B[2] = (-3.0f * u_3 + 3.0f * u_2 + 3.0f * u + 1) * (1.0f / 6.0f);
	B[3] = u_3 * (1.0f / 6.0f);

	Output[0] =
		bspline->control_points[0 + segment_id][0] * B[0] +
		bspline->control_points[1 + segment_id][0] * B[1] +
		bspline->control_points[2 + segment_id][0] * B[2] +
		bspline->control_points[3 + segment_id][0] * B[3];
	Output[1] =
		bspline->control_points[0 + segment_id][1] * B[0] +
		bspline->control_points[1 + segment_id][1] * B[1] +
		bspline->control_points[2 + segment_id][1] * B[2] +
		bspline->control_points[3 + segment_id][1] * B[3];
	Output[2] =
		bspline->control_points[0 + segment_id][2] * B[0] +
		bspline->control_points[1 + segment_id][2] * B[1] +
		bspline->control_points[2 + segment_id][2] * B[2] +
		bspline->control_points[3 + segment_id][2] * B[3];
}

static void BSpline_EvalLoop (const BSpline *bspline, float t, vec3_t Output)
{
	int segment_id;
	float B[4];
	float tmp;
	float u;
	float u_2;
	float u_3;
	int i;
	int j;

	segment_id = (int) floor(t);
	u = t - floor(t);

	segment_id %= bspline->num_control_points;
	if (segment_id < 0) {
		segment_id += bspline->num_control_points;
	}

	u_2 = u * u;
	u_3 = u * u_2;

	tmp = 1 - u;
	B[0] = (tmp * tmp * tmp) * (1.0f / 6.0f);
	B[1] = ( 3.0f * u_3 - 6.0f * u_2 + 4.0f) * (1.0f / 6.0f);
	B[2] = (-3.0f * u_3 + 3.0f * u_2 + 3.0f * u + 1) * (1.0f / 6.0f);
	B[3] = u_3 * (1.0f / 6.0f);

	Output[0] = 0;
	Output[1] = 0;
	Output[2] = 0;
	for (i = 0, j = segment_id; i < 4; i++, j++) {
		if (j >= bspline->num_control_points) {
			j -= bspline->num_control_points;
		}
		Output[0] = Output[0] + (bspline->control_points[j][0] * B[i]);
		Output[1] = Output[1] + (bspline->control_points[j][1] * B[i]);
		Output[2] = Output[2] + (bspline->control_points[j][2] * B[i]);
	}
}

static void BSpline_EvalClamp (const BSpline *bspline, float t, vec3_t Output)
{
	int segment_id;
	float B[4];
	float tmp;
	float u;
	float u_2;
	float u_3;
	int i;
	int j;

	segment_id = (int) floor(t);
	u = t - floor(t);

	u_2 = u * u;
	u_3 = u * u_2;

	tmp = 1 - u;
	B[0] = (tmp * tmp * tmp) * (1.0f / 6.0f);
	B[1] = ( 3.0f * u_3 - 6.0f * u_2 + 4.0f) * (1.0f / 6.0f);
	B[2] = (-3.0f * u_3 + 3.0f * u_2 + 3.0f * u + 1) * (1.0f / 6.0f);
	B[3] = u_3 * (1.0f / 6.0f);

	Output[0] = 0;
	Output[1] = 0;
	Output[2] = 0;
	for (i = 0; i < 4; i++, segment_id++) {
		j = segment_id;
		if (j < 0) {
			j = 0;
		} else if (j >= bspline->num_control_points) {
			j = bspline->num_control_points - 1;
		}
		Output[0] = Output[0] + (bspline->control_points[j][0] * B[i]);
		Output[1] = Output[1] + (bspline->control_points[j][1] * B[i]);
		Output[2] = Output[2] + (bspline->control_points[j][2] * B[i]);
	}
}

static void BSpline_Eval (const BSpline *bspline, float u, vec3_t Result)
{
	switch(bspline->curvetype) {
	default:
	case SPLINE_NORMAL:
		BSpline_EvalNormal(bspline, u, Result);
		break;
	case SPLINE_CLAMP:
		BSpline_EvalClamp(bspline, u, Result);
		break;
	case SPLINE_LOOP:
		BSpline_EvalLoop(bspline, u, Result);
		break;
	}
}


/**********************************************************************/

static BSpline *bsplineList[MAX_BSPLINES];

void CL_InitializeBSplines (void)
{
	int i;
	for (i = 0; i < MAX_BSPLINES; i++)
		bsplineList[i] = NULL;
}

qboolean CL_CreateBSpline (int num, int numpoints, float *points)
{
	vec3_t *veclist;
	BSpline *bspline;
	int i, j;

	if (num < 0 || num >= MAX_BSPLINES)
		return false;

	if (bsplineList[num])
		bspline = bsplineList[num];
	else {
		bspline = BSpline_New();
		bsplineList[num] = bspline;
	}

	veclist = (vec3_t *) malloc(numpoints * sizeof(vec3_t));
	assert(veclist);
	for (i = 0, j = 0; i < numpoints; i++, j += 2) {
		veclist[i][0] = points[j];
		veclist[i][1] = points[j + 1];
		veclist[i][2] = 0;
	}

	BSpline_Set(bspline, veclist, numpoints, SPLINE_LOOP);

	return true;
}

qboolean CL_BSplineExists (int num)
{
	if (num < 0 || num >= MAX_BSPLINES)
		return false;
	return (bsplineList[num] != NULL);
}

void CL_GetBSplinePosition (int num, float u, float *x, float *y)
{
	vec3_t vec;
	assert(num >= 0 && num < MAX_BSPLINES);
	assert(bsplineList[num] != NULL);
	BSpline_Eval(bsplineList[num], u, vec);
	*x = vec[0];
	*y = vec[1];
}
