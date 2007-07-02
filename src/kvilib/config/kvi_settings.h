#ifndef _KVI_SETTINGS_H_
#define _KVI_SETTINGS_H_

//=============================================================================
//
//   File : kvi_settings.h
//   Creation date : Fri Mar 19 1999 05:21:13 CEST by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 1999-2001 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc. ,59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//=============================================================================

//=============================================================================
// This file contains compile time settings, either set by configure or
// non-customizable defaults
//
// Better do not touch this
//=============================================================================

#include <qglobal.h>

#if defined(_OS_WIN32_) || defined(Q_OS_WIN32) || defined(Q_OS_WIN32_)

	#define FEEL_LIKE_I_AM_COMPILING_UNDER_WINDOZE
	#define COMPILE_ON_WINDOWS
	#define WINVER 0x0501

	// Windows has no config.h

	#include "kvi_wincfg.h"

	#ifdef __KVILIB__
		#define KVILIB_API __declspec(dllexport)
	#else
		#define KVILIB_API __declspec(dllimport)
	#endif

	#ifdef __KVIRC__
		#define KVIRC_API __declspec(dllexport)
	#else
		#define KVIRC_API __declspec(dllimport)
	#endif

#else

	#ifdef KVIRC_EXTERNAL_MODULE
		// when compiling an external module
		// include the last configuration
		#include "kvi_configstatus.h"
	#else
		#ifdef HAVE_CONFIG_H
			#include "config.h"
		#else
			#error "You're missing the config.h file: you must run configure before running make!"
		#endif
	#endif

	#define KVILIB_API
	#define KVIRC_API

	#ifndef VERSION
		#define VERSION "?.?.?"
	#endif
	
	#ifndef BUILD_DATE
		#define BUILD_DATE "?"
	#endif
	
	#ifndef BUILD_FLAGS
		#define BUILD_FLAGS "?"
	#endif

#endif

#define KVI_VERSION VERSION
#define KVI_VERSION_BRANCH VERSION_BRANCH
#define KVI_BUILD_DATE BUILD_DATE
#define KVI_BUILD_FLAGS BUILD_FLAGS

#define KVI_RELEASE_NAME "Anomalies"

// We want _GNU_SOURCE features
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif


#if defined(__GNUC__)
	// gcc
	#if __GNUC__ >= 3
		#define KVI_PTR2MEMBER(__x) &__x
	#else
		#define KVI_PTR2MEMBER(__x) &(__x)
	#endif
#elif defined(COMPILE_ON_WINDOWS)
	// Visual C++
	#define KVI_PTR2MEMBER(__x) &__x
#elif defined(__SUNPRO_CC)
	// Sun Forte
	#define KVI_PTR2MEMBER(__x) (__x)
#else
	// default
	#define KVI_PTR2MEMBER(__x) &(__x)
#endif

#ifdef COMPILE_NO_X
	#ifndef COMPILE_NO_X_BELL
		#define COMPILE_NO_X_BELL
	#endif
#endif

/*
#if __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 >= 2)
	#define KVI_DEPRECATED __attribute__ ((deprecated))
#else
	#define KVI_DEPRECATED
#endif
*/
#define KVI_DEPRECATED

#ifdef COMPILE_USE_QT4
	#define debug qDebug
#endif


#endif //_KVI_SETTINGS_H_
