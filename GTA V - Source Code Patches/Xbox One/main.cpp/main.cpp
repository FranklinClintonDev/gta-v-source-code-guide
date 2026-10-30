// 
// system/main.cpp
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
#include "file/file_config.h"

#include "stack.h"
#include "ipc.h"
#include "param.h"
#include "xtl.h"
#include "memory.h"
#include "namedpipe.h"
#include "bootmgr.h"
#include "task.h"
#include "threadtrace.h"
#include "exec.h"
#include "dependencyscheduler.h"

#include "atl/hashstring.h"
#include "bank/packet.h"
#include "diag/channel.h"
#include "diag/tracker_remote2.h"
#include "diag/output.h"
#include "file/remote.h"
#include "file/stream.h"
#include "file/tcpip.h"
#include "grcore/setup.h"
#include "paging/base.h"
#include "system/buddyallocator.h"
#include "system/cache.h"
#include "system/interlocked.h"
#include "system/exception.h"
#include "system/nelem.h"
#include "system/platform.h"
#include "system/stack.h"
#include "string/stringutil.h"
#include "system/timer.h"
#include "system/tmcommands.h"
#include "system/memvisualize.h"
#include "system/memmanager.h"
#include "system/simpleallocator.h"
#include "system/splitallocator.h"
#include "system/threadtype.h"
#include "vectormath/vectorutility.h"

#if RSG_ORBIS
#include <system_service.h>
#include <user_service.h>
#include <game_live_streaming.h>
#include <screenshot.h>
#include <video_recording.h>
#include <libsysmodule.h>
#include <libdbg.h>
#pragma comment(lib,"SceSystemService_stub_weak")
#pragma comment(lib,"SceUserService_stub_weak")
#pragma comment(lib,"SceScreenShot_stub_weak")
#pragma comment(lib,"SceVideoRecording_stub_weak")
#endif

#include <stdlib.h>

// Every project is expected to have a Main(). Even if "App" style Prologue()/Loop()/Epilogue() are to be called.
extern int Main();

#if __WIN32PC
#include <process.h>
#include <crtdbg.h>
#include <werapi.h>
#include "system/xtl.h"
#endif // __WIN32PC

#if __XENON
#include <crtdbg.h>
# if !__FINAL
# include <xbdm.h>
# pragma comment(lib,"xbdm.lib")
# endif

# if USE_D3D_DEBUG_LIBS || (defined(_DEBUG) && !HACK_GTA4)
# pragma comment(lib,"xapilibd.lib")
# elif __PROFILE || __BANK
# pragma comment(lib,"xapilibi.lib")
# elif !__DEV
# pragma comment(lib,"xapilib.lib")
// # pragma comment(lib,"d3d9ltcg.lib")
# else
# pragma comment(lib,"xapilibi.lib")
# endif

#endif

#if __PPU
#include <sys/crashdump.h>
#include <sys/process.h>
#include <sys/spu_initialize.h>
#include <sys/ppu_thread.h>
#include <sys/dbg.h>
#include <cell/sysmodule.h>
#include "system/replay.h"
#if !__FINAL
#pragma comment(lib,"sntuner")
#pragma comment(lib,"dbg")
#endif
#endif

#if RSG_ORBIS
#include <sdk_version.h>
#include <_kernel.h>
#endif

#if RAGE_TRACKING
NOSTRIP_PARAM(autoMemTrack, "Memvisualize, if running Automatically save a CSV Dump of all memory allocation after 60 frames of games.");
#endif // RAGE_TRACKING

