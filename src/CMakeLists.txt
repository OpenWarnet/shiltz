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
	"LoginDispatcher.cpp"
	"LoginDispatcher.h"
	"LoginOpcodes.h"
	"LoginServer.h"
	"LoginServer.cpp"
	"common/PayloadWriter.h"
	"models/Character.cpp" 
	"models/Character.h"
	"models/CharacterSelection.h"
	"models/CharacterSelection.cpp" 
	"models/ServerList.h"
	"models/ServerList.cpp" 
	"models/ServerSelect.h" 
	"models/ServerSelect.cpp" 
	"common/PayloadReader.h" 
	"models/Login.h" 
	"models/Login.cpp" 
	"models/GenericCharacterPayload.cpp" 
	"models/GenericCharacterPayload.h" 
	"models/CreateCharacter.h" 
	"models/SetCharacterMap.h" 
	"models/SetCharacterMap.cpp" 
	"models/CreateCharacter.cpp" "models/GameConnect.h" "models/GameConnect.cpp"
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
	"GameDispatcher.h"
	"GameDispatcher.cpp"
	"GameOpcodes.h"
	"GameServer.h"
	"GameServer.cpp"
	"common/PayloadWriter.h"
	"common/PayloadReader.h"
	"common/TCPServer.h"
	"common/TCPServer.cpp"
	"models/CharacterDataLoad.h"
	"models/CharacterDataLoad.cpp"
	"models/GameEnter.h"
	"models/GameEnter.cpp"
	"models/InventoryItemList.h"
	"models/InventoryItemList.cpp"
	"models/CrtLoad.h"
	"models/CrtLoad.cpp"
	"models/CharExitSucc.h"
	"models/CharExitSucc.cpp")

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
