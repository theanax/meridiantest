// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * blakserv.h
 *
 */

#ifndef _BLAKSERV_H
#define _BLAKSERV_H

/* charlie: i moved the defines above the local includes, as some of
	them seem to be required in them, and since some of the include
	filenames clash with that of most compilers */

#define BLAKSERV_VERSION "2.1"

#define MAX_LOGIN_NAME 50
#define MAX_LOGIN_PASSWORD 32

#define INVALID_CLASSVAR -1
#define INVALID_PROPERTY -1
#define INVALID_OBJECT -1
#define INVALID_TAG -1
#define INVALID_DATA -1
#define INVALID_ID -1
#define INVALID_CLASS -1
#define INVALID_DSTR -1
#define NO_SUPERCLASS 0

#define MAX_DEPTH 800

#define BOF_VERSION 11

// enable constants such as M_PI from math.h
#define _USE_MATH_DEFINES

enum
{
   USER_CLASS = 1,
   USER_ENTER_GAME_MSG = 2,
   SESSION_ID_PARM = 3,
   SYSTEM_CLASS = 4,
   SYSTEM_PARM = 5,
   RECEIVE_CLIENT_MSG = 6,
   CLIENT_PARM = 7,
   GARBAGE_MSG = 8,
   LOADED_GAME_MSG = 9,
   CONSTRUCTOR_MSG = 10,
   FIND_USER_BY_STRING_MSG = 11,
   NUMBER_STUFF_PARM = 12,
   GARBAGE_DONE_MSG = 13,
   STRING_PARM = 14,
   SYSTEM_STRING_MSG = 15,
   USER_NAME_MSG = 16,
   IS_FIRST_TIME_MSG = 17,
   DELETE_MSG = 18,
   NEW_HOUR_MSG = 19,
   SYSTEM_ENTER_GAME_MSG = 20,
   NAME_PARM = 21,
   ADMIN_CLASS = 22,
   TIMER_PARM = 23,
   TYPE_PARM = 24,
   DM_CLASS = 25,
   CREATOR_CLASS = 26,
   SETTINGS_CLASS = 27,
   REALTIME_CLASS = 28,
   EVENTENGINE_CLASS = 29,
   ESCAPED_CONVICT_CLASS = 30,
   TEST_CLASS = 31,
   MAX_BUILTIN_CLASS = 31

   // To add other C-accessible KOD identifiers,
   // see the BLAKCOMP's table of BuiltinIds[].
   //
   // The compiler assumes those builtins before
   // it reads any KODBASE.TXT.
};

// Enum for object constants blakod can use to call built-in objects.
enum
{
   SYSTEM_OBJECT = 0,
   SETTINGS_OBJECT = 1,
   REALTIME_OBJECT = 2,
   EVENTENGINE_OBJECT = 3,
   MAX_BUILTIN_OBJECT = 3,
   NUM_BUILTIN_OBJECTS = 4
};

#define MAX_PROC_TIME 5000
#define TIMER_DELAY_WARN 2000

#define COMM_READ_TIMEOUT_MS 30000

#define LEN_MAX_CLIENT_MSG 6000

#define NO_WALL 0
#define MIN_WALL 1
#define MAX_WALL 1000
#define NO_WALK_FLOOR 999

#define MIN_DYNAMIC_RSC 1000000

#define CONFIG_FILE "blakserv.cfg"

/* these three get the date/time appended to them */
#define ACCOUNT_FILE_SAVE "accounts."
#define GAME_FILE_SAVE "gameuser."
#define STRING_FILE_SAVE "striings."
#define DYNAMIC_RSC_FILE_SAVE "dynarscs."

#define SAVE_CONTROL_FILE "lastsave.txt"

#define MOTD_FILE "motd.txt"
#define REGFORM_FILE "regform.txt"
#define NOTE_FILE "admnote.txt"
#define PROFANE_FILE "profane.txt"

#define DEBUG_FILE "debug.txt"
#define ERROR_FILE "error.txt"
#define LOG_FILE "log.txt"
#define GOD_FILE "god.txt"
#define ADMIN_FILE "admin.txt"
#define BROADCAST_FILE "broadcast.txt"