namespace rage {

#if __PPU && !__FINAL
u64 g_DipSwitches;
#endif

#if !__FINAL || __FINAL_LOGGING
extern size_t g_sysWarnAbove;
bool g_EnableRfs = false;
#endif

#if EXCEPTION_HANDLING && __PS3
// This commandline option must be specified as @disabletmloadoptionscheck (not -disabletmloadoptionscheck).
// It must be on the real commandline, it cannot be in a response file.
PARAM(disabletmloadoptionscheck,"[startup] Disable PS3 TM Load Options check");
#endif

PARAM(memvisualize, "Enable MemVisualize");

NOSTRIP_FINAL_LOGGING_PARAM(logfile,"[startup] Specify logfile for all output (must supply name)");
NOSTRIP_FINAL_LOGGING_PARAM(logsize,"[startup] Specify maximum size per log file (in megabytes)");

#if HANG_DETECT_THREAD
PARAM(noHangDetectThread, "disable the hang detect thread");
PARAM(hangdetecttimeout, "Number of seconds until hang detection kicks in");
#endif

#if RSG_DURANGO
XPARAM(forceboothdd);
#endif

#if __WIN32
extern void SetThreadName(const char * name);
#endif

#if RSG_PC && !__NO_OUTPUT
PARAM(processinstance, "[startup] When running multiple instances of the process at once, specify a unique instance number to assign to each instance (i.e. -processinstance=2");
#endif

#if !__FINAL
PARAM(rageversion,"[startup] Display rage version this was compiled against, and then exit.");
#endif

#if __LTCG	// Avoid gratuitous errors when user Main doesn't return.
#pragma warning(disable:4702)
#endif

extern bool g_IsStaticInitializationFinished;

#if __PPU
extern sysMemAllocator* g_pResidualAllocator;
#endif

#if __XENON
sysMemAllocator* g_pXenonPoolAllocator;
#endif

#if ENABLE_DEBUG_HEAP
size_t g_sysExtraStreamingMemory = 0;
#endif

static int g_MainReturnValue = 0;


static bool CallMainOnce()
{
	g_MainReturnValue = Main();
	return false;
}

// Entry points for the main project.
bool (*g_pProjectPrologue)(void) = NULL;
bool (*g_pProjectMainOrDoOneLoop)(void) = CallMainOnce; // We default to calling Main() once.
void (*g_pProjectEpilogue)(void) = NULL;

/*
PURPOSE
	main with exception handling. It is called in main.
PARAMS
	none.
RETURNS
	none.
NOTES
*/

static bool ExceptMain_Prologue() 
{
	bool retVal = true;

	// Disable denormals for vector units on all platforms, which can hurt performance in rare cases.
	// (This only actually does anything on __WIN32PC, currently.)
	Vec::DisableDenormals();

	// Store size of data cache line in global, since it isn't known at compile-time for PC.
#if __WIN32PC
	g_DataCacheLineSize = GetDataCacheLineSize();
#endif

#if !__FINAL
	if (PARAM_rageversion.Get()) {
		Displayf("Compiled against rage release '%s'",RAGE_RELEASE_STRING);
		return false;
	}
#endif

	sysMemAllowResourceAlloc = true;

#if __WIN32PC
	// Set the multimedia timer resolution, which controls not only
	//	the multimedia timers, but also thread related functionality such
	//	as Sleep and WaitForSingleObject.
	timeBeginPeriod(1);
#endif

#if !__FINAL && __WIN32PC
	SetThreadName("[RAGE] Main Application Thread");
#endif

	if(g_pProjectPrologue)
	{
		retVal = g_pProjectPrologue();  // <--- Step in here
	}

	return retVal;
}

static bool ExceptMain_OneLoopIteration()
{
	bool retVal = true;

	Assert(g_pProjectMainOrDoOneLoop);
	retVal = g_pProjectMainOrDoOneLoop(); // <--- Step in here

	return retVal;
}

static void ExceptMain_Epilogue()
{
	if(g_pProjectEpilogue)
	{
		g_pProjectEpilogue();
	}

#if __WIN32PC
	// Restore the timer resolution, just to play nice -- will only happen
	//	for people that exit the game legally.
	timeEndPeriod(1);
#endif

	OUTPUT_ONLY(diagChannel::CloseLogFile());
}


#if !__FINAL || RSG_PC
static inline bool is_ws(char ch) { return ch==32 || ch==9 || ch==10 || ch==13; }

#ifndef GAMERUNNER_EXE
// The application to invoke to generate the response file containing the command line arguments
#define GAMERUNNER_EXE "agerunner.exe"
#endif


#if __WIN32PC
const int ARGV_HEAP_SIZE = 512*1024;
#else
const int ARGV_HEAP_SIZE = 8192;
#endif

static bool RecurseResponseFile(const char *respFile,int &argc,char **&argv,int maxArgs,char *argv_heap,int argv_heap_size) {
	fiStream *S = fiStream::Open(respFile,true);
	if (S) {
		if (S->Size() > argv_heap_size-1)
		{
			Quitf(ERR_DEFAULT,"Your response file %s is too long (%db). Max is %db", respFile, S->Size(), argv_heap_size);
		}
#if !(RSG_PC && __FINAL)	// no nested response files in final pc
		int offset = 
#endif	// !(RSG_PC && __FINAL)
		S->Read(argv_heap,argv_heap_size-1);
		S->Close();
		char *p = argv_heap;
		while (argc < maxArgs-1) {
			while (is_ws(*p)) {
				++p;
			}
			if (*p == '"') {
				argv[argc++] = ++p;
				while (*p && *p != '"') {
					++p;
				}
				if (*p) {
					*p++ = 0;
				}
				else {
					break;
				}
			}
			// Check for comments in response files
            else if(';' == *p || '#' == *p || ('/' == *p && '/' == p[1]))
            {
                while(*p && '\n' != *p)
                {
                    ++p;
                }

                if(!*p) break;
            }
			else if (*p) {
				argv[argc++] = p;
				while (*p && !is_ws(*p)) {
					++p;
				}
				if (*p)
					*p++ = 0;
#if !(RSG_PC && __FINAL)	// no nested response files in final pc
				if (*argv[argc-1]=='@') {
					--argc;
					RecurseResponseFile(argv[argc]+1,argc,argv,maxArgs,argv_heap + offset + 1,argv_heap_size - offset - 1);
				}
#endif	// !(RSG_PC && __FINAL)
				if (!*p)
					break;
			}
			else {
				break;
			}
		}
		return true;
	}
	else
		return false;
}

extern const char *g_DefaultResponseFile;

static void ResponseFile(int &argc,char** &argv) {
	const char *respFile = NULL;

	// Find a responsefile param
	if (argc >= 2)
	{
		for (int i = 0; i < argc; ++i)
		{
			const char* param = argv[i];
			
			if (param[0] == '@' && fiDevice::GetDevice(param+1)->GetAttributes(param+1) != FILE_ATTRIBUTE_INVALID)
			{
				respFile = param+1;
				break;  
			}
		}
	}
	
	// Default responsefile
	if (!respFile && g_DefaultResponseFile && fiDevice::GetDevice(g_DefaultResponseFile)->GetAttributes(g_DefaultResponseFile) != FILE_ATTRIBUTE_INVALID)
	{
		respFile = g_DefaultResponseFile;
	}
#if __PS3 || RSG_ORBIS
	char buf[256] = {0};
	if (respFile == NULL)
	{
		safecpy(buf, argv[0]);
		char* cptr = stristr(buf, "EBOOT.BIN");
		if (cptr)
		{
#if RSG_ORBIS
			safecpy(cptr, g_DefaultResponseFile, sizeof(buf) - ((u32)(cptr - buf)));
#else
			safecpy(cptr, g_DefaultResponseFile, sizeof(buf) - ((u32)cptr - (u32)buf));
#endif	// RSG_ORBIS

			if (fiDevice::GetDevice(buf)->GetAttributes(buf) != FILE_ATTRIBUTE_INVALID)
				respFile = buf;
		}
	}
#endif

	const int maxArgs = (ARGV_HEAP_SIZE/16);
	static char argv_heap[ARGV_HEAP_SIZE];
	static char *new_argv[maxArgs];

	if (respFile) {
		int oldArgc = argc;
		char **oldArgv = argv;
		new_argv[0] = argv[0];
		argv = new_argv;
		argc = 1;

		if (!RecurseResponseFile(respFile,argc,argv,maxArgs,argv_heap,ARGV_HEAP_SIZE)) {
			Errorf("Response file '%s' not found",respFile);
		}
		// Append any additional args past the response file.
		for (int i=1; i<oldArgc; i++)
			if (oldArgv[i][0] != '@' && argc<maxArgs-1)
				argv[argc++] = oldArgv[i];
		argv[argc] = 0;
	}
#if !(RSG_PC && __FINAL)	// no env var params in final pc
	else if (sysGetEnv("RAGE_COMMON_ARGS",argv_heap,sizeof(argv_heap))) {
		int oldArgc = argc;
		char **oldArgv = argv;
		new_argv[0] = argv[0];
		argv = new_argv;
		argc = 1;
		for (int i=1; i<oldArgc; i++)
			if (argc<maxArgs-1)
				argv[argc++] = oldArgv[i];
		char *ca = argv_heap;
		while ((ca = strtok(ca, " ")) != NULL) {
			if ( argc<maxArgs-1)
				argv[argc++] = ca;
			ca = NULL;
		}
		argv[argc] = 0;
	}
#endif	// !(RSG_PC && __FINAL)
}
#else
static void ResponseFile(int&,char**) { }
#endif



#if __XENON
// PURPOSE:	Perform architecture-specific initialization
// NOTES:	On Xbox and Xenon, we manually parse command line from the debugger
static void ArchInit(int &argc,char** &argv) {
#if !__FINAL
	static DM_XBE xbe;
	DmGetXbeInfo(NULL, &xbe);
	DmMapDevkitDrive();
#endif

	// This requires Aug XeDK (731) or later.
	char *bp = GetCommandLine();
	bool first = true;
	if (bp)
	{
		const int maxArgs = 64;
		static char *argv_buffer[maxArgs];
		argv = argv_buffer;
#if __FINAL
		argv[argc++] = "A:\\default.xex";
#else
		argv[argc++] = xbe.LaunchPath;
#endif
		while (*bp) {
			char *start;
			while (*bp ==32 || *bp == 9) {
				bp++;
			}

			if (*bp == '"') {
				start = ++bp;
				while (*bp && *bp != '"') {
					bp++;
				}
			}
			else {
				start = bp;
				while (*bp && *bp != 32 && *bp != 9) {
					bp++;
				}
			}
			// On Xenon, command line contains the exe name *but* it's not a fully qualified
			// path (which we prefer) so get argv[0] from LaunchData and just skip the first
			// parameter
			if (!first) {
				argv[argc++] = start;
			}
			else {
				first = false;
			}
			if (*bp) {
				*bp++ = 0;
			}
		}

		argv[argc] = 0;
		Assert(argc < maxArgs);
	}
}
#elif __GFWL
static void ArchInit(int &,char** &) {

	// Initialize Live on Windows
    XLIVE_INITIALIZE_INFO xii  = {0};
    xii.cbSize  = sizeof( xii );
    xii.pD3D    = NULL;
    xii.pD3DPP  = NULL;
    xii.langID  = GetUserDefaultLangID();

    AssertVerify(S_OK == XLiveInitialize( &xii ));

}

#elif __PSP2 || RSG_ORBIS

SceUserServiceUserId g_initialUserId;
SceUserServiceUserId g_UserIds[SCE_USER_SERVICE_MAX_LOGIN_USERS] = { SCE_USER_SERVICE_USER_ID_INVALID, SCE_USER_SERVICE_USER_ID_INVALID, SCE_USER_SERVICE_USER_ID_INVALID, SCE_USER_SERVICE_USER_ID_INVALID };

static void ArchInit(int &,char ** &) 
{
	int ret = sceUserServiceInitialize(NULL);
	Assertf(ret == SCE_OK, "sceUserServiceInitialize: %d", ret);

	ret = sceUserServiceGetInitialUser(&g_initialUserId);
	Assertf(ret == SCE_OK, "sceUserServiceGetInitialUser: %d", ret);

	SceUserServiceLoginUserIdList userList;
	memset(&userList, 0, sizeof(userList));
	ret = sceUserServiceGetLoginUserIdList(&userList);
	Assertf(ret == SCE_OK, "sceUserServiceGetLoginUserIdList: %d", ret);

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
	{
		g_UserIds[i] = userList.userId[i];
	}
}

#elif __WIN32PC || RSG_DURANGO

static void ArchInit(int &,char** &) {
	// We used to unmask signalling NaNs here, but that is not early enough
	// if a globally constructed object generated a NaN.  In that case, the
	// error wouldn't show up until the NEXT floating point operation took
	// place, often much later in user-level code.

	// See the unmask_signalling_nans object at the end of this file.  We use
	// a non-portable method to mark the object for construction before ANY
	// other user-level global objects.
}

#elif __PPU

#define MAIN_STACK_SIZE		(256*1024)

// Establish initial priority and stack size.
#if EXCEPTION_HANDLING
#define EXCEPTION_STACK_SIZE		65536
SYS_PROCESS_PARAM(2048,EXCEPTION_STACK_SIZE);
#else
#define EXCEPTION_STACK_SIZE		0
SYS_PROCESS_PARAM(2048,MAIN_STACK_SIZE);
#endif

// Enable libcrashdump.
//
// Note that this needs to be after the SYS_PROCESS_PARAM (Sony requirement).
//
// We don't want to enable this macro in non-final builds, since it will cause
// our boot time check that coredumps are enabled, to always think they are,
// even if only a mini-coredump will be generated.
//
#if __FINAL
SYS_PROCESS_CRASH_DUMP_ENABLED(0,0)
#endif

// If we want to use the user log areas (sys_crash_dump_set_user_log_area), then
// we should force the lib to be linked in here.
//
//#pragma comment(lib,"crashdump_stub")


extern "C" int snTunerInit();

void ArchInit(int,char**) {
	// https://ps3.scedev.net/forums/nodejump/96481
	if (!__sys_process_param.size) return;

	sys_spu_initialize(6,0); 
}


extern "C" int CommonMain(int,char**);

#if EXCEPTION_HANDLING

static int common_argc;
static char **common_argv;

void WrapCommonMainEvenMore(void*) {
	g_CurrentThreadName[0] = '\0';
	CommonMain(common_argc,common_argv);
}

int WrapCommonMain(int argc,char **argv) {
#if !__FINAL
	snTunerInit();
#endif
	common_argc = argc;
	common_argv = argv;

	sysThreadType::AddCurrentThreadType(sysThreadType::THREAD_TYPE_PROCESS_MAIN);

	sysIpcWaitThreadExit(sysIpcCreateThread(WrapCommonMainEvenMore,NULL,MAIN_STACK_SIZE,PRIO_NORMAL,"[RAGE] Main Thread", 0, "update"));
	if (sysException::IsEnabled()) {
		sys_dbg_unregister_ppu_exception_handler();
		sys_dbg_finalize_ppu_exception_handler();

#if HANG_DETECT_THREAD
		if (s_HangDetectThreadId != sysIpcThreadIdInvalid) {	
			sysHangDetectExit();
			sysIpcWaitThreadExit(s_HangDetectThreadId);
			s_HangDetectThreadId = sysIpcThreadIdInvalid;
		}
#endif // HANG_DETECT_THREAD
	}

	return 0;
}
#else	// EXCEPTION_HANDLING
int WrapCommonMain(int argc,char **argv) { 
#if !__FINAL
	snTunerInit();
#endif
	return CommonMain(argc,argv);
}
#endif	// EXCEPTION_HANDLING

#endif

PARAM(breakonalloc,"[startup] Break on alloc ordinal value - value in {braces} in debug output (use =0 first to stabilize allocations)");
PARAM(breakonaddr,"[startup] Break on alloc address (in hex; stop when operator rage_new would have returned this address)");
// PARAM(dumpleaks,"[startup] Dump all leaks to specified filename (default is c:\\leaks.txt)");
PARAM(logmemory,"[startup] Log all memory allocation and deallocations to file (default is c:\\logmemory.csv)");
PARAM(logallocstack,"[startup] Log all memory allocation stack tracebacks (requires -logmemory)");
PARAM(warnabove,"[startup] Warn about any allocations at or above this size (in kilobytes)");

NOSTRIP_PARAM(localhost,"[startup] Specify local host of PC for rag connection (automatically set by psnrun.exe)");

#if __ENABLE_RFS && (__PPU || __XENON)
// This thread function will consume the PING commands received from the Remote Server so 
// that our network receive queues do not fill up and degrade performance.
// Pings are sent every two seconds, so wake up every ten seconds to clean them up.
static void DrainRfs( void* /*data*/ )
{
	while ( true )
	{
		char dummy[16];
		// Don't clog things up even worse if we know it's not going to work.
		if (!fiIsShowingMessageBox)
			fiRemoteGetEnv( "DON'T_CARE", dummy, sizeof(dummy) );
		sysIpcSleep( 10000 );
	}
}
#endif

#if !__TOOL
sysIpcCurrentThreadId g_MainThreadId = NULL;
#endif


extern __THREAD int g_DisableInitNan;
extern int g_KnownRefPoolSize;


bool CommonMain_Prologue(int argc,char **argv)
{
	g_IsStaticInitializationFinished = true;

#if RSG_PC && (!defined(__RGSC_DLL) || !__RGSC_DLL)
	WerSetFlags(WER_FAULT_REPORTING_FLAG_NOHEAP);
#endif // RSG_PC

	// Globally constructed arrays (which otherwise would always be guaranteed to be initialized to zero)
	// will not be filled with NaN's since that would be inconsistent.
	g_DisableInitNan--;

	sysIpcSetCurrentThreadId();

	// Init stack range for the main thread, so sysIpcIsStackAddress() works correctly
	extern void sysIpcComputeStackRangeMainThread();
	sysIpcComputeStackRangeMainThread();

#if !__PPU // The PPU sets this on the main thread in WrapCommonMain, then spawns a child thread which calls this function
	sysThreadType::AddCurrentThreadType(sysThreadType::THREAD_TYPE_PROCESS_MAIN);
#endif

	sysThreadType::AddCurrentThreadType(sysThreadType::THREAD_TYPE_UPDATE); // This is the update thread, until we are told otherwise
#if !__TOOL
	g_MainThreadId = sysIpcGetCurrentThreadId();
#endif

	ArchInit(argc,argv);

#if __PPU && !__FINAL
	sys_ppu_thread_stack_t si;
	sys_ppu_thread_get_stack_information(&si);
	if (si.pst_size < MAIN_STACK_SIZE)
		Quitf("Stack size is wrong, please make sure connection settings in TM for Load Options has Use Stack Size from Elf checked.");
#endif

	fiDeviceTcpIp::InitClass(argc,argv);

#if (__XENON || __PPU || __PSP2 || RSG_DURANGO || RSG_ORBIS) && __ENABLE_RFS
	char addr[32] = {0};

	fiStream *S = fiStream::Open(RSG_ORBIS? "rfs.dat" : __PPU || __PSP2? "c:\\rfs.dat" : RSG_DURANGO? "G:\\RFS.DAT" : "D:\\RFS.DAT",fiDeviceLocal::GetInstance());
	if (S) 
	{
		int i = 0, ch;
		// hack so that ipconfig | grep "IP Address" > c:\rfs.dat will work :-)
		while ((ch=S->GetCh()) != -1 && i < (int)sizeof(addr)-1)
			if ((i && ch=='.') || (ch>='0'&&ch<='9'))
				addr[i++] = (char) ch;
		addr[i] = 0;
		S->Close();
	}
	else {
#if __PPU || __PSP2 || RSG_ORBIS
		printf("***** CommonMain: c:\\rfs.dat not found!\n");
#elif RSG_DURANGO
		OutputDebugString("***** CommonMain: RFS.DAT not found!\n");
		OutputDebugString("Use xbcp c:\\rfs.dat {packagename}:\rfs.dat\n");
		OutputDebugString("Use xbdir xf:\\apps to display installed packages.\n");
		OutputDebugString("GTA-V_1.0.0.0_x64__zjr0dfhgjwvde is an example of a package name.\n");
#else
		OutputDebugString("***** CommonMain: D:\\RFS.DAT not found!\n");
#endif
	}

	if(strlen(addr))
	{
		fiDeviceTcpIp::SetLocalHost(addr);
	}

#endif // (__XENON || __PPU || __PSP2) && __ENABLE_RFS

#if __XENON && __ENABLE_RFS
	// Unfortunately we have to duplicate some logic from sysBootManager here because we normally
	// need to start up rfs before we've processed command line parameters so that response files will work.
	
	// If we use -localhost, RFS is fine.
	bool hasLocalhost = false;

	for (int i=1; i<argc; i++) {
		if (!strnicmp(argv[i],"-localhost", 10)) {
			hasLocalhost = true;
			break;
		}
	}

	if (!hasLocalhost) {
		if (argv[0][0]=='A')
			g_EnableRfs = false;	// Did the exe boot from the A: drive?  If so, it's a disc, no rfs.
		else {
			for (int i=1; i<argc; i++)
				if (!stricmp(argv[i],"-forcebootcd"))
					g_EnableRfs = false;
		}
	}
	if (!g_EnableRfs)
		OutputDebugString("Disabling RFS, -forcebootcd or boot from disc detected - use -localhost=<ip> if you need asserts or local file I/O\n");
	else 
		::rage::fiDeviceRemote::InitClass(argc,argv);
#elif (__PPU || __PSP2 || RSG_DURANGO || RSG_ORBIS) && __ENABLE_RFS
	// Need similar "booted from disc" detection logic here.
	if (g_EnableRfs)
		::rage::fiDeviceRemote::InitClass(argc,argv);
#endif

	// Initialize the program name before trying to read the response file. This way if we're running
	// from /app_home/PS3_GAME/USRDIR/EBOOT.bin then the program name will be initialized and we'll know
	// we need to adjust the game: path
	sysParam::sm_ProgName = argc && argv? argv[0] : "unknown";

	ResponseFile(argc,argv);

	sysParam::Init(argc,argv);
	
	// Disable this in the Social Club DLL as it crashes the game otherwise
#if RAGE_TRACKING && !(defined(__RGSC_DLL) && __RGSC_DLL)	
	const char* pszData = NULL;
	if (PARAM_memvisualize.Get(pszData))
	{
		// Finally you can put @memvisualize in a command TXT file!
		sysMemVisualize& memVisualize = sysMemVisualize::GetInstance();
		int memVisualizePort = 0;

		// Customize memvisualize
		if (*pszData == ':')
		{
			// Yes.
			char portNumberStr[32];
			int count = sizeof(portNumberStr);
			char *portPtr = portNumberStr;
			pszData++;

			while (*pszData >= '0' && *pszData <= '9' && --count)
			{
				*(portPtr++) = *(pszData++);
			}

			*portPtr = 0;
			memVisualizePort = atoi(portNumberStr);
		}

		if (strlen(pszData) > 0)
		{
			memVisualize.Set(pszData);
		}
		else
		{
			memVisualize.Set("adfghnpru");
		}

		// HACK: There is a problem with the multi-allocator across different threads on Durango
#if !RSG_DURANGO
		USE_DEBUG_MEMORY();
#endif
		++g_TrackerDepth;
		diagTracker* t = rage_new diagTrackerRemote2(false, memVisualizePort);
		sysMemAllocator& master = sysMemAllocator::GetMaster();

		if (memVisualize.HasGame() || memVisualize.HasMisc())
		{
			sysMemSimpleAllocator* pSimple = dynamic_cast<sysMemSimpleAllocator*>(master.GetAllocator(MEMTYPE_GAME_VIRTUAL));
			if (pSimple)
				t->InitHeap("Game Virtual", master.GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetHeapBase(), master.GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetHeapSize());
		}

		if (memVisualize.HasResource())
		{
#if RSG_DURANGO || RSG_ORBIS
			t->InitHeap("Resource", master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetHeapBase(), master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetHeapSize());
#else
			const sysMemAllocator* pResVirtAllocator = master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
			const sysMemGrowBuddyAllocator* pResVirtGrowBuddyAllocator = dynamic_cast<const sysMemGrowBuddyAllocator*>(pResVirtAllocator);
			if(pResVirtGrowBuddyAllocator)
			{
				pResVirtGrowBuddyAllocator->TrackExistingPages(*t);
			}
			else
			{
				t->InitHeap("Resource Virtual", pResVirtAllocator->GetHeapBase(), pResVirtAllocator->GetHeapSize());
			}

			const sysMemAllocator* pResPhysAllocator = master.GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
			const sysMemGrowBuddyAllocator* pResPhysGrowBuddyAllocator = dynamic_cast<const sysMemGrowBuddyAllocator*>(pResPhysAllocator);
			if(pResPhysGrowBuddyAllocator)
			{
				pResPhysGrowBuddyAllocator->TrackExistingPages(*t);
			}
			else
			{
				t->InitHeap("Resource Physical", pResPhysAllocator->GetHeapBase(), pResPhysAllocator->GetHeapSize());
			}
#endif
		}

		if (memVisualize.HasStreaming())
		{
			void* address = (void*) (0x10000000 ^ (size_t) master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetHeapBase());
			t->InitHeap("Streaming Virtual", address, master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetHeapSize());
		}

#if RSG_DURANGO || RSG_ORBIS || RSG_PC
		if (memVisualize.HasPlatform())
		{
			sysMemAllocator* pAllocator = sysMemManager::GetInstance().GetReplayAllocator();
			if (pAllocator)
				t->InitHeap("Replay Allocator", pAllocator->GetHeapBase(), pAllocator->GetHeapSize());
		}
#endif

#if __PPU
		// InitHeap() for g_pResidualAllocator will be performed on allocator creation, in grcDevice::InitClass

		if (memVisualize.HasPlatform())
		{
			sysMemSplitAllocator* pSplitAllocator = sysMemManager::GetInstance().GetFlexAllocator();
			if (pSplitAllocator)
			{
				sysMemAllocator* pAllocator = pSplitAllocator->GetPrimaryAllocator();
				if (pAllocator)
					t->InitHeap("Flex Allocator", pAllocator->GetHeapBase(), pAllocator->GetHeapSize());
			}
		}
#elif RSG_ORBIS
		if (memVisualize.HasPlatform())
		{
			sysMemAllocator* pAllocator = sysMemManager::GetInstance().GetFlexAllocator();
			if (pAllocator)
				t->InitHeap("Flex Allocator", pAllocator->GetHeapBase(), pAllocator->GetHeapSize());
		}		
#endif

#if ENABLE_DEBUG_HEAP
		// If there really is a distinct separate debug heap, add a tracker for it.
		if (g_sysHasDebugHeap && memVisualize.HasDebug())
		{
			t->InitHeap("Debug Heap", master.GetAllocator(MEMTYPE_DEBUG_VIRTUAL)->GetHeapBase(), master.GetAllocator(MEMTYPE_DEBUG_VIRTUAL)->GetHeapSize());
			t->InitHeap("Recorder Heap", MEMMANAGER.GetRecorderAllocator()->GetHeapBase(), MEMMANAGER.GetRecorderAllocator()->GetHeapSize());
		}
#endif // ENABLE_DEBUG_HEAP

		//if(memVisualize.HasReplay())
		{
			sysMemSimpleAllocator* pReplayAllocator = sysMemManager::GetInstance().GetReplayAllocator();
			if(pReplayAllocator)
				t->InitHeap("Replay Heap", pReplayAllocator->GetHeapBase(), pReplayAllocator->GetHeapSize());
		}
		
#if __XENON
		if (g_pXenonPoolAllocator && memVisualize.HasPlatform())
		{
			t->InitHeap("Pool Allocator", g_pXenonPoolAllocator->GetHeapBase(), g_pXenonPoolAllocator->GetHeapSize());
		}

		if (memVisualize.HasXTL())
		{
			t->InitHeap("XTL - Virtual 4KB", (void*) MM_VIRTUAL_4KB_BASE, MM_VIRTUAL_4KB_SIZE);
			t->InitHeap("XTL - Virtual 64KB", (void*) MM_VIRTUAL_64KB_BASE, MM_VIRTUAL_64KB_SIZE);
			t->InitHeap("XTL - Image 64KB", (void*) MM_IMAGE_64KB_BASE, MM_IMAGE_64KB_SIZE);
			t->InitHeap("XTL - Encrypted 64KB", (void*) MM_ENCRYPTED_64KB_BASE, MM_ENCRYPTED_64KB_SIZE);
			t->InitHeap("XTL - Image 4KB", (void*) MM_IMAGE_4KB_BASE, MM_IMAGE_4KB_SIZE);
			t->InitHeap("XTL - Physical 64KB", (void*) MM_PHYSICAL_64KB_BASE, MM_PHYSICAL_64KB_SIZE);
			t->InitHeap("XTL - Physical 16MB", (void*) MM_PHYSICAL_16MB_BASE, MM_PHYSICAL_16MB_SIZE);
			t->InitHeap("XTL - Physical 4KB", (void*) MM_PHYSICAL_4KB_BASE, MM_PHYSICAL_4KB_SIZE);
		}
#elif RSG_DURANGO
		if (memVisualize.HasXTL())
		{			
			// If MEM_GRAPHICS is not specified, VirtualAddress must be NULL or in the range 1 TB to 8 TB.
			t->InitHeap("XTL - 1-2 TB", (void*) MM_VIRTUAL_1TB_BASE, MM_VIRTUAL_1TB_SIZE);
			t->InitHeap("XTL - 2-3 TB", (void*) MM_VIRTUAL_2TB_BASE, MM_VIRTUAL_2TB_SIZE);
			t->InitHeap("XTL - 3-4 TB", (void*) MM_VIRTUAL_3TB_BASE, MM_VIRTUAL_3TB_SIZE);
			t->InitHeap("XTL - 4-5 TB", (void*) MM_VIRTUAL_4TB_BASE, MM_VIRTUAL_4TB_SIZE);
			t->InitHeap("XTL - 5-6 TB", (void*) MM_VIRTUAL_5TB_BASE, MM_VIRTUAL_5TB_SIZE);
			t->InitHeap("XTL - 6-7 TB", (void*) MM_VIRTUAL_6TB_BASE, MM_VIRTUAL_6TB_SIZE);
			t->InitHeap("XTL - 7-8 TB", (void*) MM_VIRTUAL_7TB_BASE, MM_VIRTUAL_7TB_SIZE);

			//If MEM_GRAPHICS is specified, then VirtualAddress must be NULL or in the range 4 GB to 1 TB.			
			t->InitHeap("XTL - Graphics", (void*) MM_VIRTUAL_GRAPHICS_BASE, MM_VIRTUAL_GRAPHICS_SIZE);
		}
#elif RSG_PC && !RSG_TOOL
		if (memVisualize.HasXTL())
		{
#if API_HOOKER
			Assertf(!USE_SPARSE_MEMORY, "You need to disable USE_SPARSE_MEMORY if you want to track hooked API functions with -memvisualize=x");
#endif
			t->InitHeap("XTL - A", (void*) MM_VIRTUAL_A_BASE, MM_VIRTUAL_A_SIZE);
			/*t->InitHeap("XTL - B", (void*) MM_VIRTUAL_B_BASE, MM_VIRTUAL_B_SIZE);
			t->InitHeap("XTL - C", (void*) MM_VIRTUAL_C_BASE, MM_VIRTUAL_C_SIZE);
			t->InitHeap("XTL - D", (void*) MM_VIRTUAL_D_BASE, MM_VIRTUAL_D_SIZE);
			t->InitHeap("XTL - E", (void*) MM_VIRTUAL_E_BASE, MM_VIRTUAL_E_SIZE);
			t->InitHeap("XTL - F", (void*) MM_VIRTUAL_F_BASE, MM_VIRTUAL_F_SIZE);
			t->InitHeap("XTL - G", (void*) MM_VIRTUAL_G_BASE, MM_VIRTUAL_G_SIZE);
			t->InitHeap("XTL - H", (void*) MM_VIRTUAL_H_BASE, MM_VIRTUAL_H_SIZE);
			t->InitHeap("XTL - Remainder", (void*) MM_VIRTUAL_REMAINDER_BASE, MM_VIRTUAL_REMAINDER_SIZE);*/
		}
#endif // __XENON

		if (!memVisualize.HasXTL())
		{
			t->Push("Tracker Send Buffer");
			t->Tally(t,sizeof(diagTrackerRemote2),0);
			t->Pop();
		}

		diagTracker::SetCurrent(t);
		--g_TrackerDepth;
	}
#endif

#if __XENON && __ENABLE_RFS
	if (!fiRemoteServerIsRunning)
	{
		// Let's try this again - maybe we need the response file to figure out
		// where to connect to.
		const char *localhost = NULL;
		if (PARAM_localhost.Get(localhost))
		{
			OutputDebugString("-localhost specified, trying to connect to RFS again");
			fiDeviceTcpIp::SetLocalHost(localhost);
			::rage::fiDeviceRemote::InitClass(argc,argv);
		}
	}
#endif // __XENON && __ENABLE_RFS

	// Check for multiple instances of the game running in PC, non-final game builds
#if RSG_PC && !__FINAL && !__TOOL && !__GAMETOOL && !__RESOURCECOMPILER && !(defined(__RGSC_DLL) && __RGSC_DLL)	
	if(!PARAM_processinstance.Get())
	{
		ASSERT_ONLY(HANDLE hMutex = CreateMutexA(NULL, TRUE, "Game_PC_SingleInstanceMutex");)
		Assertf(GetLastError() != ERROR_ALREADY_EXISTS, "You are attempting to run two instances of the game.\n"
			"Please ensure that other instances of the game are closed through Windows Task Manager!\n\n"
			"If you are deliberately trying to run two instances of the game, use the -processinstance command line argument to avoid this assert.");
		Assertf(hMutex, "Failed to create single-instance mutex!");
	}
#endif

	// Open log file before connecting to R*TM, so the log contains the result of the connect attempt
#if !__NO_OUTPUT
	u32 logSizeMB = 0U;
	if(PARAM_logsize.Get(logSizeMB))
		diagChannel::SetLogFileMaxSize(logSizeMB);

	const char *logfile = NULL;
	PARAM_logfile.Get(logfile);
	if (logfile && logfile[0])
		diagChannel::OpenLogFile(logfile);
#endif

#if SYSTMCMD_ENABLE
	sysTmInit(PARAM_rockstartargetmanager.Get());
#endif // SYSTMCMD_ENABLE

	// On Durango, disabled OutputDebugString if no debugger is present. This prevents QA from
	// starting R*TM after the game has booted to caputre TTY - which causes slowdowns and audio
	// stutters due to kernel debugger.
#if RSG_DURANGO && SYSTMCMD_ENABLE
	extern bool s_allowOutputDebugString;
	s_allowOutputDebugString = sysBootManager::IsDebuggerPresent();
#endif

#if EXCEPTION_HANDLING
	sysException::Init();
#endif  //EXCEPTION_HANDLING

#if BREAKPAD_ENABLED
	sysException::SetupBreakpad();
#endif

#if BACKTRACE_DUMP_AND_UPLOAD_ENABLED
	sysException::SetupBacktraceDumpAndUpload();
#endif


#if HANG_DETECT_THREAD
	PARAM_hangdetecttimeout.Get(sysHangDetectCountdown);

	if (!PARAM_noHangDetectThread.Get() && !sysBootManager::IsDebuggerPresent())
	{
		sysThreadTraceInit();
		sysHangDetectStartThread();
	}

#endif // HANG_DETECT_THREAD

#if !__FINAL

	if (PARAM_warnabove.Get()) {
		int dummy = 0;
		PARAM_warnabove.Get(dummy);
		g_sysWarnAbove = dummy << 10;
	}
	else	// Turn it off completely
		g_sysWarnAbove = ~0U;
#endif

	OUTPUT_ONLY(diagChannel::InitClass());

	sysBootManager::InitClass();

#if !__TOOL
	pgBase::InitClass(g_KnownRefPoolSize);
#endif


	const char *localhost;
	if (PARAM_localhost.Get(localhost))
	{
		fiDeviceTcpIp::SetLocalHost(localhost);
	}

#if !__FINAL
#if __PPU || __XENON
	if (fiRemoteServerIsRunning)
        sysIpcCreateThread( DrainRfs, NULL, 16384, PRIO_HIGHEST, "[RAGE] Drain RFS" ); 
#endif
#endif


#if !__FINAL
	if (PARAM_breakonalloc.Get()) {
		int allocId;
		PARAM_breakonalloc.Get(allocId);
		sysMemAllocator::GetCurrent().SetBreakOnAlloc(allocId);
#if __TOOL && __WIN32 && !__OPTIMIZED
		_crtBreakAlloc = allocId;
#endif
	}
	if (PARAM_breakonaddr.Get()) {
		const char *hexString = NULL;
		PARAM_breakonaddr.Get(hexString);
		if (hexString) {
			// Don't trust strtoul to get this right (.Net 2003 version does though)
			if (hexString[1]=='x'||hexString[1]=='X')
				hexString+=2;
			sysMemAllocator::GetCurrent().SetBreakOnAddr((void*)strtoul(hexString,NULL,16));
		}
	}
#endif

#if !__FINAL
	sysStack::InitClass(argv[0]);
#endif // !__FINAL

	atHashStringNamespaceSupport::InitNamespaces();

#if RAGE_TRACKING
	if ( diagTracker::GetCurrent() )
		dynamic_cast< diagTrackerRemote2* >( diagTracker::GetCurrent() )->StartNetworkWorker();
#endif

	// Now done in heap startup code
	// sysMemAllocator::GetCurrent().BeginLayer();

	sysTaskManager::InitClass();
	sysDependencyScheduler::InitClass();

	const char *logmemory = "c:/logmemory.csv";
	PARAM_logmemory.GetParameter(logmemory);

	if (PARAM_logmemory.Get())
		sysMemAllocator::GetCurrent().BeginMemoryLog(logmemory,PARAM_logallocstack.Get());

#if RSG_ORBIS
#if !__FINAL && 0
	// Disable live streaming in non-final
	int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);
	if(ret == SCE_OK)
	{
		ret = sceGameLiveStreamingInitialize(SCE_GAME_LIVE_STREAMING_HEAP_SIZE);
		if(ret == SCE_OK)
		{
			ret = sceGameLiveStreamingPermitLiveStreaming(false);
		}
	}
	if(ret != SCE_OK)
	{
		diagLoggedPrintf("WARNING: Failed to disable live streaming! Error code %x\n", ret);
	}

