/*
platform.h - common platform-dependent function defines
Copyright (C) 2018 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#ifndef PLATFORM_H
#define PLATFORM_H

#include "common.h"
#include "system.h"
#include "defaults.h"

/*
==============================================================================

                       SYSTEM UTILS

==============================================================================
*/
double Platform_DoubleTime( void );
void Platform_Sleep( int msec );
void Platform_ShellExecute( const char *path, const char *parms );
void Platform_MessageBox( const char *title, const char *message, qboolean parentMainWindow );
// commented out, as this is an optional feature or maybe implemented in system API directly
// see system.c
// qboolean Sys_DebuggerPresent( void );

#ifdef __ANDROID__
const char *Android_GetAndroidID( void );
const char *Android_LoadID( void );
void Android_SaveID( const char *id );
#endif

/* 
============================================================================== 
 
			MOBILE API 
 
============================================================================== 
*/
void Platform_Vibrate( float life, char flags );
void*Platform_GetNativeObject( const char *name );

/* 
============================================================================== 
 
			INPUT 
 
============================================================================== 
*/
// Gamepad support
int Platform_JoyInit( int numjoy ); // returns number of connected gamepads, negative if error
// Text input
void Platform_EnableTextInput( qboolean enable );
// System events
void Platform_RunEvents( void );
// Mouse
void Platform_GetMousePos( int *x, int *y );
void Platform_SetMousePos( int x, int y );
void Platform_PreCreateMove( void );
// Clipboard
void Platform_GetClipboardText( char *buffer, size_t size );
void Platform_SetClipboardText( const char *buffer, size_t size );

#ifdef __ANDROID__
void Android_ShowMouse( qboolean show );
void Android_MouseMove( float *x, float *y );
#endif

/* 
============================================================================== 
 
			WINDOW MANAGEMENT 
 
============================================================================== 
*/
typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

typedef struct vidmode_s vidmode_t;

// Window
qboolean  R_Init_Video( const int type );
void      R_Free_Video( void );
qboolean  VID_SetMode( void );
rserr_t   R_ChangeDisplaySettings( int width, int height, qboolean fullscreen );
int       R_MaxVideoModes( void );
vidmode_t*R_GetVideoMode( int num );
void*     GL_GetProcAddress( const char *name ); // RenderAPI requirement
void      GL_UpdateSwapInterval( void );
int GL_SetAttribute( int attr, int val );
int GL_GetAttribute( int attr, int *val );
void GL_SwapBuffers( void );
void *SW_LockBuffer( void );
void SW_UnlockBuffer( void );
qboolean SW_CreateBuffer( int width, int height, uint *stride, uint *bpp, uint *r, uint *g, uint *b );

/*
==============================================================================

			AUDIO INPUT/OUTPUT

==============================================================================
*/
// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init( void );
int  SNDDMA_GetSoundtime( void );
void SNDDMA_Shutdown( void );
void SNDDMA_BeginPainting( void );
void SNDDMA_Submit( void );
void SNDDMA_Activate( qboolean active ); // pause audio
// void SNDDMA_PrintDeviceName( void ); // unused
// void SNDDMA_LockSound( void ); // unused
// void SNDDMA_UnlockSound( void ); // unused

#endif // PLATFORM_H
