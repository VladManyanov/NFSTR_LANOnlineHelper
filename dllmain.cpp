//
// Need for Speed: The Run - Helper script for custom LAN online gameplay
// by Hypercycle / And799 / mRally2
//

#define WIN32_LEAN_AND_MEAN
#include "LANOnlineHelper.h"

#include <thread>
#include "includes/injector/injector.hpp"
#include "includes/injector/assembly.hpp"
#include "includes/mini/ini.h"
#include "includes/patterns.hpp"

//

// mINI lib gives only strings, so...
std::string trueStr = "1";
std::string falseStr = "0";

std::string GameCfg = "GameSettings";
std::string OnlineCfg = "OnlineSettings";
std::string ServerCfg = "ServerSettings";
std::string PlaylistCfg = "PlaylistSettings";

bool IsClient = false;
bool IsServer = false;
bool TestConsole = false;

char wndTitle[100];
char wndTitleCopy[100];
char* gameCurrentLevelStr = '\0';

int PlaylistSetArrayID = 0;
int PlaylistID = 0;

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

template<typename... Args>
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
	}
}
mem::Client::OnlineEntity* onlineEnt;
mem::Client::CareerEntity* careerEnt;

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

//

uintptr_t playlistSet_ptr = 0x27A330C;
uintptr_t customPlaylistPtr;
// Get PlaylistSet to find our specified Playlist pointer.
void LoadPlaylistMap()
{
	ebx::playlists::PlaylistSet* playlistSet = *(ebx::playlists::PlaylistSet**)playlistSet_ptr;
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

uintptr_t playlistLoaderRetPtr = 0x010383A2;
__declspec(naked) void ForcePlaylistLoading_asmPart()
{
	_asm
	{
		call LoadPlaylistMap
		push customPlaylistPtr
		pop EAX
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
	TestConsolePrint("### CSM_PlaylistSessionId: %d\n\n", sessionID);
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
	injector::WriteMemory<uint8_t>(0x01400182, currentSessionId, true);
	TestConsolePrint("### Career_PlaylistSessionId: %d\n\n", currentSessionId);
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

// Due to ForcePlaylistLoading tweak, server will try to skip first event on boot.
void ResetPlaylistRouteIDTweak()
{
	if (ini.get(ServerCfg).get("ForcePlaylistLoading") == trueStr)
	{
		injector::WriteMemory<uint32_t>(0x27A3308, 0xFFFFFFFF, false);
	}
}

auto Exe_ClientCareerSetObjectives = reinterpret_cast<void(__fastcall*)(int ptr)>(0x84A350);
void ClientCareerSetObjectives()
{
	Exe_ClientCareerSetObjectives(*(int*)clientCareerEntityPtr);
}

uintptr_t playlistPtrRetPtr = 0x0085DB9A;
__declspec(naked) void ForcePlaylistPtrInMemory_asmPart()
{
	_asm
	{
		call LoadPlaylistMap
		push customPlaylistPtr
		pop ECX
		mov dword ptr [EBP + 0x10], ECX // Set Playlist ptr on Client CareerMode info area
		call ClientCareerSetObjectives // Now we must load Playlist Objectives
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

void WndTitlePlaylistStatus()
{
	if (onlineEnt->ConnectionState != 3 || strlen(gameCurrentLevelStr) == 0 || 
		strcmp(gameCurrentLevelStr, frontendLevel) == 0 ||
		ini.get(PlaylistCfg).get("ForceClientPlaylistPtrInMemory") == falseStr)
	{
		return;
	}
	careerEnt = *(mem::Client::CareerEntity**)clientCareerEntityPtr;
	//TestConsolePrint("### careerEnt PlaylistSessionId: %d\n", careerEnt->PlaylistSessionId);
	//TestConsolePrint("### careerEnt PlaylistRouteId: %d\n\n", careerEnt->PlaylistRouteId);
	snprintf(wndTitle, 100, "%sPlaylist A%d #%d | Session: %d, Race: %d",
		GameStatus::baseModTitle, PlaylistSetArrayID, PlaylistID,
		careerEnt->PlaylistSessionId, careerEnt->PlaylistRouteId + 1);
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
	if (connectStatusId > sizeof(GameStatus::connectStatus) || connectStatusId < 0)
	{
		connectStatusId = 0;
	}
	TestConsolePrint("### connectStatusId: %s\n\n", PtrToHexStr(connectStatusId).c_str());
	snprintf(wndTitle, 100, "%s%s", GameStatus::baseModTitle, GameStatus::connectStatus[connectStatusId].c_str());
}

uintptr_t gameCurrentLevelPtr = 0x289BDC8;
// Update Game window Title to display various information.
void WndTitleStatus()
{
	if (ini.get(OnlineCfg).get("EnableWndTitleStatus") == falseStr)
	{
		return;
	}
	// Disable original title changes during connection attempts
	injector::MakeNOP(0xE5FE52, 18); 
	HWND hWnd = FindWindowA(gameName, gameName);

	while (true)
	{
		snprintf(wndTitleCopy, 100, "%s", wndTitle);
		snprintf(wndTitle, 100, "%s", gameName);
		
		// On some cases, Online entity will be null
		bool isOnlineEntityExists = *(int*)clientOnlineEntityPtr != 0;
		TestConsolePrint("### ClientOnlineEntity Ptr: %s\n", PtrToHexStr(*(int*)clientOnlineEntityPtr).c_str());
		if (isOnlineEntityExists)
		{
			onlineEnt = *(mem::Client::OnlineEntity**)clientOnlineEntityPtr;
			if (onlineEnt->IsSinglePlayer == 0)
			{
				gameCurrentLevelStr = (char*)gameCurrentLevelPtr;
				TestConsolePrint("### GameCurrentLevel: %s\n", gameCurrentLevelStr);
				WndTitleConnectionStatus();
				WndTitlePlaylistStatus();
			}
		}
		if (strcmp(wndTitle, wndTitleCopy) != 0)
		{
			SetWindowText(hWnd, wndTitle);
		}
		Sleep(2000);
	}
	
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
	TestConsolePrint("### ChangeDebugCarHash: %s\n\n", SWIntToHexStr(carHashInt).c_str());

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
	WndTitleStatus();
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