	// Disable screenshots
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_SCREEN_SHOT);
	if(ret == SCE_OK)
	{
		ret = sceScreenShotDisable();
	}
	if(ret != SCE_OK)
	{
		diagLoggedPrintf("WARNING: Failed to disable screenshots! Error code %x\n", ret);
	}

	// Disable video recording
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_VIDEO_RECORDING);
	if(ret == SCE_OK)
	{
		int32_t videoInfo = SCE_VIDEO_RECORDING_CHAPTER_PROHIBIT;
		ret = sceVideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_CHAPTER, &videoInfo, sizeof(videoInfo));
	}
	if(ret != SCE_OK)
	{
		diagLoggedPrintf("WARNING: Failed to disable video recording! Error code %x\n", ret);
	}
#endif // !__FINAL
#endif // RSG_ORBIS
	
	return ExceptMain_Prologue();
}

bool CommonMain_OneLoopIteration()
{
	return ExceptMain_OneLoopIteration();
}

void CommonMain_Epilogue()
{
	ExceptMain_Epilogue();

	if (PARAM_logmemory.Get())
		sysMemAllocator::GetCurrent().EndMemoryLog();

	sysDependencyScheduler::ShutdownClass();
	sysTaskManager::ShutdownClass();

#if RAGE_TRACKING
	if (diagTracker::GetCurrent()) {
		diagTracker *old = diagTracker::GetCurrent();
		diagTracker::SetCurrent(NULL);
		delete old;
	}
#endif

	pgBase::ShutdownClass();

	sysBootManager::ShutdownClass();

#if (__XENON || __PPU) && !__FINAL
	if (g_EnableRfs)
		::rage::fiDeviceRemote::ShutdownClass();
#endif

	fiDeviceTcpIp::ShutdownClass();

#if !__FINAL
	unsigned current, maxi;
	if (sysIpcEstimateStackUsage(current,maxi))
		Displayf("Main thread used %u of %u bytes stack space.",current,maxi);

	sysStack::ShutdownClass();
#endif

	atHashStringNamespaceSupport::ShutdownNamespaces();

	DIAG_CONTEXT_CLEANUP();

#if __PPU
	sys_process_exit(0);
#endif
}

