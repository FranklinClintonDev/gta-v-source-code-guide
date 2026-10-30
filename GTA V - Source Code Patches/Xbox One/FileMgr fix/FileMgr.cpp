#include "system/FileMgr.h"

// C headers
#if __WIN32PC
#include "direct.h"
#pragma warning(disable: 4668)
#include <shlobj.h>
#pragma warning(error: 4668)
#endif
// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "diag/channel.h"
#include "file/asset.h"
#include "file/file_channel.h"
#include "file/default_paths.h"
#include "file/device_crc.h"
#include "file/device_installer.h"
#include "file/device_relative.h"
#include "file/packfile.h"
#include "file/savegame.h"
#include "grcore/debugdraw.h"
#include "grcore/light.h"
#include "grcore/setup.h"
#include "grcore/stateblock.h"
#include "grmodel/setup.h"
#include "parser/manager.h"
#include "profile/timebars.h"
#include "streaming/CacheLoader.h"
#include "streaming/packfilemanager.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginstall_psn.h"
#include "streaming/streamingvisualize.h"
#include "string/string.h"
#include "system/bootmgr.h"
#include "system/memory.h"
#include "system/system.h"
#include "system/xtl.h"
#include "system/param.h"
#include "audiohardware/waveslot.h"

// Game headers
#ifndef NAVGEN_TOOL
#include "audio/frontendaudioentity.h"
#include "control/gamelogic.h"
#include "control/videorecording/videorecording.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/loadingscreens.h"
#include "frontend/WarningScreen.h"
#include "modelinfo/ModelInfo.h"
#include "renderer/color.h"
#include "renderer/renderthread.h"
#include "scene/datafilemgr.h"
#include "scene/extracontent.h"
#include "scene/scene.h"
#include "system/controlmgr.h"
#include "system/StreamingInstall.winrt.h"
#include "system/system.h"
#include "text/Text.h"
#include "text/TextConversion.h"
#include "debug/Debug.h"
#else
#include <direct.h>
#include "GTATypes.h"
#endif

#define ASSETS_DIRECTORY		    "x:/gta5/assets_ng/"
#define ART_DIRECTORY			    RS_PROJROOT "/art/"
#if __BANK
#define TOOL_DIRECTORY			    RS_TOOLSROOT "/"
#endif

// Max number of RPFs the platform packfile can be split up into
#if RSG_ORBIS || RSG_DURANGO || (RSG_PC && RSG_CPU_X64)
#define MAX_PLATFORM_PACK_COUNT		26
#define MAX_PLATFORMCRC_PACK_COUNT	2	// only one needed right now, but we might add more later, eg, if split across chunks
#else
#define MAX_PLATFORM_PACK_COUNT		2
#endif // RSG_ORBIS || RSG_DURANGO

#define USE_NUMBERED_PLATPACKS		0	// use numbered platform packs instead of lettered, eg, xboxone02.rpf instead of xboxonec.rpf (if needing more than 26 packs)

//	The PLATFORM_DIRECTORY needs to be consistent with the tests in ragebuilder/pack.cpp.
#if RSG_ORBIS
	#define DISKROOT_DIRECTORY	    "/app0/"
    #define HDD_GAME_DIRECTORY_BASE "/data/app/"
#if __FINAL
	#define ROOT_DIRECTORY		    "/app0/" 
#else
	#define ROOT_DIRECTORY		    RS_BUILDBRANCH "/" 
#endif // __FINAL
	#define PLATFORM_PACK_COUNT	    23
	#define PLATFORMCRC_PACK_COUNT  1
	#define PLATFORM_DIRECTORY	    "ps4/"
	#define PLATFORM_PACK		    "ps4.rpf"
	#define PLATFORM_DEBUG_PACK     "ps4_debug.rpf"	// contains all unshippable data for packaged non-final builds
#elif RSG_DURANGO
	#define DISKROOT_DIRECTORY	    "G:/"
    #define HDD_GAME_DIRECTORY_BASE "G:/"
#if __FINAL
	#define ROOT_DIRECTORY		    "G:/"
#else
	#define ROOT_DIRECTORY		    RS_BUILDBRANCH "/" 
#endif // __FINAL
	#define PLATFORM_PACK_COUNT	    23
	#define PLATFORMCRC_PACK_COUNT	1
	#define PLATFORM_DIRECTORY	    "xboxone/"
	#define PLATFORM_PACK		    "xboxone.rpf"
	#define PLATFORM_DEBUG_PACK     "xboxone_debug.rpf"	// contains all unshippable data for packaged non-final builds
#else	// pc, anything else
	#define DISKROOT_DIRECTORY	    ""
	#if __PACKEDDEBUG
		#define ROOT_DIRECTORY	    ""
	#else
		#define ROOT_DIRECTORY	    RS_BUILDBRANCH "/"
	#endif
	#define PLATFORM_PACK_COUNT	    23
	#define PLATFORMCRC_PACK_COUNT	1
#if __64BIT
	#define PLATFORM_DIRECTORY	    "x64/"
	#define PLATFORM_PACK		    "x64.rpf"
	#define PLATFORM_DEBUG_PACK     "x64_debug.rpf"	// contains all unshippable data for packaged non-final builds
#else
	#define PLATFORM_DIRECTORY	    "pc/"
	#define PLATFORM_PACK		    "pc.rpf"
#endif
#endif //__XENON

#if !__PACKEDDEBUG
#define COMMON_DEBUG_PACK           "common_debug.rpf"	// contains all unshippable data for packaged non-final builds
#endif	// !__FINAL

#define COMMON_DIRECTORY		    "common/"

#if RSG_ORBIS
#define AUDIO_DIRECTORY			    "ps4/audio/"
#elif RSG_DURANGO
#define AUDIO_DIRECTORY			    "xboxone/audio/"
#elif RSG_PC
#define AUDIO_DIRECTORY			    "x64/audio/"
#else
#define AUDIO_DIRECTORY			    PLATFORM_DIRECTORY "audio/"
#endif

#if !__PACKEDDEBUG
#define AUDIO_RPF				    "audio.rpf"
#define AUDIO_PACKLIST			    "packlist.txt"
#else
#define AUDIO_RPF				    "audio_rel.rpf" 
#define AUDIO_PACKLIST			    "packlist_rel.txt"
#endif

char CFileMgr::ms_rootDirName[MAX_DIRNAME_CHARS] = ROOT_DIRECTORY;
char CFileMgr::ms_dirName[MAX_DIRNAME_CHARS];
char CFileMgr::ms_hddBootPath[MAX_DIRNAME_CHARS] = "";
#if __WIN32PC
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_appPath;
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_userPath;
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_rgscPath;

atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_appDataRootDirName;
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_originalAppDataRootDirName;
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_userDataRootDirName;
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_originalUserDataRootDirName;
atFixedString<MAX_DIRNAME_CHARS> CFileMgr::ms_rockStarRootDirName;
static sysCriticalSectionToken s_FileMgrToken;
#endif	// __WIN32PC

bool CFileMgr::ms_allChunksOnHdd = true;

#if !__FINAL
struct DummyAlloc
{
	void *		m_Buffer;
	u32			m_Size;
	u32			m_Allocator;
};

