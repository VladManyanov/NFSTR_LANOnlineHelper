//
// Need for Speed The Run - Helper script for custom LAN online gameplay
// by Hypercycle / And799 / mRally2
// code project based on "NFS The Run - Ultimate Unlocker" by Tenjoin
//

#define WIN32_LEAN_AND_MEAN
#include <thread>
#include "includes/injector/injector.hpp"
#include "includes/injector/assembly.hpp"
#include "includes/mini/ini.h"
#include "includes/patterns.hpp"

//#define TESTCONSOLE

// mINI lib gives only strings, so...
std::string trueStr = "1";
std::string falseStr = "0";

std::string GameCfg = "GameSettings";
std::string OnlineCfg = "OnlineSettings";
std::string ServerCfg = "ServerSettings";
std::string PlaylistCfg = "PlaylistSettings";

// Difference between Client and Dedicated Server executables
const char clientHex[] = "16 51 6A 00 52 8B C8 E8";
const char serverHex[] = "16 51 52 6A 01 8B C8 E8";

bool IsClient = false;
bool IsServer = false;

mINI::INIStructure ini;

//

__declspec(naked) uint32_t EndianSwap(uint32_t value)
{
	__asm
	{
		mov eax, dword ptr[esp + 4]
		bswap eax
		ret
	}
}

void RemoveSpaces(std::string str)
{
	str.erase(remove(str.begin(), str.end(), ' '), str.end());
}

void StartTestConsole()
{
	AllocConsole();
	FILE* fDummy;
	freopen_s(&fDummy, "CONIN$", "r", stdin);
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
	freopen_s(&fDummy, "CONOUT$", "w", stdout);
}

std::string PtrToHexStr(uintptr_t ptr)
{
	std::stringstream stream;
	stream << std::hex << ptr;
	std::string hexStr = "0x" + stream.str();
	return hexStr;
}

// Check for int hex > string hex conversion
std::string SWIntToHexStr(uint32_t value)
{
	value = EndianSwap(value);
	std::stringstream stream;
	stream << std::hex << value;
	std::string hexStr = stream.str();
	return hexStr;
}

// Check for numeric string > hex conversion
unsigned long StrToULong(std::string str)
{
	return strtoul(str.c_str(), 0, 10);
}

// Check for string hash conversion
unsigned long StrHashToULong(std::string str)
{
	unsigned long value = strtoul(str.c_str(), 0, 16);
	value = EndianSwap(value);
	return value;
}

//

uint32_t GetPlaylistArrayConfigValue()
{
	std::string playlistSetArrayIDStr = ini.get(PlaylistCfg).get("ForcePlaylistSetArrayID");
	return StrToULong(playlistSetArrayIDStr);
}

uint32_t GetPlaylistIDConfigValue()
{
	std::string playlistIDStr = ini.get(PlaylistCfg).get("ForcePlaylistID");
	uint32_t playlistID = StrToULong(playlistIDStr);
	if (playlistID < 0) { playlistID = 0; }
	return playlistID;
}

