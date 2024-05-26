#include <string>
#include <vector>
#include <chrono>
#pragma once

// Difference between Client and Dedicated Server executables
const char clientHex[] = "16 51 6A 00 52 8B C8 E8";
const char serverHex[] = "16 51 52 6A 01 8B C8 E8";

const char gameName[] = "Need for Speed(TM) The Run";
const char frontendLevel[] = "_c4/Levels/FE/FrontEnd/FrontEnd";

std::string GameCfg = "GameSettings";
std::string OnlineCfg = "OnlineSettings";
std::string ServerCfg = "ServerSettings";
std::string PlaylistCfg = "PlaylistSettings";
// mINI lib gives only strings, so...
std::string trueStr = "1";
std::string falseStr = "0";

std::chrono::milliseconds helperSleepMs(4000);

namespace GameStatus
{
	const char baseModTitle[] = "NFSTR LANs: ";
	std::vector<std::string> connectStatus = { "Disconnected", "Pending Connection", "Attempting to Connect...", "Connected", "Disconnecting..." };
	const char onlineEntityNotAvailable[] = "Offline";
}