DummyAlloc s_dummyAllocs[] =
{
	// orbis, durango, pc

	// keep in sync VIRTUAL_STREAMING_MEMORY_LIMIT and PHYSICAL_STREAMING_MEMORY_LIMIT (GameScriptMgr.h)
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_VIRTUAL },
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_VIRTUAL },
	{ NULL, 1 << 22, MEMTYPE_RESOURCE_VIRTUAL },	//4

	{ NULL, 1 << 23, MEMTYPE_RESOURCE_PHYSICAL },
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_PHYSICAL },
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_PHYSICAL },
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_PHYSICAL },
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_PHYSICAL },
	{ NULL, 1 << 23, MEMTYPE_RESOURCE_PHYSICAL },	//8
	{ NULL, 1 << 22, MEMTYPE_RESOURCE_PHYSICAL },	//4
	{ NULL, 1 << 21, MEMTYPE_RESOURCE_PHYSICAL },	//2
	{ NULL, 1 << 20, MEMTYPE_RESOURCE_PHYSICAL }	//1
};
#endif //!__FINAL

fiDeviceRelative gPlatformDevice;
fiDeviceRelative gCommonDevice;
fiDeviceRelative gGameCacheDevice;
fiDeviceRelative gAudioDevice;
fiDeviceCrc gCrcPlatformDevice;
fiDeviceCrc gCrcCommonDevice;
#if !__PACKEDDEBUG
fiDeviceRelative gAssetsDevice;
fiDeviceRelative gArtDevice;
fiDeviceRelative gDebugDevice;			// mounted as debug: typically from the hosted build directory (eg, /<project>/build/<branch>)
// For packaged non-final builds, the devices below mount extra 'xxx_debug.rpf' packfiles that have -all- unshippable data in them
// To disable these (eg, for simulating a final configuration), use -nodebugpack as well as any applicable redirects (only final shaders, scripts, and text will be available)
// Note that these are mounted after the main devices, but before the update: device, so if there's any overlap, the search hierarchy is update: -> debug common:/platform: -> main common:/platform:
fiPackfile gDebugPlatformPackfileDevice;	// only for packaged non-final builds (mounted as platform: from {platform}_debug.rpf)
fiPackfile gDebugCommonPackfileDevice;		// only for packaged non-final builds (mounted as common: from common_debug.rpf)
#if __BANK
fiDeviceRelative gToolDevice;
#endif // __BANK
#endif // !__PACKEDDEBUG

#if RSG_DURANGO
#if !__FINAL
fiDeviceRelative gPlatformDeviceRfs;
fiDeviceRelative gCommonDeviceRfs;
fiDeviceCrc gCrcPlatformDeviceRfs;
fiDeviceCrc gCrcCommonDeviceRfs;
#endif // !__FINAL
#elif RSG_ORBIS && !__PACKEDDEBUG
fiDeviceRelative gPlatformDeviceRfs;
fiDeviceRelative gCommonDeviceRfs;
fiDeviceCrc gCrcPlatformDeviceRfs;
fiDeviceCrc gCrcCommonDeviceRfs;
#endif // RSG_DURANGO

#if __WIN32PC
fiDeviceRelative gUserDevice;
fiDeviceRelative gAppDataDevice;
#endif // __WIN32PC

fiPackfile gCommonPackfileDevice;
fiPackfile gPlatformPackfileDevice[MAX_PLATFORM_PACK_COUNT];
fiDeviceCrc gPlatformCrcPackfileDevice[MAX_PLATFORMCRC_PACK_COUNT]; 
fiPackfile gAudioPackfileDevice;
atArray<fiPackfile*> gAudioPacks;
int gPlatformPackfileCount = PLATFORM_PACK_COUNT;
int gPlatformCRCPackfileCount = PLATFORMCRC_PACK_COUNT;
#define PLAYGO_PACK_COUNT (2)

bool gCommonRpfInMemory = false;
bool gCommonRpfFromOdd = true;

namespace rage 
{
	NOSTRIP_XPARAM(audiofolder);
}

PARAM(marketing, "Marketing memory size");
PARAM(maponly, "Only load the map - turn off all cars & peds");
PARAM(maponlyextra, "Only load the map - turn off all cars & peds");
PARAM(stealextramemory, "Steal some extra memory");
PARAM(cheapmode, "Only load the map - turn off all cars & peds - no water reflections - reflection slods=300 - far clip=1500");
PARAM(common, "Indicate where common folder is");
PARAM(platform, "Indicate where platform folder is");
PARAM(nodebugpack, "Disable use of xxx_debug.rpf packfiles in packaged non-final builds");
NOSTRIP_PARAM(commonpack, "Indicate where common packfile is");
NOSTRIP_PARAM(platformpack, "Indicate where platform packfile is");
NOSTRIP_PARAM(audiopack, "Indicate where audio packfile is");
PARAM(audiopacklist, "Override audio packlist location");
PARAM(artdir, "Indicate where the art dir for the game is");
PARAM(assetsdir, "Indicate where the assets dir for the game is");
#if __BANK
PARAM(tooldir, "Indicate where the tools dir for the game is");
#endif
PARAM(rootdir, "Indicate where the root dir for the game is");
PARAM(usepackfiles, "Use RPF pack files");
PARAM(forceadpcm, "Use mp3 files for PS4");
#if defined(VIDEO_RECORDING_ENABLED) && VIDEO_RECORDING_ENABLED && (RSG_PC || RSG_DURANGO)
PARAM(videoOutputDir, "Indicate where to place recorded videos");
#endif
NOSTRIP_PARAM(userdir, "Indictate where the user folder is");
PARAM(cleaninstall, "Do not use hdd data even when it's newer.");
PARAM(playgoemu, "Emulation of PlayGo through private http server");
PARAM(playgotest, "Triggers the PlayGo script for debug and testing");

#if RSG_DURANGO || RSG_ORBIS
namespace rage
{
	XPARAM(forceboothdd);
	XPARAM(forceboothddloose);
};
#endif

static fiPackfile::CacheMode GetDefaultPackfileCacheMode()
{
	return fiDeviceInstaller::GetIsBootedFromHdd() ? fiPackfile::CACHE_INSTALLER_FILE : fiPackfile::CACHE_NONE;
}

const char* GetRootFolder()
{
	const char* pRootFolder = ROOT_DIRECTORY;
#if __CONSOLE
	if (fiDeviceInstaller::GetIsBootedFromHdd())
	{
		return CFileMgr::GetHddBootPath();
	}
	if(sysBootManager::IsBootedFromDisc())
	{
		return DISKROOT_DIRECTORY;
	}
#endif
	if(PARAM_rootdir.Get(pRootFolder))
	{
		fileAssertf((pRootFolder[strlen(pRootFolder)-1] == '/'), "-rootdir parameter requires terminating //!");
	}

#if RSG_DURANGO && __FINAL
	return "G:/";
#else
#if RSG_DURANGO
	if(PARAM_forceboothdd.Get() || PARAM_forceboothddloose.Get())
	{
		return "G:/";
	}
	else
#endif
	{
#if !__FINAL
		fileDisplayf("Root folder %s\n", pRootFolder);
#endif
		return pRootFolder;
	}
#endif
}

