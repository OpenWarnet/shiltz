# CMakeList.txt : CMake project for shiltz, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (shiltz "shiltz.cpp" "shiltz.h" "DESCipher.cpp" "DESCipher.h")
add_executable (client "client.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET shiltz PROPERTY CXX_STANDARD 20)
  set_property(TARGET client PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