// TODO Can be redone by using all we know from server-side GetPlaylistSetOffset()
uintptr_t LoadPlaylistMap()
{
	// Basic Playlist Set entity info
	int playlistSetSkip = 0x24 + 0x44; // Skip entity base and playlist pointers bytes, to array map
	// Before playlists pointers starts, there comes a 0x4 int with array size.
	uintptr_t playlistSet_ptr = *(int*)0x27A330C;
	unsigned int playlistArrayMapPtr = (int)(playlistSet_ptr + playlistSetSkip);

	uintptr_t playlistArray1Ptr = *(int*)playlistArrayMapPtr;
	uintptr_t playlistArray1[6] = {
		*(int*)playlistArray1Ptr, // Mixed
		*(int*)(playlistArray1Ptr + 0x4), // Exotic
		*(int*)(playlistArray1Ptr + 0x8), // Muscle
		*(int*)(playlistArray1Ptr + 0xC), // Underground
		*(int*)(playlistArray1Ptr + 0x10), // NFS
		*(int*)(playlistArray1Ptr + 0x14) }; // Supercar

	uintptr_t playlistArray2Ptr = *(int*)(playlistArrayMapPtr + 0x4);
	uintptr_t playlistArray2[1] = {
		*(int*)playlistArray2Ptr }; // All

	uintptr_t playlistArray3Ptr = *(int*)(playlistArrayMapPtr + 0x8);
	uintptr_t playlistArray3[2] = {
		*(int*)playlistArray3Ptr, // Smoketest
		*(int*)(playlistArray3Ptr + 0x4) }; // AllTracksTenCars

	uintptr_t playlistArray4Ptr = *(int*)(playlistArrayMapPtr + 0xC);
	uintptr_t playlistArray4[3] = {
		*(int*)playlistArray4Ptr, // Playtest
		*(int*)(playlistArray4Ptr + 0x4), // Stage2
		*(int*)(playlistArray4Ptr + 0x8) }; // Debug

	// What Playlist array group, and Playlist ID we want?
	uint32_t playlistSetArrayID = GetPlaylistArrayConfigValue();
	uint32_t playlistID = GetPlaylistIDConfigValue();

	uintptr_t customPlaylistPtr;
	switch (playlistSetArrayID)
	{
	default:
	case 0:
		if (playlistID >= sizeof(playlistArray1)) { playlistID = 0; }
		customPlaylistPtr = playlistArray1[playlistID];
		break;
	case 1:
		if (playlistID >= sizeof(playlistArray2)) { playlistID = 0; }
		customPlaylistPtr = playlistArray2[playlistID];
		break;
	case 2:
		if (playlistID >= sizeof(playlistArray3)) { playlistID = 0; }
		customPlaylistPtr = playlistArray3[playlistID];
		break;
	case 3:
		if (playlistID >= sizeof(playlistArray4)) { playlistID = 0; }
		customPlaylistPtr = playlistArray4[playlistID];
		break;
	}

#ifdef TESTCONSOLE
	printf("### CSM_PlaylistArrayMap_Ptr: %s\n", PtrToHexStr(playlistArrayMapPtr).c_str());
	printf("### CSM_PlaylistArray1_Ptr: %s\n", PtrToHexStr(playlistArray1Ptr).c_str());
	printf("### CSM_PlaylistArray1_0: %s\n", PtrToHexStr(playlistArray1[0]).c_str());
	printf("### CSM_PlaylistArray1_1: %s\n", PtrToHexStr(playlistArray1[1]).c_str());
	printf("### CSM_PlaylistArray1_2: %s\n", PtrToHexStr(playlistArray1[2]).c_str());
	printf("### CSM_PlaylistArray1_3: %s\n", PtrToHexStr(playlistArray1[3]).c_str());
	printf("### CSM_PlaylistArray1_4: %s\n", PtrToHexStr(playlistArray1[4]).c_str());
	printf("### CSM_PlaylistArray1_5: %s\n\n", PtrToHexStr(playlistArray1[5]).c_str());

	printf("### CSM_PlaylistArray2_Ptr: %s\n", PtrToHexStr(playlistArray2Ptr).c_str());
	printf("### CSM_PlaylistArray2_0: %s\n\n", PtrToHexStr(playlistArray2[0]).c_str());

	printf("### CSM_PlaylistArray3_Ptr: %s\n", PtrToHexStr(playlistArray3Ptr).c_str());
	printf("### CSM_PlaylistArray3_0: %s\n", PtrToHexStr(playlistArray3[0]).c_str());
	printf("### CSM_PlaylistArray3_1: %s\n\n", PtrToHexStr(playlistArray3[1]).c_str());

	printf("### CSM_PlaylistArray4_Ptr: %s\n", PtrToHexStr(playlistArray4Ptr).c_str());
	printf("### CSM_PlaylistArray4_0: %s\n", PtrToHexStr(playlistArray4[0]).c_str());
	printf("### CSM_PlaylistArray4_1: %s\n", PtrToHexStr(playlistArray4[1]).c_str());
	printf("### CSM_PlaylistArray4_2: %s\n\n", PtrToHexStr(playlistArray4[2]).c_str());

	printf("### CSM_Playlist_Ptr: %s\n", PtrToHexStr(customPlaylistPtr).c_str());
#endif

	return customPlaylistPtr;
}

//

// Disable secure connections for any client/server backend.
void DisableSSLCertRequirement()
{
	injector::WriteMemory<uint8_t>(0xDE6BA7, 0x15, true);
	injector::WriteMemory<uint8_t>(0xDE6BA8, 0x00, true);
}

// [fb::online::OnlineManager::init] Force clients to use socket.
// Allows to correctly connect to dedicated servers with enabled Presence.
void ForceSocketForClient()
{
	injector::MakeJMP(0xE9B2A8, 0xE9B2B2);
}

// [ClientPlaylistStartPlaylist_SPMessage] Change hash value of client message.
// Instead, we sent PlaylistMP message, to set up Playlist mode on client-server sides.
void SwapPlaylistSPMessage()
{
	injector::WriteMemory<uint8_t>(0x2269335, 0xFC, true);
	injector::WriteMemory<uint8_t>(0x2269336, 0x08, true);
	injector::WriteMemory<uint8_t>(0x2269337, 0x5A, true);
	injector::WriteMemory<uint8_t>(0x2269338, 0xB0, true);
}