const char *GetAudioFolder()
{
    if (fiDeviceInstaller::GetIsBootedFromHdd())
	{
#if RSG_DURANGO || RSG_ORBIS
        return HDD_GAME_DIRECTORY_BASE AUDIO_DIRECTORY;
#else
        return GetRootFolder();
#endif
	}

	if(sysBootManager::IsBootedFromDisc())
	{
#if RSG_DURANGO || RSG_ORBIS
        return DISKROOT_DIRECTORY AUDIO_DIRECTORY;
#else
		return GetRootFolder();
#endif
	}

#if RSG_ORBIS
	if(PARAM_forceadpcm.Get())
	{
		return "x64/audio/";
	}
	else
	{
		return "ps4/audio/";
	}
#elif RSG_DURANGO
	return "xboxone/audio/";
#elif RSG_PC
	return "x64/audio/";
#else
	return "platform:/audio/";
#endif
}

namespace rage {
NOSTRIP_XPARAM(nocachetimestamps);
};

#if !__FINAL
namespace rage {
XPARAM(noquits);
XPARAM(update);
};
#endif

const char* CFileMgr::GetAssetsFolder()
{
	const char* pAssetsFolder = ASSETS_DIRECTORY;
	if(PARAM_assetsdir.Get(pAssetsFolder))
	{
		fileAssertf((pAssetsFolder[strlen(pAssetsFolder)-1] == '/'), "-assetsdir parameter requires terminating //!");
	}
#if !__FINAL
	fileDisplayf("Assets folder %s\n", pAssetsFolder);
#endif
	return pAssetsFolder;
}

const char* GetArtFolder()
{
	const char* pArtFolder = ART_DIRECTORY;
	if(PARAM_artdir.Get(pArtFolder))
	{
		fileAssertf((pArtFolder[strlen(pArtFolder)-1] == '/'), "-artdir parameter requires terminating //!");
	}
#if !__FINAL
	fileDisplayf("Art folder %s\n", pArtFolder);
#endif
	return pArtFolder;
}

#if __BANK
const char* CFileMgr::GetToolFolder()
{
	const char* pToolFolder = TOOL_DIRECTORY;
	if(PARAM_tooldir.Get(pToolFolder))
	{
		fileAssertf((pToolFolder[strlen(pToolFolder)-1] == '/'), "-tooldir parameter requires terminating //!");
	}
#if !__FINAL
	fileDisplayf("Tool folder %s\n", pToolFolder);
#endif
	return pToolFolder;
}
#endif

//
// name:		ReadErrorHandler
// description:	Read error handler
void ReadErrorHandler()
{
#if RSG_ORBIS 
#if __FINAL
	CWarningScreen::SetFatalReadMessageWithHeader(WARNING_MESSAGE_STANDARD, "", "READ_ERROR", FE_WARNING_NONE, false, -1);	
#else
	CWarningScreen::SetFatalReadMessageWithHeader(WARNING_MESSAGE_STANDARD, "", "READ_ERROR", FE_WARNING_OK, false, -1);	
#endif
#endif
}



void CheckPack(bool success, const char *OUTPUT_ONLY(filename))
{
	//@@: location CFILEMGR_CHECKPACK_ENTRY
	if(!success)
	{
		if(sysBootManager::IsBootedFromDisc())
		{
			Errorf("Unable to find critical archive '%s', your disc image might be broken.",filename);
			ReadErrorHandler();
			PS3_ONLY(Quitf("Missing packfile" OUTPUT_ONLY("%s", filename)));
		}
		else
		{
			fileErrorf("Unable to find critical archive '%s', check that RFS connected properly.",filename);
			__debugbreak();
			ReadErrorHandler();
		}
	}
}

#if !__FINAL && !defined(NAVGEN_TOOL)
// allocate the dummy chunks of memory to prevent art mode using up all resource memory
void CFileMgr::AllocateDummyResourceMem()
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);
	MEM_USE_USERDATA(MEMUSERDATA_TEXLITE);

	for(u32 i=0; i<NELEM(s_dummyAllocs); i++)
	{
		Assert(s_dummyAllocs[i].m_Buffer == NULL);
		s_dummyAllocs[i].m_Buffer = strStreamingEngine::GetAllocator().Allocate(s_dummyAllocs[i].m_Size, 128, s_dummyAllocs[i].m_Allocator);
		Assert(s_dummyAllocs[i].m_Buffer);
	}
}

void CFileMgr::FreeDummyResourceMem()
{
	MEM_USE_USERDATA(MEMUSERDATA_TEXLITE);

	for(u32 i=0; i<NELEM(s_dummyAllocs); i++){
		strStreamingEngine::GetAllocator().Free(s_dummyAllocs[i].m_Buffer);
		s_dummyAllocs[i].m_Buffer = NULL;
	}
}

atArray<void*> s_extraallocs;
// allocate the dummy chunks of memory to prevent art mode using up all resource memory
void CFileMgr::AllocateExtraResourceMem(size_t memory)
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);
	MEM_USE_USERDATA(MEMUSERDATA_TEXLITE);

	strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();

	size_t taken = 0;
	size_t chunk = g_rscVirtualLeafSize << 8;
	while (memory > g_rscVirtualLeafSize)
	{
		while (chunk > memory)
		{
			chunk >>= 1;
		}

		if (chunk < g_rscVirtualLeafSize)
			break;

		void* alloc = allocator.TryAllocate(chunk, 128, MEMTYPE_RESOURCE_VIRTUAL);
		if (alloc) 
		{
			s_extraallocs.Grow() = alloc;
			memory -= chunk;
			taken += chunk;
		}
		else
		{
			chunk >>= 1;
		}
	}

	Warningf("Taken %" SIZETFMT "dKb of extra resource memory",taken >> 10);
}

void CFileMgr::FreeExtraResourceMem()
{
	MEM_USE_USERDATA(MEMUSERDATA_TEXLITE);
	strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
	for (int i = 0; i < s_extraallocs.GetCount();++i)
	{
		allocator.Free(s_extraallocs[i]);
	}
	s_extraallocs.Reset();
}
#endif //!__FINAL

static void MakePlatformPackName(char *outBuffer, size_t outBufferSize, const char *rawName, int packIndex)
{
	if (gPlatformPackfileCount == 1)
	{
		Assert(packIndex == 0);
		safecpy(outBuffer, rawName, outBufferSize);
	}
	else
	{
		Assert(packIndex < gPlatformPackfileCount);

		const char *extension = strrchr(rawName, '.');

		size_t baseSize = Min(outBufferSize-2, (size_t) (extension - rawName));
		memcpy(outBuffer, rawName, baseSize);
#if USE_NUMBERED_PLATPACKS
		formatf(&outBuffer[baseSize],outBufferSize-baseSize,"%02d",packIndex);
		safecat(outBuffer,extension,outBufferSize);
#else
		outBuffer[baseSize] = 'a' + (char) packIndex;
		safecpy(&outBuffer[baseSize+1], extension, outBufferSize - baseSize - 1);
#endif	// USE_NUMBERED_PLATPACKS
	}
}

// callback for when blocking on loading to avoid network timeouts
static const unsigned int KEEP_ALIVE_CALLBACK_INTERVAL = 50;
static void KeepAliveCallback()
{
	//CLoadingScreens::KeepAliveUpdate();

#if __BANK
	// Also keep the bank up to date here, to make sure we're flushing any queues (in particular the invoke queue)
	BANKMGR.Update();
#endif
}

