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

void removeSpaces(std::string str)
{
	str.erase(remove(str.begin(), str.end(), ' '), str.end());
}

//

// Disable one of connection-related functions, sometimes helps to solve server connection issues.
void DisableUnkNetworkConnectionFunc()
{
	injector::MakeNOP(0x834303, 2);
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

void EnableLANOnlineTweaks()
{
	if (ini.get(OnlineCfg).get("EnableLANOnlineTweaks") == trueStr)
	{
		//DisableUnkNetworkConnectionFunc();
		ForceSocketForClient();
		SwapPlaylistSPMessage();
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

// Erase EA Gosredirector address, to make any failure Autolog connection timeouts faster.
void EraseRedirectorAddress()
{
	if (ini.get(OnlineCfg).get("EraseRedirectorAddress") == trueStr)
	{
		injector::MakeNOP(0x2634EA0, 20);
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
	Sleep(3000); // TODO Work with pointer instead of that
	injector::WriteMemoryRaw(0xF3D42BCC, &carHashInt, 4, true);
}

void InitClientPreLoadHelper()
{
	EnableLANOnlineTweaks();
	EnableDebugModeMenus();
	ForceFastBoot();
	ForceCustomInitFlow();
	ClientComputerNameTweaks();
	EraseRedirectorAddress();
}

void InitServerPreLoadHelper()
{
	ForcePlaylistSession();
	ForceCareerStatePreLoad();
}

void InitClientThreadedHelper()
{
	ChangeDebugCarHash(); // Sleep timer here!
}

void InitServerThreadedHelper()
{
	//
	return;
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
