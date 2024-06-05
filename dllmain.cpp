//
// Need for Speed: The Run - Helper script for custom LAN online gameplay
// by Hypercycle / And799 / mRally2
//

#define WIN32_LEAN_AND_MEAN
#include "LANOnlineHelper.h"

#include <thread>
#include <random>

#include "includes/injector/injector.hpp"
#include "includes/injector/assembly.hpp"
#include "includes/mini/ini.h"
#include "includes/patterns.hpp"

//
// Variables
//

bool IsClient = false;
bool IsServer = false;
bool TestConsole = false;

char wndTitle[100];
char wndTitleCopy[100];
char* gameCurrentLevelStr;

int PlaylistSetArrayID = 0;
int PlaylistID = 0;
uint32_t forcedGarageCarHash;
char langCode[] = "en_Us";

std::mt19937 rng;
mINI::INIStructure ini;

//
// Util
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

template<typename... Args>
// Always use c_str() for any non-char string here
void TestConsolePrint(const char* format, Args&&... args) {
	if (TestConsole) {
		std::printf(format, std::forward<Args>(args)...);
	}
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

void CheckForMaxMinFloat(float value, float min, float max)
{
	if (value > max) {
		value = max;
	}
	if (value < min) {
		value = min;
	}
}

void CheckForMaxMinInt(int value, int min, int max)
{
	if (value > max) {
		value = max;
	}
	if (value < min) {
		value = min;
	}
}

std::vector<std::string> SplitStr(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(str);
	while (getline(tokenStream, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}

inline std::string ConcatStrVec(std::vector<std::string> vec, char delimiter) {
	std::string out = vec[0];
	for (unsigned int i = 1; i < vec.size(); i++) {
		out += delimiter + vec[i];
	}
	return out;
}

void InitRandomGen()
{
	std::mt19937 rngTmp(
		(unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
	rng = rngTmp;
}

//
// Structures
//

// GUID takes 0x10 bytes, pointers refers on class after it.
// Usually Array Pointers contains sizes at -0x4 position.
namespace ebx
{ 
	namespace playlists
	{
		class PlaylistSet {
		public:
			uintptr_t pt_EBXTypeHeader;		// 0x00
			uintptr_t pt_UnkRef;			// 0x04
			char EBXUnkCommonPart[4];		// 0x08
			uintptr_t pt_EBXNamePtr;		// 0x0C
			uintptr_t pt_DefaultPlaylist;	// 0x10
			uintptr_t ar_PlaylistGroups;	// 0x14
			char _Padding[12];				// 0x18
		};
		class PlaylistGroup {
		public:
			int count;
			std::vector<uintptr_t> pt_Playlists;
		};
		class PlaylistGroupArray {
		public:
			int count;
			std::vector<uintptr_t> pt_PlaylistGroup;
			std::vector<PlaylistGroup> PlaylistGroups;
		};
	}
	
};

uintptr_t clientOnlineEntityPtr = 0x2888F7C;
uintptr_t clientCareerEntityPtr = 0x28823BC;
uintptr_t serverCareerEntityPtr = 0x27A3300;
namespace mem
{
	namespace Client
	{
		class OnlineEntity {
		public:
			char EntityHeaderPart[8];		// 0x00
			BYTE IsSinglePlayer;			// 0x08
			BYTE IsLocalHost;				// 0x09
			BYTE IsInprocClient;			// 0x0A
			BYTE _UnkByte;					// 0x0A
			char _VarData1[64];				// 0x0C
			uint32_t SocketMode;			// 0x4C
			char _VarData2[84];				// 0x50
			uint32_t ConnectionState;		// 0xA4
		};

		class CareerEntity {
		public:
			char EntityHeaderPart[4];		// 0x00
			uint32_t PlaylistSessionId;		// 0x04
			uint32_t PlaylistRouteId;		// 0x08
			uintptr_t PlaylistSet;			// 0x0C
			uintptr_t CurrentPlaylist;		// 0x10
			char _VarData1[452];			// 0x14
			uint32_t CareerState;			// 0x1D8
			char _VarData2[12];				// 0x1DC
			uint32_t PListObjectivesValue;	// 0x1E8
		};

		class RoutePlayerEntity {
		public:
			char _VarData1[16];				// 0x00
			uint32_t PlayerID;				// 0x10
			uint32_t IsAI;					// 0x14
			uint32_t State1;				// 0x18
			uint32_t State2;				// 0x1C
			uint32_t RacePlace;				// 0x20
			float RoutePercentPassed;		// 0x24
			float RouteMetersPassed;		// 0x28
		};
	}
	namespace Server
	{
		class CareerEntity {
		public:
			char EntityHeaderPart[4];			// 0x00
			uint32_t PlaylistSessionId;			// 0x04
			uint32_t PlaylistRouteId;			// 0x08
			uintptr_t PlaylistSet;				// 0x0C
			uintptr_t CurrentPlaylist;			// 0x10
			char _VarData1[32];					// 0x14
			uint32_t PlayerCount;				// 0x34
			char _VarData2[4152];				// 0x38
			uint32_t CareerState;				// 0x1070
			float CareerStateTimer;				// 0x1074
			char _VarData3[8];					// 0x1078
			uint32_t PlaylistSessionsCount;		// 0x1080
			// And more unknown data
		};
	}
}

mem::Server::CareerEntity* serverCareerEnt;

mem::Client::OnlineEntity* onlineEnt;
// This pointer could be null sometimes, during connection attempts
// Also this pointer always being present during single-player
bool IsOnlineEntityExists()
{
	bool idk = *(int*)clientOnlineEntityPtr != 0;
	if (idk)
	{
		onlineEnt = *(mem::Client::OnlineEntity**)clientOnlineEntityPtr;
	}
	return idk;
}

mem::Client::CareerEntity* clientCareerEnt;
void UpdateClientCareerEntityPtr()
{
	clientCareerEnt = *(mem::Client::CareerEntity**)clientCareerEntityPtr;
}

uintptr_t gameCurrentLevelPtr = 0x289BDC8;

//
// Playlists
//

int GetPlaylistArrayConfigValue()
{
	std::string playlistSetArrayIDStr = ini.get(PlaylistCfg).get("ForcePlaylistSetArrayID");
	int playlistSetArrayID = StrToULong(playlistSetArrayIDStr);
	if (playlistSetArrayID < 0) { playlistSetArrayID = 0; }
	return playlistSetArrayID;
}

int GetPlaylistIDConfigValue()
{
	std::string playlistIDStr = ini.get(PlaylistCfg).get("ForcePlaylistID");
	int playlistID = StrToULong(playlistIDStr);
	if (playlistID < 0) { playlistID = 0; }
	return playlistID;
}

uintptr_t customPlaylistPtr;
// Get PlaylistSet to find our specified Playlist pointer.
void LoadPlaylistMap(uintptr_t setPtr)
{
	ebx::playlists::PlaylistSet* playlistSet = (ebx::playlists::PlaylistSet*)setPtr;
	ebx::playlists::PlaylistGroupArray groupArray { };

	groupArray.count = *(int*)(playlistSet->ar_PlaylistGroups - 0x4);

	for (int a{ 0 }; a < groupArray.count; a++)
	{
		groupArray.pt_PlaylistGroup.push_back(
			(*(int*)(playlistSet->ar_PlaylistGroups + (0x4 * a)) ) );
		//printf("### pt_PlaylistGroup 1st entry: %s\n", PtrToHexStr(groupArray.pt_PlaylistGroup[a]).c_str());

		ebx::playlists::PlaylistGroup playlistGroup { };
		playlistGroup.count = *(int*)(groupArray.pt_PlaylistGroup[a] - 0x4);

		for (int p{ 0 }; p < playlistGroup.count; p++)
		{
			playlistGroup.pt_Playlists.push_back(
				(*(int*)(groupArray.pt_PlaylistGroup[a] + (0x4 * p)) ) );
			//printf("### pt_Playlists entry: %s\n", PtrToHexStr(playlistGroup.pt_Playlists[p]).c_str());
		}
		groupArray.PlaylistGroups.push_back(playlistGroup);
	}

	// What Playlist array group, and Playlist ID we want?
	PlaylistSetArrayID = GetPlaylistArrayConfigValue();
	PlaylistID = GetPlaylistIDConfigValue();

	if (PlaylistSetArrayID > (groupArray.count - 1) ||
		PlaylistID > (groupArray.PlaylistGroups[PlaylistSetArrayID].count - 1))
	{
		PlaylistSetArrayID = 0;
		PlaylistID = 0;
		TestConsolePrint("!!! Playlist config values is not correct - all IDs have been reset to 0.\n");
	}
	customPlaylistPtr = groupArray.PlaylistGroups[PlaylistSetArrayID].pt_Playlists[PlaylistID];
	if (TestConsole)
	{
		printf("### Career_DefaultPlaylist: %s\n", PtrToHexStr(playlistSet->pt_DefaultPlaylist).c_str());
		printf("### Career_PlaylistSet size: %d\n", groupArray.count);
		printf("### Career_PlaylistPtr: %s\n\n", PtrToHexStr(customPlaylistPtr).c_str());
	}
}
void LoadPlaylistMapServer()
{
	LoadPlaylistMap(serverCareerEnt->PlaylistSet);
}
void LoadPlaylistMapClient()
{
	UpdateClientCareerEntityPtr();
	LoadPlaylistMap(clientCareerEnt->PlaylistSet);
}

//
// Client
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
	injector::WriteMemory<uint32_t>(0x2269335, EndianSwap(0xFC085AB0), true);
}

// By default, View Cars menu limits the car roster which can be saved as "base" player car.
// On come cases, even default unlocked special cars cannot be saved inside that menu.
// This tweak allows to save any chosen car from menu.
// Note: Locked status of the vehicle doesn't matter anymore, it depends on other check.
void EnableAllCarsAssignment()
{
	injector::MakeNOP(0x8848F5, 13);
}

// Pause state tricks to allow returning on FrontEnd level
void ModifyPlayerPauseState()
{
	// Force Single-Player state on one specific place, required to allow players use Quit option
	injector::WriteMemory<uint8_t>(0x926EE7, 0xB8, true);
	injector::WriteMemory<uint32_t>(0x926EE8, EndianSwap(0x01000000), true);
	// Replace True with False word: it will disable Game Time stop during pause
	injector::WriteMemory<uint32_t>(0xEC0572, EndianSwap(0xD00E3F02), true);
}

void EnableLANOnlineTweaks()
{
	if (ini.get(OnlineCfg).get("EnableLANOnlineTweaks") != trueStr)
	{
		return;
	}
	ForceSocketForClient();
	SwapPlaylistSPMessage();
	DisableSSLCertRequirement();
	EnableAllCarsAssignment();
	ModifyPlayerPauseState();
}

// Disable various driving assists #1
void DisablePrimaryDrivingAssists()
{
	if (ini.get(GameCfg).get("DisablePrimaryDrivingAssists") != trueStr)
	{
		return;
	}
	// Disable AlignToRoad
	injector::WriteMemory<uint8_t>(0x69B167, 0x74, true);
	// Disable OverrideDriftIntent
	injector::WriteMemory<uint8_t>(0x69B5E2, 0x75, true);
	// Switch RaceLineAssist status to RaceLineAssist_Off
	injector::WriteMemory<uint8_t>(0x1819984, 0x00, true);
	// Skip RaceLineAssist calculation
	injector::MakeNOP(0x18199A6, 6); 
	injector::MakeJMP(0x18199A6, 0x1819CDC);
	// Disables all RaceLineAssist forces
	injector::WriteMemory<uint8_t>(0x1819AB2, 0x85, true);
	// Disable Drifting Assists
	injector::MakeNOP(0x181AA62, 7);
	// Skip DriftIntents calculation
	injector::MakeNOP(0x1828E73, 6);
	injector::MakeJMP(0x1828E73, 0x18293A0);
}

// Disable various driving assists #2
void DisableSecondaryDrivingAssists()
{
	if (ini.get(GameCfg).get("DisableSecondaryDrivingAssists") != trueStr)
	{
		return;
	}
	// Disable DriftIntentAnalyzer
	injector::WriteMemory<uint8_t>(0x181A09C, 0x85, true);
	// Disable RaceLineAnalyzer
	injector::WriteMemory<uint8_t>(0x181A1EF, 0x75, true);
	// Disable RaceCarExtraForces
	injector::WriteMemory<uint8_t>(0x181A4D7, 0x74, true);
	// Disable RaceLineAssist states preparation
	injector::WriteMemory<uint8_t>(0x181A35E, 0x84, true);
	// Enable "Wallride Grief Protection" - it disables wheel control for 1 second after wall collision
	//injector::WriteMemory<uint8_t>(0x69B003, 0x74, true);
}

// Enhance drifting driving assists
void EnhanceDriftAssists()
{
	if (ini.get(GameCfg).get("EnhanceDriftAssists") == trueStr)
	{
		injector::WriteMemory<uint8_t>(0x69AF4D, 0x75, true);
	}
}

// Skips intro movies and sequence.
void ForceFastBoot()
{
	if (ini.get(GameCfg).get("GameFastBoot") == trueStr)
	{
		injector::WriteMemory<uint8_t>(0x8A9361, 0x01, true);
	}
}

// Disable Player respawns when driving backwards.
void DisableWrongWayRespawn()
{
	if (ini.get(GameCfg).get("DisableWrongWayRespawn") == trueStr)
	{
		injector::MakeNOP(0x808915, 6);
	}
}

// Disable Player respawns when driving outside of allowed route.
// Note: respawns will still happen on some places.
void DisableOutOfTrackRespawn()
{
	if (ini.get(GameCfg).get("DisableOutOfTrackRespawn") == trueStr)
	{
		injector::MakeNOP(0x7FAA8C, 3);
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
uintptr_t defaultWin32PlayerNamePtr = 0x23EB5A8;
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
		injector::WriteMemoryRaw(defaultWin32PlayerNamePtr, &clientCustomName[0], 16, true);
		// Display Player nickname instead of "You" label
		injector::WriteMemory<uint32_t>(0x832C32, defaultWin32PlayerNamePtr, true);
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

auto Exe_ClientCareerSetPlaylistData = reinterpret_cast<void(__thiscall*)(int careerPtr, int playlistPtr)>(0x84CEA0);
// This method also loads Playlist Objectives as well
void ClientCareerSetPlaylistData()
{
	LoadPlaylistMapClient(); 
	uintptr_t customPlaylistGUIDPtr = customPlaylistPtr - 0x10;
	Exe_ClientCareerSetPlaylistData(*(int*)clientCareerEntityPtr, customPlaylistGUIDPtr);
}

uintptr_t forceClientPlaylistAfterResetRetPtr = 0x84AB1C;
__declspec(naked) void ForceClientPlaylistAfterReset_asmPart()
{
	_asm
	{
		mov EAX, dword ptr [EDX + 0x18] // Original
		call EAX // Original, Call 0x850A20 (Reset ClientCareerEntity)
		call ClientCareerSetPlaylistData
		jmp forceClientPlaylistAfterResetRetPtr
	}
}

// Force Playlist pointer by default. Without it, Client instances will crash on Playlist mode loading.
// Since original sync request to load server Playlist is not kicks in, we force it manually.
// Originally, clients gets that pointers by proceeding through Frontend menus.
void ForceClientPlaylistPtrInMemory()
{
	if (ini.get(PlaylistCfg).get("ForceClientPlaylistPtrInMemory") == falseStr)
	{
		return;
	}
	injector::MakeNOP(0x1033DD5, 3); // Remove assignment to 0

	injector::MakeNOP(0x85DB93, 7);
	injector::MakeCALL(0x85DB93, ClientCareerSetPlaylistData);

	// Load Playlist & Objectives data again after Entity reset, during entering FrontEnd from Pause menu
	injector::MakeNOP(0x84AB17, 5);
	injector::MakeJMP(0x84AB17, ForceClientPlaylistAfterReset_asmPart);
	return;
}

uintptr_t forceGarageCarHashRetPtr1 = 0x897029;
__declspec(naked) void ForceGarageCarHash_asmPart1()
{
	_asm
	{
		push forcedGarageCarHash
		pop EAX
		mov dword ptr[ESI + 0xC], EAX
		jmp forceGarageCarHashRetPtr1
	}
}

uintptr_t forceGarageCarHashRetPtr2 = 0x896FF6;
__declspec(naked) void ForceGarageCarHash_asmPart2()
{
	_asm
	{
		push forcedGarageCarHash
		pop EAX
		mov dword ptr[EDI + 0xC], EAX
		jmp forceGarageCarHashRetPtr2
	}
}

uintptr_t forceGarageCarHashRetPtr3 = 0x89E5F3;
__declspec(naked) void ForceGarageCarHash_asmPart3()
{
	_asm
	{
		mov ESI, ECX // Original
		push forcedGarageCarHash
		pop EBX
		mov dword ptr[ESI + 0x10], EBX
		jmp forceGarageCarHashRetPtr3
	}
}

// Force Garage vehicle to your preferred choice by hash. Any Garage car choice will be discarded.
// Note: you must enter into View Cars menu at least once, to apply your car to Server.
void ForceGarageCarHash()
{
	std::string garageCarHash = ini.get(GameCfg).get("ForceGarageCarHash");
	if (garageCarHash == falseStr)
	{
		return;
	}
	forcedGarageCarHash = StrHashToULong(garageCarHash);
	TestConsolePrint("### ForceGarageCarHash: %s\n\n", SWIntToHexStr(forcedGarageCarHash).c_str());

	// Change initial Default car assignment #1
	injector::MakeNOP(0x897024, 9);
	injector::MakeJMP(0x897024, ForceGarageCarHash_asmPart1);
	injector::WriteMemory<uint32_t>(0x897029, EndianSwap(0x5EC20800), true);

	// Change initial Default car assignment #2
	injector::MakeNOP(0x896FF1, 10);
	injector::MakeJMP(0x896FF1, ForceGarageCarHash_asmPart2);
	injector::WriteMemory<uint8_t>(0x896FE7, 0x0E, true);
	injector::WriteMemory<uint8_t>(0x896FF6, 0x5F, true);
	injector::WriteMemory<uint32_t>(0x896FF7, EndianSwap(0x5EC20800), true);

	// Change Garage car choice assignment
	injector::MakeNOP(0x89E5EE, 5);
	injector::MakeJMP(0x89E5EE, ForceGarageCarHash_asmPart3);
}

uintptr_t gameLanguage = 0x273DF70;
auto Exe_PickGameLanguage = reinterpret_cast<int(*)(char* langCodePtr)>(0x1887CC0);
void SetGameLanguage_InGamePart()
{
	uint32_t langId = Exe_PickGameLanguage(langCode);
	injector::WriteMemory<uint32_t>(gameLanguage, langId, true);
}

// Set game Language without Registry values.
void SetGameLanguage()
{
	std::string langCodeStr = ini.get(GameCfg).get("SetGameLanguage");
	if (langCodeStr == falseStr)
	{
		return;
	}
	strcpy(langCode, langCodeStr.c_str());
	injector::MakeNOP(0x1897A2E, 5); // Disable one of assignments
	injector::MakeCALL(0x1890DF0, SetGameLanguage_InGamePart);
	injector::MakeCALL(0x1897A22, SetGameLanguage_InGamePart);
	TestConsolePrint("### Selected config Language: %s\n", langCode);
}

//
// Server
//

void ExperimentalSinglePlayerCoopTweaks()
{
	if (ini.get(ServerCfg).get("ExperimentalSinglePlayerCoopTweaks") != trueStr)
	{
		return;
	}
	// Enable AI spawning on Multiplayer mode
	// Depends on 027A4F73 = 1, which is usually always enabled
	// Doesn't work for Online Playlists, due to missing network messages
	injector::MakeNOP(0x013230FA, 6);
	injector::WriteMemory<uint8_t>(0x27A8253, 0x1, true);

	// Avoid server-logic crash when Cops appear
	injector::WriteMemory<uint8_t>(0x125482A, 0x31, true);
	injector::WriteMemory<uint8_t>(0x125482B, 0xC0, true);
	injector::WriteMemory<uint8_t>(0x125482C, 0x90, true);

}

float customTrafficDensity;
uintptr_t trafficDensityRetPtr = 0x125EEEE;
__declspec(naked) void OverrideTrafficDensity_asmPart()
{
	_asm
	{
		movss XMM2, customTrafficDensity
		jmp trafficDensityRetPtr
	}
}

// Override Session Traffic settings
// Note: on some routes and places, Traffic amount will be still controlled by specific track settings.
void OverrideTrafficSettings()
{
	if (ini.get(ServerCfg).get("OverrideTrafficSettings") != trueStr)
	{
		return;
	}
	customTrafficDensity = std::stof(ini.get(ServerCfg).get("CustomTrafficDensity"));
	CheckForMaxMinFloat(customTrafficDensity, 0.0, 1.0);

	// Game doesn't attempt to spawn more than 50 traffic cars, even without limit.
	// Gameplay stability is not guaranteed after original 25 car limit.
	uint32_t customTrafficCarLimit = std::stoi(ini.get(ServerCfg).get("CustomTrafficCarLimit"));
	CheckForMaxMinInt(customTrafficCarLimit, 0, 50);

	TestConsolePrint("### Loaded Traffic override Density: %f, Car Limit: %d.\n\n",
		customTrafficDensity, customTrafficCarLimit);

	// Density
	injector::MakeNOP(0x125EEE9, 5);
	injector::MakeJMP(0x125EEE9, OverrideTrafficDensity_asmPart);

	// Car Limit
	injector::MakeNOP(0x125A9A0, 11); // Delete assignment and limit check (25)
	injector::WriteMemory<uint8_t>(0x125A9AC, customTrafficCarLimit, true);
}

// TODO Not ideal, any Client can force FrontEnd return by wish
void CheckForFrontEndLevelLoading()
{
	gameCurrentLevelStr = (char*)gameCurrentLevelPtr;
	if (strcmp(gameCurrentLevelStr, frontendLevel) == 0)
	{
		serverCareerEnt->CareerState = 0x2;
		serverCareerEnt->CareerStateTimer = 0.0;
		serverCareerEnt->PlaylistRouteId = 0xFFFFFFFF;
		TestConsolePrint("### Entered on FrontEnd level, CareerState is restarted.\n\n");
	}
}

uintptr_t checkForFrontEndLevelLoadingRetPtr = 0x11A46B9;
__declspec(naked) void CheckForFrontEndLevelLoading_asmPart()
{
	_asm
	{
		push EAX
		push ECX
		call CheckForFrontEndLevelLoading
		pop ECX
		pop EAX
		mov EDX, 0x289BCC8 // Original
		jmp checkForFrontEndLevelLoadingRetPtr
	}
}

// Force Career Mode into PreLoad state. By default, server always starts with 0 (disabled Career Mode).
void ForceCareerStatePreLoad()
{
	if (ini.get(ServerCfg).get("ForceCareerStatePreLoad") != trueStr)
	{
		return;
	}
	// Disable initial CareerState assignment to 0
	injector::MakeNOP(0x1401BEF, 6);

	// Useful if Server loads on level different than FrontEnd
	serverCareerEnt->CareerState = 0x2; 

	// Check for FrontEnd level loading
	injector::MakeNOP(0x11A46B4, 5);
	injector::MakeJMP(0x11A46B4, CheckForFrontEndLevelLoading_asmPart);
}

uintptr_t playlistLoaderRetPtr = 0x0140016E;
__declspec(naked) void ReplacePlaylistDefaultID_asmPart()
{
	_asm
	{
		mov EAX, dword ptr[ESI + 0xC] // Original Playlist Set loading
		call LoadPlaylistMapServer
		push customPlaylistPtr
		pop ECX // Load our Playlist instead of default one [EAX + 0x10]
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
	// Force Playlist data to be loaded earlier than intended.
	// As a consequence, first selected Route ID will be 2nd instead of 1st.
	injector::MakeNOP(0x14011C8, 5);
	injector::WriteMemory<uint8_t>(0x14011CF, 0x2, true);
	injector::WriteMemory<uint8_t>(0x14011D1, 0xC, true);

	// Commands -NfsGame.FELevel AnyWordHereToBreakIt and -Game.DefaultLayerInclusion StartupMode=Game;GameMode=CareerMode
	// allows to automatically boot Playlist data, but it breaks later on Intermission.

	// Replace Default Playlist with our custom one
	injector::MakeNOP(0x1400168, 6);
	injector::MakeJMP(0x1400168, ReplacePlaylistDefaultID_asmPart);
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
	serverCareerEnt->PlaylistSessionId = sessionID;
	TestConsolePrint("### Career_PlaylistSessionId: %d\n\n", serverCareerEnt->PlaylistSessionId);
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

// TODO Not so safe to patch the code when running that code, make it differently
void SaveRandomSessionId()
{
	injector::WriteMemory<uint8_t>(0x01400182, serverCareerEnt->PlaylistSessionId, true);
	TestConsolePrint("### Career_PlaylistSessionId: %d\n\n", serverCareerEnt->PlaylistSessionId);
}

uintptr_t fixPlaylistSessionRandomizerRetPtr = 0x13FFE49;
__declspec(naked) void FixPlaylistSessionRandomizer_asmPart()
{
	_asm
	{
		mov dword ptr[ESI + 0x1084], 0x1 // Original
		call SaveRandomSessionId
		jmp fixPlaylistSessionRandomizerRetPtr
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
	injector::MakeNOP(0x13FFE3F, 10);
	injector::MakeJMP(0x13FFE3F, FixPlaylistSessionRandomizer_asmPart);
}

std::vector<uint32_t> forcedServerCarHashes;
bool isSingleServerCarHash = false;
uint32_t pickedServerCarHash;
std::uniform_int_distribution<int> carPickRandRegion;
// TODO Currently, it will change players cars every race, instead of every Session.
// Player ID connections must be found first
void GetRandomServerCarHash()
{
	if (!isSingleServerCarHash)
	{
		// This function executes twice for each player (Car Select + PreRace stages)
		int randomIndex = carPickRandRegion(rng);
		pickedServerCarHash = forcedServerCarHashes[randomIndex];
	}
}

uintptr_t forceServerPlayersCarRetPtr = 0x13EAFAE;
__declspec(naked) void ForceServerPlayersCar_asmPart()
{
	_asm
	{
		call GetRandomServerCarHash
		push pickedServerCarHash
		pop EAX
		jmp forceServerPlayersCarRetPtr
	}
}

// Force user-specified car hashes to all Clients. Changes being applied during Intermission screens.
// More than 1 car hash will be forced to players by random.
void ForceServerPlayersCar()
{
	std::string forcedServerCarHashStr = ini.get(ServerCfg).get("ForceServerPlayersCarHash");
	if (forcedServerCarHashStr == falseStr)
	{
		return;
	}
	std::vector<std::string> forcedServerCarHashArray = SplitStr(forcedServerCarHashStr, ',');
	for (unsigned int i = 0; i < forcedServerCarHashArray.size(); i++)
	{
		uint32_t carHashInt = StrHashToULong(forcedServerCarHashArray[i]);
		forcedServerCarHashes.push_back(carHashInt);
	}

	TestConsolePrint("### ForceServerPlayersCarHashes: %s\n\n", 
		ConcatStrVec(forcedServerCarHashArray, ',').c_str());
	if (forcedServerCarHashes.size() < 2)
	{
		isSingleServerCarHash = true;
		pickedServerCarHash = forcedServerCarHashes[0];
	}
	else
	{
		InitRandomGen();
		std::uniform_int_distribution<int> carPickRandRegionTmp(0, forcedServerCarHashes.size() - 1);
		carPickRandRegion = carPickRandRegionTmp;
	}

	injector::MakeNOP(0x13EAFA8, 6);
	injector::MakeJMP(0x13EAFA8, ForceServerPlayersCar_asmPart);
}

// Disable Debug Car change attempts during events.
void DisableDebugCarChangeInRace()
{
	if (ini.get(ServerCfg).get("DisableDebugCarChangeInRace") == trueStr)
	{
		injector::WriteMemory<uint8_t>(0x13CDC8C, 0xEB, true);
	}
}

// Due to ForcePlaylistLoading tweak, server will try to skip first event on boot.
// TODO Find a better way
void ResetPlaylistRouteIDTweak()
{
	if (ini.get(ServerCfg).get("ForcePlaylistLoading") == trueStr)
	{
		serverCareerEnt->PlaylistRouteId = 0xFFFFFFFF;
	}
}

//
// Status data
//

void WndTitlePlaylistStatus()
{
	if (onlineEnt->ConnectionState != 3 || strlen(gameCurrentLevelStr) == 0 || 
		strcmp(gameCurrentLevelStr, frontendLevel) == 0 ||
		ini.get(PlaylistCfg).get("ForceClientPlaylistPtrInMemory") == falseStr)
	{
		return;
	}
	//TestConsolePrint("### careerEnt PlaylistSessionId: %d\n", careerEnt->PlaylistSessionId);
	//TestConsolePrint("### careerEnt PlaylistRouteId: %d\n\n", careerEnt->PlaylistRouteId);
	snprintf(wndTitle, 100, "%sPlaylist A%d #%d | Session: %d, Race: %d",
		GameStatus::baseModTitle, PlaylistSetArrayID, PlaylistID,
		clientCareerEnt->PlaylistSessionId, clientCareerEnt->PlaylistRouteId + 1);
}

void WndTitleConnectionStatus()
{
	uint32_t connectStatusId = onlineEnt->ConnectionState;
	// We can't just compare CareerState, because it's not always being reset
	if (connectStatusId == 3 && strlen(gameCurrentLevelStr) > 0 
			&& strcmp(gameCurrentLevelStr, frontendLevel) != 0)
	{
		return;
	}
	CheckForMaxMinInt(connectStatusId, 0, GameStatus::connectStatus.size());
	TestConsolePrint("### connectStatusId: %s\n\n", PtrToHexStr(connectStatusId).c_str());
	snprintf(wndTitle, 100, "%s%s", GameStatus::baseModTitle, GameStatus::connectStatus[connectStatusId].c_str());
}

// Update Game window Title to display various information.
void WndTitleStatus()
{
	if (ini.get(OnlineCfg).get("EnableWndTitleStatus") != trueStr ||
		(IsOnlineEntityExists() && onlineEnt->IsSinglePlayer == 1) )
	{
		return; // Disable title updates on single-player
	}
	// Disable original title changes during connection attempts
	injector::MakeNOP(0xE5FE52, 18); 

	UpdateClientCareerEntityPtr();

	HWND hWnd = FindWindowA(gameName, gameName);
	while (hWnd == 0)
	{
		std::this_thread::sleep_for(wndTitleUpdateTimeMs);
		hWnd = FindWindowA(gameName, gameName);
	}

	while (true)
	{
		snprintf(wndTitleCopy, 100, "%s", wndTitle);
		snprintf(wndTitle, 100, "%s", gameName);
		
		//TestConsolePrint("### ClientOnlineEntity Ptr: %s\n", PtrToHexStr(*(int*)clientOnlineEntityPtr).c_str());
		if (IsOnlineEntityExists() && onlineEnt->IsSinglePlayer == 0)
		{
			gameCurrentLevelStr = (char*)gameCurrentLevelPtr;
			TestConsolePrint("### GameCurrentLevel: %s\n", gameCurrentLevelStr);
			WndTitleConnectionStatus();
			WndTitlePlaylistStatus();
		}
		if (strcmp(wndTitle, wndTitleCopy) != 0)
		{
			SetWindowText(hWnd, wndTitle);
		}
		std::this_thread::sleep_for(wndTitleUpdateTimeMs);
	}
}

//
// Loading
//

void InitClientPreLoadHelper()
{
	SetGameLanguage();
	EnableLANOnlineTweaks();
	EnableDebugModeMenus();
	ForceFastBoot();
	ForceCustomInitFlow();

	ClientComputerNameTweaks();
	DisableAutologConnectAttempts();
	ForceClientPlaylistPtrInMemory();
	ForceGarageCarHash();
	
	DisableWrongWayRespawn();
	DisableOutOfTrackRespawn();
	DisablePrimaryDrivingAssists();
	DisableSecondaryDrivingAssists();
	EnhanceDriftAssists();
}

void InitServerPreLoadHelper()
{
	serverCareerEnt = (mem::Server::CareerEntity*)serverCareerEntityPtr;
	ForceServerPlaylistLoading();
	ForceServerPlayersCar();
	DisableDebugCarChangeInRace();
	ForceCareerStatePreLoad();
	DisableSSLCertRequirement();
	FixPlaylistSessionRandomizer();
	OverrideTrafficSettings();
	ExperimentalSinglePlayerCoopTweaks();
}

void InitClientThreadedHelper()
{
	std::this_thread::sleep_for(helperSleepMs);
	WndTitleStatus();
}

void InitServerThreadedHelper()
{
	std::this_thread::sleep_for(helperSleepMs); // TODO Works for now
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
	CheckGameExecutableType();

	mINI::INIFile file("NFSTR_LANOnlineHelper.ini");
	file.read(ini);
	
	if (ini.get(GameCfg).get("EnableTestConsole") == trueStr)
	{
		TestConsole = true;
		StartTestConsole();
	}

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