void CFileMgr::PreSetupDevices()
{
    // when running debug hdd builds on ng, mount all data on PC too.
    // this allows us to load rpfs from the pc if data there is newer than what's installed on console
#if !__FINAL && (RSG_ORBIS || RSG_DURANGO)
	if ((!PARAM_forceboothdd.Get() && !PARAM_forceboothddloose.Get()) || PARAM_cleaninstall.Get())
        return;

	const char* pRootFolder = ROOT_DIRECTORY;
	if (PARAM_rootdir.Get(pRootFolder))
		fileAssertf((pRootFolder[strlen(pRootFolder)-1] == '/'), "-rootdir parameter requires terminating //!");

	fiDevice::SetFindNewest(true);

	char commonFolder[MAX_DIRNAME_CHARS];
	char platformFolder[MAX_DIRNAME_CHARS];

	strcpy(commonFolder, COMMON_DIRECTORY);
	strcpy(platformFolder, PLATFORM_DIRECTORY);

    char pCommonFolderPath[RAGE_MAX_PATH] = {0};
    char pPlatformFolderPath[RAGE_MAX_PATH] = {0};
    formatf(pCommonFolderPath, "%s%s", pRootFolder, commonFolder);
    formatf(pPlatformFolderPath, "%s%s", pRootFolder, platformFolder);

	const char* pCommonFolder = pCommonFolderPath;
	const char* pPlatformFolder = pPlatformFolderPath;

	PARAM_common.Get(pCommonFolder);
	PARAM_platform.Get(pPlatformFolder);

	// setup "common:/" device
	gCommonDeviceRfs.Init(pCommonFolder, true);
	gCommonDeviceRfs.MountAs("common:/");

	// mount crc device
	gCrcCommonDeviceRfs.Init("common:/", true, &gCommonDeviceRfs);
	gCrcCommonDeviceRfs.MountAs("commoncrc:/");

	// setup "platform:/" device
	gPlatformDeviceRfs.Init(pPlatformFolder, true);
	gPlatformDeviceRfs.MountAs("platform:/");

	// mount platformcrc device
	gCrcPlatformDeviceRfs.Init("platform:/", true, &gPlatformDeviceRfs);
	gCrcPlatformDeviceRfs.MountAs("platformcrc:/");
#endif // !__FINAL && (RSG_ORBIS || RSG_DURANGO)
}