#if __FINAL_LOGGING
int CommonMain(int argc, char **argv) 
#else
int CommonMain(int argc, char **argv) 
#endif
{
#if RSG_ORBIS
#if __FINAL
	argv[0] = (char*)"/app0/eboot.bin";
#else
	SceKernelModuleInfo modinfo;
	modinfo.size = sizeof(SceKernelModuleInfo);
	sceKernelGetModuleInfo(0,&modinfo);
	char executablePath[256];
	if(!stricmp(modinfo.name,"eboot.bin"))
	{
		argv[0] = (char*)"/app0/eboot.bin";
	}
	else
	{
		// NB: While it's invalid to call any debug functions on a test kit, a test kit will always
		// be a deployed build, and will therefore always fall into the above if(). If it's not,
		// then things are going to break anyway...
		sceDbgGetExecutablePath(executablePath, sizeof(executablePath));
		if(strncmp(executablePath, "/host/", 6) == 0)
		{
			argv[0] = executablePath+6;
		}
		else
		{
			argv[0] = executablePath;
		}
	}
#endif	// __FINAL
	sceSystemServiceHideSplashScreen();
#endif	// RSG_ORBIS

#if RSG_PC && !__FINAL
	if(IsDebuggerPresent())
	{
		SetEnvironmentVariableA("_NO_DEBUG_HEAP", "1");
	}
#endif

#if __PPU
	grcSetup::InitSysUtilCallback();			//set this up as early as possible
#endif //__PPU

#if RSG_DURANGO
	static const char* bakedArgs[] =
	{
		"-forceboothdd",
		"-DoReleaseStartup",
		"-sc_noPrologueCheckForMP",
		"-net_all=debug3",
		"-rline_all=debug3",
		"-nompsavegame",
		"-netSessionIgnoreDataHash",
		"-netSessionIgnoreCheater",
		"-netSessionIgnoreAim",
		"-netSessionIgnoreTimeout",
		"-netSessionIgnoreVersion"
		// platform specific commands
		"-output",
		"-rag",
		"-ragUseOwnWindow",
		"-ragAddr=192.168.137.1",
		"-ttyframeprefix",
		"-logfile=xd:/console_final.log",
	};
	
#define MAX_ARGS 50
	// argument array (consists of baked and supplied arguments)
	static const char* argValues[MAX_ARGS];

	// copy baked arguments
	int nBakedArgs = COUNTOF(bakedArgs);
	for(int i = 0; i < nBakedArgs; i++)
	{
		argValues[i] = bakedArgs[i];
	}

	// take exe name from supplied arguments (non-PS4)
#if !RSG_SCE
	argValues[0] = argv[0];
#endif

	// pass these to CommonMain_Prologue
	char** gargv = (char**)(&argValues[0]);
	int gargc = nBakedArgs;

	Displayf("Commandline params - Total: %d (Baked: %d | Supplied: %d):",gargc+argc-1,gargc,argc);
	for(int i=0;i<gargc;i++)
		Displayf("%s",gargv[i]);
	for(int i=0;i<argc;i++)
		Displayf("%s",argv[i]);
	// append logging args with those passed in; needed for pc, but could be useful for other platforms
	if(argc>1)
	{
		if(argc+gargc-1>MAX_ARGS)
			Errorf("Too many commandline parameters (%d) for this build configuration (max=%d); extras will be ignored.",argc-1,MAX_ARGS-gargc);

		for(int i=1;i<argc && i<MAX_ARGS;i++)
		{
			argValues[gargc++]=argv[i];
		}
	}

	if(CommonMain_Prologue(gargc, gargv))
#else
	if(CommonMain_Prologue(argc, argv))
#endif
	{
		bool carryOn = true;

		while(carryOn)
		{
			carryOn = CommonMain_OneLoopIteration();
		}
		CommonMain_Epilogue();
	}
	return g_MainReturnValue;
}


