//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/ltstyle.h                                              $
// $Revision:: 2                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 12/27/00 7:31 PM                                               $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// Creates a lightstyle from a TGA


#ifndef __LTSTYLE_H__
#define __LTSTYLE_H__

#define MAX_LIGHTSTYLE_STRINGLENGTH 512

char *createLightstyleFromTGA (const char *filename);
extern qboolean LoadTGA(const char * name, byte ** pic, int *width, int *height, byte **palette, qboolean * hasalpha );


#endif // ltstyle.h