// By default, View Cars menu limits the car roster which can be saved as "base" player car.
// On come cases, even default unlocked special cars cannot be saved inside that menu.
// This tweak allows to save any chosen car from menu.
// Note: Locked status of the vehicle doesn't matter anymore, it depends on other check.
void EnableAllCarsAssignment()
{
	injector::MakeNOP(0x8848F5, 13);
}

void EnableLANOnlineTweaks()
{
	if (ini.get(OnlineCfg).get("EnableLANOnlineTweaks") == trueStr)
	{
		ForceSocketForClient();
		SwapPlaylistSPMessage();
		DisableSSLCertRequirement();
		EnableAllCarsAssignment();
	}
}

// Force Career Mode into PreLoad state. By default, server always starts with 0 (disabled Career Mode).
void ForceCareerStatePreLoad()
{
	if (ini.get(ServerCfg).get("ForceCareerStatePreLoad") == trueStr)
	{
		// Disable initial CareerState assignment to 0
		injector::MakeNOP(0x1401BEF, 6);

		injector::WriteMemory<uint8_t>(0x27A4370, 0x02, true);
	}
}

uintptr_t playlistSetOffset;
void GetPlaylistSetOffset()
{
	// What Playlist array group, and Playlist ID we want?
	uint32_t playlistSetArrayID = GetPlaylistArrayConfigValue();
	uint32_t playlistID = GetPlaylistIDConfigValue();

	uint8_t playlistSetHeaderSkip = 0x28;
	uint8_t offsetToOurPlaylist = 0x0;
	uint8_t playlistArray_0_Offset[6] = { 0x0, 0x4, 0x8, 0xC, 0x10, 0x14 };
	uint8_t playlistArray_1_Offset = 0x1C;
	uint8_t playlistArray_2_Offset[2] = { 0x24, 0x28 };
	uint8_t playlistArray_3_Offset[3] = { 0x30, 0x34, 0x38 };

	switch (playlistSetArrayID)
	{
	default:
	case 0: // Mixed, Exotic, Muscle, Underground, NFS, Supercar
		if (playlistID >= sizeof(playlistArray_0_Offset)) { playlistID = 0x0; }
		offsetToOurPlaylist = playlistArray_0_Offset[playlistID];
		break;
	case 1: // All
		offsetToOurPlaylist = playlistArray_1_Offset;
		break;
	case 2: // Smoketest, AllTracksTenCars
		if (playlistID >= sizeof(playlistArray_2_Offset)) { playlistID = 0x0; }
		offsetToOurPlaylist = playlistArray_2_Offset[playlistID];
		break;
	case 3: // Playtest, Stage2, Debug
		if (playlistID >= sizeof(playlistArray_3_Offset)) { playlistID = 0x0; }
		offsetToOurPlaylist = playlistArray_3_Offset[playlistID];
		break;
	}
	playlistSetOffset = playlistSetHeaderSkip + offsetToOurPlaylist;
}

uintptr_t playlistLoaderRetPtr = 0x010383A2;
__declspec(naked) void ForcePlaylistLoading_asmPart()
{
	_asm
	{
		mov EAX, dword ptr [ESI + 0xC] // Get PlaylistSet
		add EAX, playlistSetOffset // Add offset to reach Playlist ptr
		mov EAX, dword ptr [EAX] // Get Playlist ptr
		mov dword ptr [ESI + 0x10], EAX // Save Playlist ptr on 0x027A3310
		jmp playlistLoaderRetPtr
	}
}

// This tweak makes possible to load Playlist data earlier than intended.
// Without it and outside of Debug Playlist method, Ending Vote will cause server crash due to missing data.
void ForceServerPlaylistLoading()
{
	if (ini.get(ServerCfg).get("ForcePlaylistLoading") != trueStr)
	{
		return;
	}
	injector::MakeNOP(0x14011C8, 5);
	injector::WriteMemory<uint8_t>(0x14011CF, 0x2, true);
	injector::WriteMemory<uint8_t>(0x14011D1, 0xC, true);

	// Set our Playlist ptr on specific place, to properly load resoures
	GetPlaylistSetOffset();
	injector::MakeNOP(0x103839D, 9);
	injector::MakeJMP(0x103839D, ForcePlaylistLoading_asmPart);
	injector::WriteMemory<uint32_t>(0x10383A2, EndianSwap(0x5EC20400), true);
}

