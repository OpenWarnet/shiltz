# CMakeList.txt : CMake project for the game server executable.
#

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
	"GameOpcodes.cpp"
	"handlers/Session.cpp"
	"handlers/Session.h"
	"handlers/Movement.cpp"
	"handlers/Movement.h"
	"GameServer.h"
	"GameServer.cpp"
	"GameSessionStore.h"
	"GameSessionStore.cpp"
	"protocol/client/GameEnter.h"
	"protocol/client/GameEnter.cpp"
	"protocol/client/CharMove.h"
	"protocol/client/CharMove.cpp"
	"protocol/server/CharacterDataLoad.h"
	"protocol/server/CharacterDataLoad.cpp"
	"protocol/server/InventoryItemList.h"
	"protocol/server/InventoryItemList.cpp"
	"protocol/server/ItemMapNew.h"
	"protocol/server/ItemMapNew.cpp"
	"protocol/server/CrtLoad.h"
	"protocol/server/CrtLoad.cpp"
	"protocol/server/CharExitSucc.h"
	"protocol/server/CharExitSucc.cpp"
	"protocol/server/EnterFail.h"
	"protocol/server/EnterFail.cpp"
	"${CMAKE_SOURCE_DIR}/src/common/Dispatcher.h"
	"${CMAKE_SOURCE_DIR}/src/common/PayloadWriter.h"
	"${CMAKE_SOURCE_DIR}/src/common/PayloadReader.h"
	"${CMAKE_SOURCE_DIR}/src/common/TCPServer.h"
	"${CMAKE_SOURCE_DIR}/src/common/TCPServer.cpp"
	"${CMAKE_SOURCE_DIR}/src/common/HexDump.h"
	"${CMAKE_SOURCE_DIR}/src/common/PacketCapture.h"
	"${CMAKE_SOURCE_DIR}/src/common/PacketCapture.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/IDatabase.h"
	"${CMAKE_SOURCE_DIR}/src/storage/Database.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/SQLiteDatabase.h"
	"${CMAKE_SOURCE_DIR}/src/storage/SQLiteDatabase.cpp"
	"${CMAKE_SOURCE_DIR}/src/storage/MigrationRunner.h"
	"${CMAKE_SOURCE_DIR}/src/storage/MigrationRunner.cpp")

target_include_directories(game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(game PRIVATE SHILTZ_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

find_package(cryptopp CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(game PRIVATE cryptopp::cryptopp unofficial::sqlite3::sqlite3)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET game PROPERTY CXX_STANDARD 20)
endif()
