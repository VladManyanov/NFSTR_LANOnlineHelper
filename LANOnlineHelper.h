#include <string>
#pragma once

// Difference between Client and Dedicated Server executables
const char clientHex[] = "16 51 6A 00 52 8B C8 E8";
const char serverHex[] = "16 51 52 6A 01 8B C8 E8";

const char gameName[] = "Need for Speed(TM) The Run";

const char frontendLevel[] = "_c4/Levels/FE/FrontEnd/FrontEnd";

namespace GameStatus
{
	const char baseModTitle[] = "NFSTR LANs: ";
	std::string connectStatus[] = { "Disconnected", "Pending Connection", "Attempting to Connect...", "Connected", "Disconnecting..." };
	const char onlineEntityNotAvailable[] = "Offline";
}