char g_AudioPack[RAGE_MAX_PATH];
//
// name:		SetupDevices
// description:	
void CFileMgr::SetupDevices()
{
    PreSetupDevices();

	char commonFolder[MAX_DIRNAME_CHARS];
	char platformFolder[MAX_DIRNAME_CHARS];
	char audioFolder[MAX_DIRNAME_CHARS];

	
	strcpy(commonFolder, COMMON_DIRECTORY);
	strcpy(platformFolder, PLATFORM_DIRECTORY);
	if(PARAM_forceadpcm.Get())
	{
		strcpy(audioFolder, "x64/audio/");
	}
	else
	{
		strcpy(audioFolder, AUDIO_DIRECTORY);
	}

	fileDisplayf("Booting %s", sysParam::GetProgramName());

	fiDeviceInstaller::SetKeepaliveCallback(datCallback(CFA(KeepAliveCallback)), KEEP_ALIVE_CALLBACK_INTERVAL);

#if __CONSOLE
#if RSG_ORBIS || RSG_DURANGO
	if ((fiDeviceInstaller::GetIsBootedFromHdd() || PARAM_playgoemu.Get()) && !PARAM_forceboothddloose.Get())
	{
        if (PARAM_playgoemu.Get())
		{
            PARAM_commonpack.Set(DISKROOT_DIRECTORY "common.rpf");
            PARAM_platformpack.Set(DISKROOT_DIRECTORY PLATFORM_PACK);
            PARAM_audiopack.Set(DISKROOT_DIRECTORY AUDIO_DIRECTORY AUDIO_RPF);
		}
		else
		{
            PARAM_commonpack.Set(HDD_GAME_DIRECTORY_BASE "common.rpf");
            PARAM_platformpack.Set(HDD_GAME_DIRECTORY_BASE PLATFORM_PACK);
            PARAM_audiopack.Set(HDD_GAME_DIRECTORY_BASE AUDIO_DIRECTORY AUDIO_RPF);
		}

		strCacheLoader::SetReadOnly(true);
	}
	else
#endif
	if(sysBootManager::IsBootedFromDisc())
	{
		const char* rootDir = GetRootFolder();
		static char commonPath[RAGE_MAX_PATH];
		static char platformPath[RAGE_MAX_PATH];
		static char audioPath[RAGE_MAX_PATH];

		if (gCommonRpfFromOdd)
		{
			formatf(commonPath, "%s%s", rootDir, "common.rpf");
		}

		PARAM_commonpack.Set(commonPath);
		
		formatf(platformPath, "%s%s", rootDir, PLATFORM_PACK);
		PARAM_platformpack.Set(platformPath);
		
		formatf(audioPath, "%s" AUDIO_RPF, GetAudioFolder());
		Displayf("Audio rpf = %s", audioPath);
		PARAM_audiopack.Set(audioPath);
// #if !__FINAL
// 		PARAM_noquits.Set("1");
// #endif
		strCacheLoader::SetReadOnly(true);
	}
#if !__FINAL
	else if(PARAM_usepackfiles.Get())
	{
		gPlatformPackfileCount = 1;	// We never use split platform packfiles when running with -usepackfiles.
		PARAM_commonpack.Set("common.rpf");
		PARAM_platformpack.Set(PLATFORM_PACK);
		strCacheLoader::SetReadOnly(true);
	}
#endif
#else
// pc final builds assume packaged config - may switch this to __MASTER only
#if __PACKEDDEBUG
    PARAM_commonpack.Set(DISKROOT_DIRECTORY "common.rpf");
    PARAM_platformpack.Set(DISKROOT_DIRECTORY PLATFORM_PACK);
    PARAM_audiopack.Set(DISKROOT_DIRECTORY AUDIO_DIRECTORY AUDIO_RPF);
#endif	// !__FINAL
#endif

	const char* pCommonFolder = commonFolder;
	const char* pPlatformFolder = platformFolder;
	const char* pCommonPack = NULL;
	const char* pPlatformPack = NULL;
	const char* pAudioFolder = audioFolder;
#if RSG_ORBIS
	if(PARAM_forceadpcm.Get())
	{
		formatf(g_AudioPack, "%sx64/audio/" AUDIO_RPF, GetRootFolder());
	}
	else
	{
		formatf(g_AudioPack, "%sps4/audio/" AUDIO_RPF, GetRootFolder());
	}
#elif RSG_DURANGO
	formatf(g_AudioPack, "%sxboxone/audio/" AUDIO_RPF, GetRootFolder());
#elif RSG_PC
	formatf(g_AudioPack, "%sx64/audio/" AUDIO_RPF, GetRootFolder());
#else
	formatf(g_AudioPack, "%s" AUDIO_RPF, GetAudioFolder());
#endif

	Displayf("Audio rpf = %s", g_AudioPack);

	PARAM_common.Get(pCommonFolder);
	PARAM_platform.Get(pPlatformFolder);
	PARAM_audiofolder.Get(pAudioFolder);

	// Indicate whether or not this is a packaged build.
#if !__PACKEDDEBUG
	sysBootManager::SetIsPackagedBuild(PARAM_platformpack.Get() && PARAM_commonpack.Get());

	if (PARAM_commonpack.Get())
	{
		// If we're grabbing common from a pack file, it's read only - that means we can't update the cache.
		// As such, the cache is expected to be there and up-to-date.
		PARAM_nocachetimestamps.Set("");
	}
#else
	// final can (and probably will) have packaged common data
	if (PARAM_commonpack.Get())
		PARAM_nocachetimestamps.Set("");
#endif // !__FINAL

	// setup "common:/" device
	if(PARAM_commonpack.Get(pCommonPack))
	{
#if 0
		CheckPack(gCommonPackfileDevice.Init(pCommonPack,true,fiPackfile::CACHE_MEMORY),pCommonPack);
		gCommonRpfInMemory = true;
#else
		CheckPack(gCommonPackfileDevice.Init(pCommonPack,true,GetDefaultPackfileCacheMode()),pCommonPack);
#endif
		gCommonPackfileDevice.MountAs("common:/");
		gCommonDevice.Init(pCommonFolder, true);
		gCommonDevice.MountAs("common:/");

		// mount crc device
		gCrcCommonDevice.Init("common:/", true, &gCommonPackfileDevice);
		gCrcCommonDevice.MountAs("commoncrc:/");
	}
	else
	{
		gCommonDevice.Init(pCommonFolder, true NOTFINAL_ONLY(&& sysBootManager::IsPackagedBuild()));	// A packaged build might overlap common:/ with a read-only device, and we can't have mismatching readonly flags
		gCommonDevice.MountAs("common:/");

		// mount crc device
		gCrcCommonDevice.Init("common:/", true, &gCommonDevice);
		gCrcCommonDevice.MountAs("commoncrc:/");
	}

	// Let's also set up the cache folder. This can be different in some build types where common:/ is read-only.
	// We WILL have to point it to the read-only RPF in some cases - like in disc builds.
	// Since common:/ can be read-only, we have to use a separate mapping here.
    bool packedCommon = pCommonPack!=NULL;
    if (fiDeviceInstaller::GetIsBootedFromHdd() || PARAM_playgoemu.Get())
	{
        packedCommon = true;
#if RSG_ORBIS || RSG_DURANGO
        if (PARAM_forceboothddloose.Get())
		{
            packedCommon = false;
		}
#endif
	}

	// setup "platform:/" device
	if(PARAM_platformpack.Get(pPlatformPack))
	{
		gPlatformDevice.Init(pPlatformFolder, true);
		gPlatformDevice.MountAs("platform:/");

		// mount platformcrc device
		gCrcPlatformDevice.Init("platform:/", true, &gPlatformDevice);
		gCrcPlatformDevice.MountAs("platformcrc:/");
		
        if (CFileMgr::IsGameInstalled())
		{
            for (int x=0; x<gPlatformPackfileCount; x++)
            {
                char platformPackName[RAGE_MAX_PATH];
                MakePlatformPackName(platformPackName, sizeof(platformPackName), pPlatformPack, x);
                CheckPack(gPlatformPackfileDevice[x].Init(platformPackName,true,GetDefaultPackfileCacheMode()),platformPackName);
                gPlatformPackfileDevice[x].MountAs("platform:/");
                if(x<gPlatformCRCPackfileCount)
                {
                    gPlatformCrcPackfileDevice[x].Init("platform:/", true, &gPlatformPackfileDevice[x]);
                    gPlatformCrcPackfileDevice[x].MountAs("platformcrc:/");
                }
            }
		}
		else
		{
            // the first two platform packs contain the initial payload for the prologue
			for (int x=0; x<PLAYGO_PACK_COUNT; x++)
			{
				char platformPackName[RAGE_MAX_PATH];
				MakePlatformPackName(platformPackName, sizeof(platformPackName), pPlatformPack, x);
				CheckPack(gPlatformPackfileDevice[x].Init(platformPackName,true,GetDefaultPackfileCacheMode()),platformPackName);
				gPlatformPackfileDevice[x].MountAs("platform:/");
				if(x<gPlatformCRCPackfileCount)
				{
					gPlatformCrcPackfileDevice[x].Init("platform:/", true, &gPlatformPackfileDevice[x]);
					gPlatformCrcPackfileDevice[x].MountAs("platformcrc:/");
				}
			}
		}
	}
	else
	{
		gPlatformDevice.Init(pPlatformFolder, false);
		gPlatformDevice.MountAs("platform:/");

		// mount platformcrc device
		gCrcPlatformDevice.Init("platform:/", true, &gPlatformDevice);
		gCrcPlatformDevice.MountAs("platformcrc:/");
	}

#if !__PACKEDDEBUG
	if(!PARAM_nodebugpack.Get() && (PARAM_commonpack.Get() || PARAM_platformpack.Get()))
	{
		if(PARAM_commonpack.Get())
		{
			CheckPack(gDebugCommonPackfileDevice.Init(COMMON_DEBUG_PACK,true,GetDefaultPackfileCacheMode()),COMMON_DEBUG_PACK);
			gDebugCommonPackfileDevice.MountAs("common:/");
		}
		if(PARAM_platformpack.Get())
		{
			CheckPack(gDebugPlatformPackfileDevice.Init(PLATFORM_DEBUG_PACK,true,GetDefaultPackfileCacheMode()),PLATFORM_DEBUG_PACK);
			gDebugPlatformPackfileDevice.MountAs("platform:/");
		}
	}
#endif	// !__FINAL

#if RSG_PC
	const char *pParamValue;
	char userDirPath[RAGE_MAX_PATH] = {0};

	// Get the user directory from command line (passed via Social Club), default to regular UserDataRootDirectory
	if(PARAM_userdir.GetParameter(pParamValue))
	{
		// Safety check the Path, default to regular UserDataRootDirectory
		safecpy(userDirPath, pParamValue);
		if (ASSET.CreateLeadingPath(userDirPath))
		{
			gUserDevice.Init(userDirPath, false);
		}
		else
		{
			ASSET.CreateLeadingPath(GetUserDataRootDirectory());
			gUserDevice.Init(GetUserDataRootDirectory(), false);
		}
	}
	else
	{
		ASSET.CreateLeadingPath(GetUserDataRootDirectory());
		gUserDevice.Init(GetUserDataRootDirectory(), false);
	}
	
	gUserDevice.MountAs("user:/");

	// Create app data folder_
	ASSET.CreateLeadingPath(GetAppDataRootDirectory());
	gAppDataDevice.Init(GetAppDataRootDirectory(), false);
	gAppDataDevice.MountAs("appdata:/");
	
#endif // RSG_PC

#if (RSG_PC || RSG_DURANGO)
#if defined(VIDEO_RECORDING_ENABLED) && VIDEO_RECORDING_ENABLED
	// Set the output directory for videos on PC
	char const * videoDirectory = NULL;
	PARAM_videoOutputDir.Get( videoDirectory );
	VideoRecording::InitializeExtraRecordingData( videoDirectory );
#endif
#endif // (RSG_PC || RSG_DURANGO)

	// setup "audio:/" device
	if(!PARAM_audiofolder.Get())
	{
		// Default to loading assets from audio.rpf - only use loose assets if -audiofolder=x is specified on cmd line			
		MountAudioPacks(g_AudioPack);
	}
	else
	{
		PARAM_audiofolder.Get(pAudioFolder);
		Warningf("Using loose audio assets from %s", pAudioFolder);
		gAudioDevice.Init(pAudioFolder, true);
		gAudioDevice.MountAs("audio:/");
	}

#if !__PACKEDDEBUG
	gAssetsDevice.Init(GetAssetsFolder(), false);
	gAssetsDevice.MountAs("assets:/");
	gArtDevice.Init(GetArtFolder(), false);
	gArtDevice.MountAs("art:/");
	gDebugDevice.Init("x:\\gta5\\build\\dev_ng", false);
	gDebugDevice.MountAs("debug:/");
#if __BANK
	gToolDevice.Init(GetToolFolder(), false);
	gToolDevice.MountAs("tool:/");
#endif // __BANK
#endif // !FINAL

	CExtraContentManager::MountUpdate();

	if (__PACKEDDEBUG || sysBootManager::IsBootedFromDisc() || packedCommon)
	{
		Displayf("Game cache is using read-only device - if you didn't prebuild your cache, you'll have a problem.");
		extern fiDeviceRelative gTitleUpdateCommon2;
		gGameCacheDevice.Init("common:/", true, &gTitleUpdateCommon2 );
	}
	else
	{
#if !__PACKEDDEBUG && RSG_PC	// definitely helps with pc, but may want to enable for other platforms, too
		const char* pUpdateFolder;
		if (PARAM_update.Get(pUpdateFolder))
		{
			gGameCacheDevice.Init("update:/common/", false);
		}
		else
#endif
		{
			extern fiDeviceRelative gTitleUpdateCommon2;
			gGameCacheDevice.Init("common:/", false, &gTitleUpdateCommon2);
		}
	}
	gGameCacheDevice.MountAs("gamecache:/");
}