#if __XENON

#if RAGE_TRACKING
static const char *AllocType(int type) {
	switch (type) {
		case 1: return "pgRscAllocator::AllocateMemory";
		case 2: return "sysMemSimpleAllocator";
		case eXALLOCAllocatorId_D3D: return "XTL:D3D";
		case eXALLOCAllocatorId_D3DX : return "XTL:D3DX";
		case eXALLOCAllocatorId_XAUDIO: return "XTL:XAUDIO";
		case eXALLOCAllocatorId_XAPI: return "XTL:XAPI";
		case eXALLOCAllocatorId_XACT: return "XTL:XACT";
		case eXALLOCAllocatorId_XBOXKERNEL: return "XTL:XBOXKERNEL";
		case eXALLOCAllocatorId_XBDM: return "XTL:XBDM";
		case eXALLOCAllocatorId_XGRAPHICS: return "XTL:XGRAPHICS";
		case eXALLOCAllocatorId_XONLINE: return "XTL:XONLINE";
		case eXALLOCAllocatorId_XVOICE: return "XTL:XVOICE";
		case eXALLOCAllocatorId_XHV: return "XTL:XHV";
		case eXALLOCAllocatorId_USB: return "XTL:USB";
		case eXALLOCAllocatorId_XMV: "XTL:XMV";
		case eXALLOCAllocatorId_SHADERCOMPILER: return "XTL:SHADERCOMPILER";
		case eXALLOCAllocatorId_XUI: return "XTL:XUI";
		case eXALLOCAllocatorId_XASYNC: return "XTL:XASYNC";
		case eXALLOCAllocatorId_XCAM: return "XTL:XCAM";
		default: return "XTL:Unknown";
	}
}
#endif