#define KODBASE_FILE "kodbase.txt"

#define PACKAGE_FILE "packages.txt"
#define SPROCKET_FILE "sprocket.dll"

#ifdef BLAK_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include "resource.h"
#include <crtdbg.h>
#include <io.h>
#include <process.h>
#include "mutex_windows.h"
#include "thdmsgqueue_windows.h"
#include "main_windows.h"
#endif  // BLAK_PLATFORM_WINDOWS

#ifdef BLAK_PLATFORM_LINUX
#include "linux-types.h"
#endif  // BLAK_PLATFORM_LINUX

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <ppl.h>

#include "btime.h"

#include "bool.h"
#include "rscload.h"
#include "bkod.h"
#include "crc.h"
#include "md5.h"
typedef union 
{
   int int_val;
   constant_type v;
} val_type;

typedef struct
{
   int value;
   int name_id; /* for call-by-name parm list only */
   char type; /* for normal c parms (not call by name) only */
} parm_node;

typedef struct
{
   unsigned short len;
   unsigned short crc16;
   unsigned short len_verify;
   unsigned char seqno; /* 0 = synched, 1-255 = game epoch */
   char data[LEN_MAX_CLIENT_MSG];
} client_msg;

typedef struct
{
   int object_id;
   int message_id;
   int year, month, day, hour, minute, second;
   val_type parm1, parm2, parm3, parm4;
} blakod_reg_callback;

/* in main.c */
extern DWORD main_thread_id;
void MainReloadGameData();
char * GetLastErrorStr();
#define WM_BLAK_MAIN_READ           (WM_APP + 4000)
#define WM_BLAK_MAIN_RECALIBRATE    (WM_APP + 4001)
#define WM_BLAK_MAIN_DELETE_ACCOUNT (WM_APP + 4002)
#define WM_BLAK_MAIN_VERIFIED_LOGIN (WM_APP + 4003)
#define WM_BLAK_MAIN_LOAD_GAME      (WM_APP + 4004)

#include "bof.h"

#include "config.h"

#include "stringinthash.h"
#include "intstringhash.h"

#include "geometry.h"

#include "blakres.h"
#include "channel.h"
#include "kodbase.h"
#include "message.h"
#include "class.h"
#include "object.h"
#include "list.h"
#include "loadkod.h"
#include "sendmsg.h"
#include "ccode.h"
#include "timer.h"
#include "account.h"
#include "user.h"
#include "system.h"
#include "loadrsc.h"
#include "loadgame.h"
#include "astar.h"
#include "roofile.h"
#include "roomdata.h"
#include "files.h"

#include "bufpool.h"
#include "admin.h"
#include "game.h"
#include "trysync.h"
#include "synched.h"
#include "resync.h"
#include "session.h"

#include "term.h"

#include "proto.h"
#include "commcli.h"
#include "parsecli.h"
#include "sprocket.h"
#include "bstring.h"
#include "admin.h"
#include "garbage.h"
#include "savegame.h"

#include "loadacco.h"
#include "saveacco.h"
#include "savestr.h"
#include "loadstr.h"
#include "nameid.h"
#include "time.h"
#include "dllist.h"
#include "motd.h"
#include "gamelock.h"
#include "apndfile.h"
#include "builtin.h"
#include "version.h"
#include "rscfile.h"
#include "systimer.h"
#include "memory.h"

#ifdef BLAK_PLATFORM_WINDOWS
#include "interface_windows.h"
#else
#include "interface_linux.h"
#endif

#include "intrlock.h"
#include "chanbuf.h"

#include "saveall.h"
#include "loadall.h"

#include "saversc.h"

#include "adminfn.h"

#include "async.h"

#ifdef BLAK_PLATFORM_WINDOWS
#include "async_windows.h"
#else
#include "async_linux.h"
#endif

#include "debug.h"

#include "admincons.h"

#include "table.h"

#include "maintenance.h"
#include "block.h"

#ifdef BLAK_PLATFORM_WINDOWS
#include "database.h"
#endif

#include "jansson.h"

#endif