bool CFileMgr::ShouldForceReleaseAudioPacklist()
{
	return false;
}

bool CFileMgr::IsDownloadableVersion()
{
	return false;
}

bool CFileMgr::IsAudioPackInInitialPackage(const char *packName)
{
	const u32 initialPackList[] = { 0x41B63FE9, 0x231498DE };

	const u32 hashToSearch = atStringHash(packName);

	for(u32 i = 0; i < NELEM(initialPackList); i++)
	{
		if(hashToSearch == initialPackList[i])
		{
			return true;
		}
	}
	return false;
}

bool CFileMgr::ShouldAudioPackBeSkipped(const char *CONSOLE_ONLY(packName))
{
#if !RSG_PC
	const u32 skipList[] = { 0x3B9F8E17 }; // DLC_GTAO

	const u32 hashToSearch = atStringHash(packName);

	for(u32 i = 0; i < NELEM(skipList); i++)
	{
		if(hashToSearch == skipList[i])
		{
			return true;
		}
	}
#endif
	return false;
}

void CFileMgr::MountAudioPacks(const char *packName)
{
	bool secondaryInit = false;

	if(!gAudioPackfileDevice.IsInitialized())
	{
		audWaveSlot::LoadStaticBankInfo();

		PARAM_audiopack.Get(packName);

		CheckPack(gAudioPackfileDevice.Init(packName,true,GetDefaultPackfileCacheMode()), packName);
		gAudioPackfileDevice.MountAs("audio:/");
	}
	else
	{
		secondaryInit = true;
	}

	const char *packList = ShouldForceReleaseAudioPacklist() ? "audio:/config/packlist_rel.txt" : "audio:/config/" AUDIO_PACKLIST;


	if (!sysBootManager::IsBootedFromDisc() || RSG_ORBIS || RSG_DURANGO)
	{
		PARAM_audiopacklist.Get(packList);
		FileHandle handle = OpenFile(packList);
		if(handle != INVALID_FILE_HANDLE)
		{
			bool eof = false;
			while(!eof)
			{
				const char *line = ReadLine(handle, true);

				if(!line || strlen(line) == 0)
				{
					eof = true;
					continue;
				}

				if(ShouldAudioPackBeSkipped(line))
				{
					audDisplayf("Skipping pack: %s", line);
					continue;
				}

				const bool isInitialPack = IsAudioPackInInitialPackage(line);
				if(isInitialPack)
				{
					audDisplayf("Initial pack: %s", line);
				}
			
				if(isInitialPack && secondaryInit)
				{
					continue;
				}
				else if(!isInitialPack && !IsGameInstalled())
				{
					continue;
				}

				fiPackfile *pack = rage_new fiPackfile();
				gAudioPacks.Grow() = pack;
				char packPath[RAGE_MAX_PATH];
				formatf(packPath, "%ssfx/%s.rpf", GetAudioFolder(), line);
				Displayf("Mounting audio pack %s", packPath);
				CheckPack(pack->Init(packPath,true,GetDefaultPackfileCacheMode()), packPath);

				formatf(packPath, "audio:/sfx/%s/", line);
				pack->MountAs(packPath);
			}

			CloseFile(handle);
		}
		else
		{
			audErrorf("Failed to open audio packlist: %s", packList);
		}
	}
	else
	{
		// MEGA RPF SUPPORT
		static const char* const audioPackfiles[] = {
			"audio_cs",
			"audio_misc",
			"audio_music",
			"audio_radio"
		};

		for(int i = 0; i < NELEM(audioPackfiles); i++)
		{
			char packPath[RAGE_MAX_PATH];
			formatf(packPath, "%ssfx/%s.rpf", GetAudioFolder(), audioPackfiles[i]);
			
			fiPackfile *pack = rage_new fiPackfile();
			gAudioPacks.Grow() = pack;
			CheckPack(pack->Init(packPath, true, GetDefaultPackfileCacheMode()), packPath);
			pack->MountAs("audio:/sfx/");
		}

		if(!secondaryInit)
		{
			fiDeviceRelative *occlusionDevice = rage_new fiDeviceRelative();
			occlusionDevice->Init("audio:/");

#if RSG_ORBIS
			occlusionDevice->MountAs("ps4/audio/");
#elif RSG_DURANGO
			occlusionDevice->MountAs("xboxone/audio/");
#elif RSG_PC
			occlusionDevice->MountAs("x64/audio/");
#else
			occlusionDevice->MountAs("platform:/audio/");
#endif
		}
	}
}

//
// name:		CFileMgr::SetupDevicesAfterInit
// description:	shutdown memory version of packfile
void CFileMgr::SetupDevicesAfterInit()
{
	const char* pCommonPack = NULL;
	if(PARAM_commonpack.Get(pCommonPack) && gCommonRpfInMemory)
	{
		fiDevice::Unmount("common:/");
		// shutdown memory device and init file device
		gCommonPackfileDevice.Shutdown();
		gCommonPackfileDevice.Init(pCommonPack,true,GetDefaultPackfileCacheMode());
		gCommonPackfileDevice.MountAs("common:/");
	}
}

