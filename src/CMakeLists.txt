# CMakeList.txt : CMake project for shiltz, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (
	login 
	"login.cpp"
	"login.h"
	"cipher/DESCipher.cpp"
	"cipher/DESCipher.h"
	"LoginPacket.cpp"
	"LoginPacket.h"
	"login/LoginDispatcher.cpp"
	"login/LoginDispatcher.h"
	"login/LoginOpcodes.h"
	"login/LoginServer.h"
	"login/LoginServer.cpp"
	"common/Dispatcher.h"
	"common/PayloadWriter.h"
	"login/server/Character.cpp"
	"login/server/Character.h"
	"login/server/CharacterSelection.h"
	"login/server/CharacterSelection.cpp"
	"login/server/ServerList.h"
	"login/server/ServerList.cpp"
	"login/client/ServerSelect.h"
	"login/client/ServerSelect.cpp"
	"common/PayloadReader.h"
	"login/client/Login.h"
	"login/client/Login.cpp"
	"login/shared/GenericCharacterPayload.cpp"
	"login/shared/GenericCharacterPayload.h"
	"login/client/CreateCharacter.h"
	"login/client/SetCharacterMap.h"
	"login/client/SetCharacterMap.cpp"
	"login/client/CreateCharacter.cpp"
	"login/client/GameConnect.h"
	"login/client/GameConnect.cpp"
	"login/server/GameConnectSuccess.h"
	"login/server/GameConnectSuccess.cpp"
	"common/TCPServer.h"
	"common/TCPServer.cpp")

add_executable (
	game
	"game.h"
	"game.cpp"
	"cipher/BlowfishCipher.cpp"
	"cipher/BlowfishCipher.h"
	"GamePacket.cpp"
	"GamePacket.h"
	"game/GameDispatcher.h"
	"game/GameDispatcher.cpp"
	"game/GameOpcodes.h"
	"game/GameServer.h"
	"game/GameServer.cpp"
	"common/Dispatcher.h"
	"common/PayloadWriter.h"
	"common/PayloadReader.h"
	"common/TCPServer.h"
	"common/TCPServer.cpp"
	"game/server/CharacterDataLoad.h"
	"game/server/CharacterDataLoad.cpp"
	"game/client/GameEnter.h"
	"game/client/GameEnter.cpp"
	"game/server/InventoryItemList.h"
	"game/server/InventoryItemList.cpp"
	"game/server/CrtLoad.h"
	"game/server/CrtLoad.cpp"
	"game/server/CharExitSucc.h"
	"game/server/CharExitSucc.cpp")

add_executable (
	client
	"client.cpp")


target_include_directories(game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(login PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
find_package(cryptopp CONFIG REQUIRED)
target_link_libraries(game PRIVATE cryptopp::cryptopp)
target_link_libraries(login PRIVATE cryptopp::cryptopp)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET game PROPERTY CXX_STANDARD 20)
  set_property(TARGET login PROPERTY CXX_STANDARD 20)
  set_property(TARGET client PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