void *XenonMemAlloc(size_t size,u32 dwAllocAttributes) {
	void *addr = XMemAllocDefault(size,dwAllocAttributes);
#if RAGE_ENABLE_RAGE_NEW
	if (addr)
		RAGE_LOG_NEW(addr, size, __FILE__, __LINE__);
#endif
#if RAGE_TRACKING
	if (addr && diagTracker::GetCurrent()) {
		int allocId = (dwAllocAttributes>>16)&255;
		diagTracker::GetCurrent()->Push(AllocType(allocId));
		diagTracker::GetCurrent()->Tally(addr,XMemSizeDefault(addr,dwAllocAttributes),MEMTYPE_RESOURCE_PHYSICAL /* let's just lie for now, not sure how this is actually used */);
		diagTracker::GetCurrent()->Pop();
	}
#endif
	return addr;
}

void XenonMemFree(void *ptr,u32 dwAllocAttributes) {
#if RAGE_TRACKING
	if (ptr && diagTracker::GetCurrent())
		diagTracker::GetCurrent()->UnTally(ptr,XMemSizeDefault(ptr,dwAllocAttributes));
#endif
#if RAGE_ENABLE_RAGE_NEW
	if (ptr)
		RAGE_LOG_DELETE(ptr);
#endif
	XMemFreeDefault(ptr,dwAllocAttributes);
}

size_t XenonMemSize(void *ptr,u32 dwAllocAttributes) {
	return XMemSizeDefault(ptr,dwAllocAttributes);
}

#endif // __XENON


}	// namespace rage

#if __PPU
#include <exception>
namespace std {
	void std::exception::_Raise() const {
		AssertMsg(false,"_Raise called?");
	}
	void std::exception::_Doraise() const {
		AssertMsg(false,"_Doraise called?");
	}
}
#endif


#if RSG_DURANGO
bool CommonMain_Prologue_DurangoWrapper(int argc,char **argv)
{
	// The OS passes us an argv[0] based on the package name (eg, "G:\\GTAV"),
	// so we need to remap it to what the rest of the code is expecting.
	argv[0] = "G:\\game_durango_" RSG_CONFIGURATION_LOWERCASE ".exe";

	return rage::CommonMain_Prologue(argc, argv);
}

bool CommonMain_OneLoopIteration_DurangoWrapper()
{
	return rage::CommonMain_OneLoopIteration();
}

void CommonMain_Epilogue_DurangoWrapper()
{
	rage::CommonMain_Epilogue();
}
#endif // RSG_DURANGO
