cmake_minimum_required (VERSION 3.5.1)
project(spawn)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.66 REQUIRED COMPONENTS system context coroutine)

add_library(spawn INTERFACE)
target_include_directories(spawn INTERFACE include)
target_compile_options(spawn INTERFACE "-DBOOST_COROUTINES_NO_DEPRECATION_WARNING")
target_link_libraries(spawn INTERFACE Boost::system Boost::context Boost::coroutine)
install(DIRECTORY include/spawn DESTINATION include)

option(SPAWN_TEST_ADDRESS_SANITIZER "build spawn tests with address sanitizer" ON)

option(SPAWN_BUILD_TESTS "build spawn tests" ON)
if(SPAWN_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()