void CFileMgr::Initialise()
{
    ms_allChunksOnHdd = StreamingInstall::Init();

#if !__FINAL
    // pretend we need to run the streaming install code for debugging and such
    if (PARAM_playgotest.Get())
	{
        ms_allChunksOnHdd = false;
	}
#endif

#if RSG_DURANGO || (RSG_ORBIS && !__FINAL)
#if !__FINAL
	if (PARAM_forceboothdd.Get() || PARAM_forceboothddloose.Get())
#endif
#if RSG_DURANGO || !__FINAL
	{
		SetGameHddBootPath(HDD_GAME_DIRECTORY_BASE);
	}
#endif
#endif // RSG_DURANGO || RSG_ORBIS

#if RSG_ORBIS && !__FINAL
    if (PARAM_playgoemu.Get())
	{
		SetGameHddBootPath(DISKROOT_DIRECTORY);
	}
#endif

	strcpy(ms_rootDirName, GetRootFolder());

#if RSG_DURANGO && !__FINAL
	if (PARAM_forceboothdd.Get() || PARAM_forceboothddloose.Get())
	{
		// Add the build branch as a secondary directory so we can open files from there
		strncat_s(ms_rootDirName, ";", _TRUNCATE);
		strncat_s(ms_rootDirName, ROOT_DIRECTORY, _TRUNCATE);
	}
#endif // RSG_DURANGO && !__FINAL

	ASSET.SetPath(ms_rootDirName);

	fiDevice::InitClass();
	fiDevice::sm_FatalReadError = &ReadErrorHandler;
	PF_START_STARTUPBAR("Init image list");
	strPackfileManager::InitImageList();

#if __WIN32PC
	// These need to be passed in once we know where they'll come from
	ms_appPath = "\\Rockstar Games\\GTA V\\";
	ms_userPath = "\\Rockstar Games\\GTA V\\";
	ms_rgscPath = "\\Rockstar Games\\RGSC\\";

	// get the current working dir on systems that support it.
	//_getcwd(ms_rootDirName, MAX_DIRNAME_CHARS);	
	char dir[MAX_DIRNAME_CHARS];
	char path[MAX_DIRNAME_CHARS];
	//grab the directory and the path from the exe
	if (!_splitpath_s(sysParam::sm_ProgName, dir, MAX_DIRNAME_CHARS, path, MAX_DIRNAME_CHARS, NULL, 0, NULL, 0))
	{
		safecat(dir, path, MAX_DIRNAME_CHARS);
		safecpy(ms_rootDirName, dir, MAX_DIRNAME_CHARS);
		//fix up any `/` in the path 
		size_t length = strlen(ms_rootDirName);
		for (size_t i=0;i<length,i<MAX_DIRNAME_CHARS;i++)
			if (ms_rootDirName[i] == '/')
				ms_rootDirName[i] = '\\';
		if(ms_rootDirName[length-1]  != '\\')
			safecat(ms_rootDirName, "\\", MAX_DIRNAME_CHARS);
	}
	else
	{
		_getcwd(ms_rootDirName, MAX_DIRNAME_CHARS);	
	}

	fileDisplayf("Starting in %s", ms_rootDirName);

	wchar_t AppDataRoot[MAX_DIRNAME_CHARS] = {0};
	char TempPathBuffer[MAX_DIRNAME_CHARS] = {0};
	int UnusedOut = 0;
	if ( SUCCEEDED( SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, AppDataRoot) ) )
	{	// CSIDL_LOCAL_APPDATA = <username>/Local Settings/Application Data/
		ms_originalAppDataRootDirName = WideToUtf8(TempPathBuffer, reinterpret_cast<const char16*>(AppDataRoot), MAX_DIRNAME_CHARS, MAX_DIRNAME_CHARS, &UnusedOut);
		ms_originalAppDataRootDirName += ms_appPath;
		ms_appDataRootDirName = ms_originalAppDataRootDirName;

		ms_rockStarRootDirName = TempPathBuffer;
		ms_rockStarRootDirName += ms_rgscPath;
  	}
 	else
 	{	Assert(0); 	}
 

	wchar_t UserDataRoot[MAX_DIRNAME_CHARS] = {0};
	
 	if ( SUCCEEDED( SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, UserDataRoot) ) )
 	{	// CSIDL_PERSONAL = My Documents
 		ms_originalUserDataRootDirName = WideToUtf8(TempPathBuffer, reinterpret_cast<const char16*>(UserDataRoot), MAX_DIRNAME_CHARS, MAX_DIRNAME_CHARS, &UnusedOut);;
		ms_originalUserDataRootDirName += ms_userPath;
 		ms_userDataRootDirName = ms_originalUserDataRootDirName;
 	}
 	else
 	{	Assert(0); 	}

#endif

	PF_START_STARTUPBAR("Setup devices");
	//@@: location CFILEMGR_INITIALISE
	SetupDevices();

	fileAssertf(!strstr(ms_rootDirName,"VS_Project"),"Your working directory is highly likely to be wrong. Currently is %s", ms_rootDirName); // DW - fed up of the working directory being wrong and it crashing later in the code.
} // end - CFileMgr::Initialise

//
// name:		NeedsPath
// description:	Returns if a filename needs the root directory prepended to it
bool CFileMgr::NeedsPath(const char *base) {

	if ( !base )
		return true;
	// If the name starts with either slash, or contains a colon anywhere, it does not need a path.
	if (base[0] == '/' || base[0] == '\\' || strchr(base,':'))
		return false;
	else
		return true;
}

#if RSG_DURANGO || RSG_ORBIS
void CFileMgr::SetGameHddBootPath(const char* hddBootPath)
{
	strncpy(ms_hddBootPath, hddBootPath, MAX_DIRNAME_CHARS);
	fiDeviceInstaller::SetIsBootedFromHdd(true, ms_hddBootPath);
}
#endif // RSG_DURANGO || RSG_ORBIS

void CFileMgr::SetDir(const char* pDirName)
{	
#if __WIN32PC
	sysCriticalSection oLock(s_FileMgrToken);
#endif

	ms_dirName[0] ='\0';
	if(NeedsPath(pDirName))
		strcpy(ms_dirName, ms_rootDirName);
	if(pDirName[0] != '\0')
	{
		strcat(ms_dirName, pDirName);
		if(pDirName[strlen(pDirName)-1] != '\\')
			strcat(ms_dirName, "\\");
	} 
	ASSET.SetPath(ms_dirName);
} // end - CFileMgr::SetDir


#if __WIN32PC
void CFileMgr::SetAppDataDir(const char* pDirName, bool bUseOriginal)
{	

	sysCriticalSection oLock(s_FileMgrToken);

	ms_dirName[0] = '\0';
	atFixedString<MAX_DIRNAME_CHARS> dirToSet;
	if(NeedsPath(pDirName))
	{
 		if( bUseOriginal )
 			dirToSet = ms_originalAppDataRootDirName;
 		else
 			dirToSet = ms_appDataRootDirName;
	}

	SetPCDir(dirToSet, pDirName);
} // end - CFileMgr::SetAppDataDir


void CFileMgr::SetUserDataDir(const char* pDirName, bool bUseOriginal)
{	
	sysCriticalSection oLock(s_FileMgrToken);

	ms_dirName[0] = '\0';
	atFixedString<MAX_DIRNAME_CHARS> dirToSet;
	if(NeedsPath(pDirName))
	{
 		if( bUseOriginal )
 			dirToSet = ms_originalUserDataRootDirName;
 		else
 			dirToSet = ms_userDataRootDirName;
	}

	SetPCDir(dirToSet, pDirName);
} // end - CFileMgr::SetUserDataDir


void CFileMgr::SetRockStarDir(const char* pDirName)
{	

	sysCriticalSection oLock(s_FileMgrToken);

	ms_dirName[0] = '\0';
	atFixedString<MAX_DIRNAME_CHARS> dirToSet = ms_rockStarRootDirName;
	
	SetPCDir(dirToSet, pDirName);
} // end - CFileMgr::SetRockStartDataDir