// Saves custom Session ID on Server boot.
void ForcePlaylistSession()
{
	if (ini.get(ServerCfg).get("ForceCustomPlaylistSessionFirstID") == falseStr)
	{
		return;
	}
	std::string sessionIDParam = ini.get(ServerCfg).get("PlaylistSessionID");
	uint32_t sessionID = StrToULong(sessionIDParam);
	injector::WriteMemory<uint8_t>(0x27A3304, sessionID, false);
#ifdef TESTCONSOLE
	printf("### CSM_PlaylistSessionId: %s\n\n", SWIntToHexStr(sessionID).c_str());
#endif
}

// Disable any Playlist Session ID updates.
// Ending Vote results also will be ignored, and the same session will be restarted.
void DisablePlaylistSessionIDChanges()
{
	if (ini.get(ServerCfg).get("DisablePlaylistSessionIDChanges") == trueStr)
	{
		injector::MakeNOP(0x13FBE77, 3);
	}
}

void SaveRandomSessionId()
{
	uint8_t currentSessionId = *(BYTE*)0x027A3304;
#ifdef TESTCONSOLE
	printf("### CSM_PlaylistSessionId: %s\n\n", SWIntToHexStr(currentSessionId).c_str());
#endif
	injector::WriteMemory<uint8_t>(0x01400182, currentSessionId, true);
}

uintptr_t playlistSessionLoaderRetPtr = 0x013FFE4B;
__declspec(naked) void FixPlaylistSessionRandomizer_asmPart()
{
	_asm
	{
		call SaveRandomSessionId
		jmp playlistSessionLoaderRetPtr
	}
}

// Tweak some of safety-checks, to save first randomized Session ID.
// Doesn't happen with "normal" game servers, but it's necessary to manually define it with our launch method.
void FixPlaylistSessionRandomizer()
{
	if (ini.get(ServerCfg).get("ForcePlaylistLoading") != trueStr
		|| ini.get(ServerCfg).get("ForceCustomPlaylistSessionFirstID") == trueStr)
	{
		return;
	}
	injector::MakeNOP(0x13FFE3F, 16);
	injector::WriteMemory<uint32_t>(0x13FFE3F, EndianSwap(0xC6868410), true);

	injector::WriteMemory<uint8_t>(0x13FFE43, 0x00, true);
	injector::WriteMemory<uint8_t>(0x13FFE44, 0x00, true);
	injector::WriteMemory<uint8_t>(0x13FFE45, 0x01, true);
	
	injector::MakeJMP(0x13FFE46, FixPlaylistSessionRandomizer_asmPart);
	injector::WriteMemory<uint32_t>(0x13FFE4B, EndianSwap(0x5EC20400), true);
}

// Skips intro movies and sequence.
void ForceFastBoot()
{
	if (ini.get(GameCfg).get("GameFastBoot") == trueStr)
	{
		injector::WriteMemory<uint8_t>(0x8A9361, 0x01, true);
	}
}

// Choose initial UI flow to load first.
// With "MainMenu" flow, game loads even faster.
void ForceCustomInitFlow()
{
	std::string initFlow = ini.get(GameCfg).get("ForceInitFlow");
	// Possible variants: 
	// BootFlow, StartScreen, MainMenu, TheRun, ChallengeSeries
	// Career, OnlineLobby, Playgroup
	if (!initFlow.empty())
	{
		injector::WriteMemoryRaw(0x247CF70, &initFlow[0], 12, true);
	}
}

// [UILabelTreeMenuItem::Visibility] Makes hidden menu labels visible.
void EnableDebugModeMenus()
{
	if (ini.get(GameCfg).get("EnableDebugModeMenus") == trueStr)
	{
		injector::MakeJMP(0x968F50, 0x968F7B);
	}
}

// [fb::unknownSpace::getHostId] Activate fail-check for default client name.
// Controls the size of GetComputerNameA function result. 
// Making it as "0" causes code to activate fail-safe client name.
// Then, we replace default "WIN32DEFAULT" string with custom client name.
void ClientComputerNameTweaks()
{
	if (ini.get(OnlineCfg).get("DisableUserComputerName") == falseStr)
	{
		return;
	}
	injector::WriteMemory<uint8_t>(0x4F2B07, 0x0, true);

	std::string clientCustomName = ini.get(OnlineCfg).get("ClientName");
	if (!clientCustomName.empty())
	{
		injector::WriteMemoryRaw(0x23EB5A8, &clientCustomName[0], 16, true);
	}
}

