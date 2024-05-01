//
// Need for Speed The Run - Helper script for custom LAN online gameplay
// by Hypercycle / And799 / mRally2
// code project based on "NFS The Run - Ultimate Unlocker" by Tenjoin
//

#define WIN32_LEAN_AND_MEAN
#include <thread>
#include "includes/injector/injector.hpp"
#include "includes/mini/ini.h"
#include "includes/patterns.hpp"

//#define TESTCONSOLE

// mINI lib gives only strings, so...
std::string trueStr = "1";
std::string falseStr = "0";

std::string GameCfg = "GameSettings";
std::string OnlineCfg = "OnlineSettings";
std::string ServerCfg = "ServerSettings";

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

// Disable Playlist Session randomizer, and force custom Session ID.
// Note: Ending Vote results will be ignored, and the same session will be restarted.
void ForcePlaylistSession()
{
	if (ini.get(ServerCfg).get("DisablePlaylistSessionRandomizer") != trueStr)
	{
		return;
	}
	// Disable initial Session ID assignment to 0
	injector::MakeNOP(0x1033DCC, 3);
	// Disable Playlist Session ID update from client messages
	injector::MakeNOP(0x13FBE77, 3);

	std::string sessionIDParam = ini.get(ServerCfg).get("ForcePlaylistSessionID");
	// Expecting int here, not a hex
	uint32_t sessionID = strtoul(sessionIDParam.c_str(), 0, 10);
	injector::WriteMemory<uint8_t>(0x27A3304, sessionID, true);
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
// Possible variants: 
// BootFlow, StartScreen, MainMenu
// TheRun, ChallengeSeries
// Career, OnlineLobby, Playgroup
void ForceCustomInitFlow()
{
	std::string initFlow = ini.get(GameCfg).get("ForceInitFlow");
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

// Force Playlist pointer by default. Without it, game instances will crash on Playlist mode loading.
// Due to lack of sync requests to load server Playlist, we force it manually.
// Originally, clients gets this pointer by proceeding through Frontend menus.
void ForcePlaylistDefaultID()
{
	std::string playlistDefaultID = ini.get(ServerCfg).get("ForcePlaylistDefaultID");
	if (playlistDefaultID == falseStr)
	{
		return;
	}
	// TODO Load custom PlaylistSetArray and specified ID on it
	uintptr_t playlistSet_ptr = *(int*)0x27A330C;
	uintptr_t customPlaylistPtr = *(int*)(playlistSet_ptr + 0x28);
#ifdef TESTCONSOLE
	printf("### CSM_Playlist_Ptr: %s\n", PtrToHexStr(customPlaylistPtr).c_str());
#endif
	
	if (IsClient)
	{
		uintptr_t clientPlaylistPtr = *(int*)0x28823BC;
		injector::WriteMemoryRaw(clientPlaylistPtr + 0x10, &customPlaylistPtr, 4, true);
		return;
	}
	if (IsServer)
	{
		uint32_t serverPlaylist = 0x27A3310;
		injector::WriteMemoryRaw(serverPlaylist, &customPlaylistPtr, 4, false);
	}
}

// Swap one of cars from Debug Car List, to your preferred vehicle by hash.
// Effectively replaces "Audi Quattro 20V - NFS Edition" vehicle on the list.
void ChangeDebugCarHash()
{
	std::string debugCarHash = ini.get(GameCfg).get("ChangeDebugCarHash");
	if (debugCarHash == falseStr)
	{
		return;
	}
	uint32_t carHashInt = strtoul(debugCarHash.c_str(), 0, 16);
	carHashInt = EndianSwap(carHashInt);

	// Prevent game crash and wait for Car List init
	// TODO Work with pointer instead of that
	injector::WriteMemoryRaw(0xF3D42BCC, &carHashInt, 4, true);
}

void InitClientPreLoadHelper()
{
	EnableLANOnlineTweaks();
	EnableDebugModeMenus();
	ForceFastBoot();
	ForceCustomInitFlow();
	ClientComputerNameTweaks();
	DisableAutologConnectAttempts();
}

void InitServerPreLoadHelper()
{
	ForcePlaylistSession();
	ForceCareerStatePreLoad();
	DisableSSLCertRequirement();
}

void InitClientThreadedHelper()
{
	Sleep(3000);
	ChangeDebugCarHash();
	ForcePlaylistDefaultID();
}

void InitServerThreadedHelper()
{
	Sleep(3000);
	ForcePlaylistDefaultID();
}

// Apply tweaks, depending on the type of game executable.
void checkGameExecutableType()
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
	checkGameExecutableType();

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
