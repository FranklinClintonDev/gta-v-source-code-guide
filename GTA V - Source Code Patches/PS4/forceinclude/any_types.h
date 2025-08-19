#ifdef __GNUG__
#define NULL	__null
#else
#define NULL	0
#endif

#define UNUSED_PARAM(x)

// So we can tell it's the new build system and not the VC6-based one.
#define BUILD_SYSTEM_VERSION	0x0200

// For compatibility with old code
#define IS_CONSOLE		__CONSOLE

#ifndef __SPUFRAG
#define __SPUFRAG 0
#endif

#define	__PACKEDDEBUG	1
#define	__SKIPDEBUGAUTH	1
#define	__USEDEBUGAUDIO	0
#define DEBUGAUDIO_ONLY(...)
#define DEBUGUNPACKED_ONLY(...)