// Enables "IsGuestAccount" variable, which instantly cancel any Autolog connection attempts.
void DisableAutologConnectAttempts()
{
	if (ini.get(OnlineCfg).get("DisableAutologConnectAttempts") == trueStr)
	{
		injector::WriteMemory<uint8_t>(0x8FEF9E, 0x1, true);
	}
}

// Due to ForcePlaylistLoading tweak, server will try to skip first event on boot.
void ResetPlaylistRouteIDTweak()
{
	if (ini.get(ServerCfg).get("ForcePlaylistLoading") == trueStr)
	{
		injector::WriteMemory<uint32_t>(0x27A3308, 0xFFFFFFFF, false);
	}
}

uintptr_t playlistCustomPtr;
void GetPlaylistPtrFromSet()
{
	playlistCustomPtr = LoadPlaylistMap();
}

uintptr_t playlistPtrRetPtr = 0x0085DB9A;
__declspec(naked) void ForcePlaylistPtrInMemory_asmPart()
{
	_asm
	{
		call GetPlaylistPtrFromSet
		push playlistCustomPtr
		pop ECX
		mov dword ptr [EBP + 0x10], ECX // Set Playlist ptr on Client CareerMode info area
		jmp playlistPtrRetPtr
	}
}

// Force Playlist pointer by default. Without it, Client instances will crash on Playlist mode loading.
// Due to lack of working sync requests to load server Playlist, we force it manually.
// Originally, clients gets this pointer by proceeding through Frontend menus.
void ForceClientPlaylistPtrInMemory()
{
	if (ini.get(PlaylistCfg).get("ForceClientPlaylistPtrInMemory") == falseStr)
	{
		return;
	}
	injector::MakeNOP(0x1033DD5, 3); // Remove assignment to 0

	injector::MakeNOP(0x85DB93, 7);
	injector::MakeJMP(0x85DB93, ForcePlaylistPtrInMemory_asmPart);
	return;
}

// Swap one of cars from Debug Car List, to your preferred vehicle by hash.
// Effectively replaces stock "Aston Martin One-77" vehicle on the list.
void ChangeDebugCarHash()
{
	std::string debugCarHash = ini.get(GameCfg).get("ChangeDebugCarHash");
	if (debugCarHash == falseStr)
	{
		return;
	}
	uint32_t carHashInt = StrHashToULong(debugCarHash);

#ifdef TESTCONSOLE
	printf("### ChangeDebugCarHash: %s\n\n", SWIntToHexStr(carHashInt).c_str());
#endif

	// Prevent game crash and wait for Car List init
	injector::WriteMemoryRaw(0xF3D313CC, &carHashInt, 4, true);
}

void InitClientPreLoadHelper()
{
	EnableLANOnlineTweaks();
	EnableDebugModeMenus();
	ForceFastBoot();
	ForceCustomInitFlow();
	ClientComputerNameTweaks();
	DisableAutologConnectAttempts();
	ForceClientPlaylistPtrInMemory();
}

void InitServerPreLoadHelper()
{
	ForceCareerStatePreLoad();
	DisableSSLCertRequirement();
	ForceServerPlaylistLoading();
	FixPlaylistSessionRandomizer();
}

void InitClientThreadedHelper()
{
	Sleep(4000); // TODO Find all pointers, or find another way
	ChangeDebugCarHash();
}

void InitServerThreadedHelper()
{
	Sleep(4000); // TODO Works for now
	ResetPlaylistRouteIDTweak();
	ForcePlaylistSession();
}

// Apply tweaks, depending on the type of game executable.
void CheckGameExecutableType()
{
	pattern::Win32::Init();
	IsClient = pattern::get_first(clientHex) != 0;
	if (!IsClient)
	{
		IsServer = pattern::get_first(serverHex) != 0;
	}
}

// Do stuff before game boots up
void InitHelperPreLoadBase()
{
#ifdef TESTCONSOLE
	StartTestConsole();
#endif
	CheckGameExecutableType();

	mINI::INIFile file("NFSTR_LANOnlineHelper.ini");
	file.read(ini);

	if (IsClient)
	{
		InitClientPreLoadHelper();
		return;
	}
	if (IsServer)
	{
		InitServerPreLoadHelper();
		return;
	}
}

// Do stuff on new thread, while game runs
void InitHelperThreadedBase()
{
	if (IsClient)
	{
		InitClientThreadedHelper();
		return;
	}
	if (IsServer)
	{
		InitServerThreadedHelper();
		return;
	}
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		InitHelperPreLoadBase();
		std::thread(InitHelperThreadedBase).detach();
	}
	return TRUE;
}
