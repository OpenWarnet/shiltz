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
	"LoginOpcodes.cpp"
	"handlers/Auth.cpp"
	"handlers/Auth.h"
	"handlers/Character.cpp"
	"handlers/Character.h"
	"handlers/GameHandover.cpp"
	"handlers/GameHandover.h"
	"LoginServer.h"
	"LoginServer.cpp"
	"LoginSessionStore.h"
	"LoginSessionStore.cpp"
	"protocol/server/Character.cpp"
	"protocol/server/Character.h"
	"protocol/server/CharacterSelection.h"
	"protocol/server/CharacterSelection.cpp"
	"protocol/server/CreateCharacterFail.h"
	"protocol/server/CreateCharacterFail.cpp"
	"protocol/server/GameConnectSuccess.h"
	"protocol/server/GameConnectSuccess.cpp"
	"protocol/server/LoginFail.h"
	"protocol/server/LoginFail.cpp"
	"protocol/server/ServerList.h"
	"protocol/server/ServerList.cpp"
	"protocol/client/ServerSelect.h"
	"protocol/client/ServerSelect.cpp"
	"protocol/client/Login.h"
	"protocol/client/Login.cpp"
	"protocol/shared/GenericCharacterPayload.cpp"
	"protocol/shared/GenericCharacterPayload.h"
	"protocol/client/CreateCharacter.h"
	"protocol/client/SetCharacterMap.h"
	"protocol/client/SetCharacterMap.cpp"
	"protocol/client/CreateCharacter.cpp"
	"protocol/client/GameConnect.h"
	"protocol/client/GameConnect.cpp"
	"${CMAKE_SOURCE_DIR}/src/common/Dispatcher.h"
	"${CMAKE_SOURCE_DIR}/src/common/PayloadWriter.h"
	"${CMAKE_SOURCE_DIR}/src/common/PayloadReader.h"
	"${CMAKE_SOURCE_DIR}/src/common/ConnectionId.h"
	"${CMAKE_SOURCE_DIR}/src/common/Server.h"
	"${CMAKE_SOURCE_DIR}/src/common/Server.cpp"
	"${CMAKE_SOURCE_DIR}/src/common/HexDump.h"
	"${CMAKE_SOURCE_DIR}/src/common/PacketCapture.h"
	"${CMAKE_SOURCE_DIR}/src/common/PacketCapture.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/IDatabase.h"
	"${CMAKE_SOURCE_DIR}/src/storage/Database.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/SQLiteDatabase.h"
	"${CMAKE_SOURCE_DIR}/src/storage/SQLiteDatabase.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/MigrationRunner.h"
	"${CMAKE_SOURCE_DIR}/src/storage/MigrationRunner.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/Transaction.h"
	"${CMAKE_SOURCE_DIR}/src/storage/Transaction.cpp")

target_include_directories(login PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(login PRIVATE
	SHILTZ_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
	WIN32_LEAN_AND_MEAN
	_WIN32_WINNT=0x0601)

find_package(cryptopp CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(boost_asio CONFIG REQUIRED)
target_link_libraries(login PRIVATE cryptopp::cryptopp unofficial::sqlite3::sqlite3 Boost::asio ws2_32 mswsock)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET login PROPERTY CXX_STANDARD 20)
endif()