void CFileMgr::SetPCDir(atFixedString<MAX_DIRNAME_CHARS>& baseDir, const char* subDir)
{
	if(subDir[0] != '\0')
	{
		baseDir += subDir;
		if(!baseDir.EndsWith("\\") && !baseDir.EndsWith("/"))
			baseDir += "/";

		safecpy(ms_dirName, baseDir.c_str(), sizeof(ms_dirName));
		USES_CONVERSION;
		int result = SHCreateDirectoryExW(NULL, reinterpret_cast<const wchar_t*>(UTF8_TO_WIDE(ms_dirName)), NULL);
		if(result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS)
		{	Assertf(0, "Can't create directory: %s \n", ms_dirName);}
	}
	ASSET.SetPath(ms_dirName);
}
#endif // __WIN32PC


// Name			:	LoadFile
// Purpose		:	Loads a file into the specified buffer
// Parameters	:	pFilename - file to load
//					pBuffer - ptr to buffer to load file into
//					bufferSize - maximum size of file to copy to buffer
//					if 0, no action is taken
// Returns		:	File size or -1 if there is an error

s32 CFileMgr::LoadFile(const char* pFilename, u8* pBuffer, s32 DEV_ONLY(bufferSize), const char* UNUSED_PARAM(pReadString))
{
	fiStream* pStream;
	s32 size;
#if __DEV
	fileDebugf1("LoadFile: %s Size:%d", pFilename, bufferSize);
#endif

	// open file
	pStream = ASSET.Open(pFilename, "");

	if(pStream == NULL)
		return -1;

	size = pStream->Size();
	pStream->Read(pBuffer, size);

	// place end of file character at end of buffer
	pBuffer[size] = '\0';

	// close file
	pStream->Close();

#if __DEV
	fileDebugf1("Done LoadFile: %s Size:%d", pFilename, size);
#endif
	return size;
} // end - CFileMgr::LoadFile


// Name			:	OpenFile
// Purpose		:	opens a file and returns a file id
// Parameters	:	pFilename - file to load
FileHandle CFileMgr::OpenFile(const char* pFilename, const char* pReadString)
{
	fiStream* pStream;
	bool bReadonly = true;
	bool bCreate = false;

	// check if 'w' is in readstring
	if(pReadString)
	{
		while(*pReadString)
		{
			if(*pReadString == 'a')
				bReadonly = false;
			if(*pReadString == 'w')
				bCreate = true;
			pReadString++;
		}
	}

	fileDebugf1("Opening %s", pFilename);
	if(bCreate)
	{
		pStream = ASSET.Create(pFilename, "", false);
	}
	else
	{
		pStream = ASSET.Open(pFilename, "", false, bReadonly);
	}

	return (FileHandle)pStream;
}

// Name			:	OpenFileFroWriting
// Purpose		:	opens a file and returns a file id
// Parameters	:	pFilename - file to load
FileHandle CFileMgr::OpenFileForWriting(const char* pFilename)
{
	return OpenFile(pFilename, "w");
}

FileHandle CFileMgr::OpenFileForAppending(const char* pFilename)
{
	FileHandle NewFileHandle = OpenFile(pFilename, "a");
	if(NewFileHandle!=INVALID_FILE_HANDLE)
	{
		fiStream* pStream = (fiStream*) NewFileHandle;
		pStream->Seek(pStream->Size());
	}
	return NewFileHandle;
}

s32 CFileMgr::Read(FileHandle id, char* pData, s32 size)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Read(pData, size);
}

s32 CFileMgr::Write(FileHandle id, const char* pData, s32 size)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Write(pData, size);
}

s32 CFileMgr::Seek(FileHandle id, s32 offset)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Seek(offset);
}

s32 CFileMgr::Flush(FileHandle id)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Flush();
}

//
// name:		ReadLine
// description:	Read a single line from the file
bool CFileMgr::ReadLine(FileHandle id, char* pLine, s32 maxLen)
{
	fiStream* pStream = (fiStream*)id;

	s32 read = fgets(pLine,maxLen-1,pStream);
	if (read == 0  && pStream->Tell() >= pStream->Size())
		return false;

	s32 i;
	for(i=0; i<read; i++)
	{
		switch(pLine[i])
		{
			case '\r':
				pLine[i] = '\0';
				break;
			case '\n':
				pLine[i] = '\0';
			case '\0' :
				return true;				
		}
	}

	pLine[i] = '\0';

	return true;
}

//
// name:		ReadLine
// description:	Read a single line from the file and possibly remove crap (tabs and commas)
// params:		fileId & whether you want to removed crap (defaults to true)
char* CFileMgr::ReadLine(FileHandle id, bool bRemoveUnwantedChars)
{
	#define READ_SIZE_FOR_A_LINE 1024 //increased from 512 because time cycle was failing
	static char line[READ_SIZE_FOR_A_LINE];
	char* pLineStart = &line[0];
	if(ReadLine(id, &line[0], READ_SIZE_FOR_A_LINE))
	{
		if (bRemoveUnwantedChars)
		{
			char* pC = &line[0];
			while(*pC != '\0')
			{
				if(((*pC < ' ' && *pC >= 0) || *pC == ','))
					*pC = ' ';
				pC++;
			}
		}

		// get beginning of line first non-space/non-control character
		while(*pLineStart == ' ' && *pLineStart != '\0') 
			pLineStart++;

		return pLineStart;
	}
	return NULL;
}

//
// name:		CloseFile
// description:	Close a file handle
s32 CFileMgr::CloseFile(FileHandle id)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Close();
}

//
// name:		GetTotalSize
// description:	Get size of file
s32 CFileMgr::GetTotalSize(FileHandle id)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Size();
}

s32 CFileMgr::Tell(FileHandle id)
{
	fiStream* pStream = (fiStream*)id;
	return pStream->Tell();
}

bool CFileMgr::HasAsyncInstallFinished()
{
    if (IsGameInstalled())
        return true;

    return StreamingInstall::HasInstallFinished();
}

void CFileMgr::CleanupAsyncInstall()
{
    if (!HasAsyncInstallFinished())
        return;

    StreamingInstall::Shutdown();

	if (!IsGameInstalled())
	{
		ms_allChunksOnHdd = true;

		const char* pPlatformPack = NULL;
		if (PARAM_platformpack.Get(pPlatformPack))
		{
			for (int x=PLAYGO_PACK_COUNT; x<gPlatformPackfileCount; x++)
			{
				char platformPackName[RAGE_MAX_PATH];
				MakePlatformPackName(platformPackName, sizeof(platformPackName), pPlatformPack, x);
				CheckPack(gPlatformPackfileDevice[x].Init(platformPackName,true,GetDefaultPackfileCacheMode()),platformPackName);
				gPlatformPackfileDevice[x].MountAs("platform:/");
				if(x<gPlatformCRCPackfileCount)
				{
					gPlatformCrcPackfileDevice[x].Init("platform:/", true, &gPlatformPackfileDevice[x]);
					gPlatformCrcPackfileDevice[x].MountAs("platformcrc:/");
				}
			}
		}

		// Mount audio packs that were not included in the initial package
		MountAudioPacks(g_AudioPack);

		//CGameSessionStateMachine::SetForceInitLevel();
		//CGame::StartNewGame(CGameLogic::GetRequestedLevelIndex());
	}
}
