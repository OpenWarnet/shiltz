# CMakeList.txt : CMake project for shiltz, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (
	shiltz 
	"shiltz.cpp" 
	"shiltz.h"
	"DESCipher.cpp"
	"DESCipher.h" 
	"LoginPacket.cpp" 
	"LoginPacket.h"
	"LoginHandler.cpp"
	"LoginHandler.h" 
	"common/PayloadWriter.h" 
	"data/Character.cpp" 
	"data/Character.h"
	"data/CharacterSelection.h"
	"data/CharacterSelection.cpp" 
	"data/ServerList.h"
	"data/ServerList.cpp" "data/ServerSelect.h" "data/ServerSelect.cpp" "common/PayloadReader.h" "data/Login.h" "data/Login.cpp")

add_executable (
	client
	"client.cpp")

target_include_directories(shiltz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET shiltz PROPERTY CXX_STANDARD 20)
  set_property(TARGET client PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
