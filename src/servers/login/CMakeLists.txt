# CMakeList.txt : CMake project for the login server executable.
#

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
	"protocol/server/Character.cpp"
	"protocol/server/Character.h"
	"protocol/server/CharacterSelection.h"
	"protocol/server/CharacterSelection.cpp"
	"protocol/server/ServerList.h"
	"protocol/server/ServerList.cpp"
	"protocol/client/ServerSelect.h"
	"protocol/client/ServerSelect.cpp"
	"protocol/client/Login.h"
	"protocol/client/Login.cpp"
	"shared/GenericCharacterPayload.cpp"
	"shared/GenericCharacterPayload.h"
	"protocol/client/CreateCharacter.h"
	"protocol/client/SetCharacterMap.h"
	"protocol/client/SetCharacterMap.cpp"
	"protocol/client/CreateCharacter.cpp"
	"protocol/client/GameConnect.h"
	"protocol/client/GameConnect.cpp"
	"protocol/server/GameConnectSuccess.h"
	"protocol/server/GameConnectSuccess.cpp"
	"${CMAKE_SOURCE_DIR}/src/common/Dispatcher.h"
	"${CMAKE_SOURCE_DIR}/src/common/PayloadWriter.h"
	"${CMAKE_SOURCE_DIR}/src/common/PayloadReader.h"
	"${CMAKE_SOURCE_DIR}/src/common/TCPServer.h"
	"${CMAKE_SOURCE_DIR}/src/common/TCPServer.cpp")

target_include_directories(login PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

find_package(cryptopp CONFIG REQUIRED)
target_link_libraries(login PRIVATE cryptopp::cryptopp)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET login PROPERTY CXX_STANDARD 20)
endif()
