cmake_minimum_required(VERSION 3.20)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0012 NEW)
  cmake_policy(SET CMP0115 NEW)

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
  endif()

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
  endif()
endif()

option(SERVER_ONLY "Compile only adaptyst-server (OS-portable)" OFF)
option(ENABLE_TESTS "Enable Adaptyst automated tests" OFF)
set(ADAPTYST_SCRIPT_PATH "/opt/adaptyst" CACHE STRING "Path where Adaptyst helper scripts should be installed into")
set(ADAPTYST_CONFIG_PATH "/etc/adaptyst.conf" CACHE STRING "Path where Adaptyst config file should be stored in")

if(SERVER_ONLY)
  project(adaptyst-server
    DESCRIPTION "Processing server for Adaptyst"
    LANGUAGES CXX)
else()
  project(adaptyst
    DESCRIPTION "Comprehensive profiling tool based on Linux perf"
    LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(nlohmann_json REQUIRED)
find_package(Poco REQUIRED Net Foundation)
find_package(CLI11 CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(src/version.cpp.in version.cpp @ONLY)

add_library(version.o OBJECT version.cpp)
target_include_directories(version.o PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_library(archive.o OBJECT src/archive.cpp)

add_library(client.o OBJECT src/server/client.cpp)
target_include_directories(client.o PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_library(server.o OBJECT src/server/server.cpp)
add_library(subclient.o OBJECT src/server/subclient.cpp)

add_library(socket.o OBJECT src/server/socket.cpp)
if(SERVER_ONLY)
  target_compile_definitions(socket.o PRIVATE SERVER_ONLY)
endif()

add_library(server_entrypoint.o OBJECT src/server/entrypoint.cpp)
target_include_directories(server_entrypoint.o PRIVATE ${CMAKE_SOURCE_DIR}/src/cmd)

add_library(adaptystserv SHARED)
target_link_libraries(adaptystserv PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(adaptystserv PUBLIC Poco::Foundation Poco::Net)
target_link_libraries(adaptystserv PUBLIC LibArchive::LibArchive)
target_link_libraries(adaptystserv PRIVATE server.o client.o subclient.o socket.o archive.o)

add_executable(adaptyst-server
  src/main.cpp)

target_compile_definitions(adaptyst-server PRIVATE SERVER_ONLY)
target_link_libraries(adaptyst-server PUBLIC Poco::Foundation Poco::Net)
target_link_libraries(adaptyst-server PUBLIC CLI11::CLI11)
target_link_libraries(adaptyst-server PRIVATE adaptystserv server_entrypoint.o version.o)

if(NOT SERVER_ONLY)
  add_executable(adaptyst
    src/main.cpp)

  add_library(main_entrypoint.o OBJECT src/entrypoint.cpp)
  target_include_directories(main_entrypoint.o PRIVATE ${CMAKE_SOURCE_DIR}/src/cmd)

  add_library(print.o OBJECT src/print.cpp)
  add_library(profilers.o OBJECT src/profilers.cpp)
  add_library(profiling.o OBJECT src/profiling.cpp)
  add_library(requirements.o OBJECT src/requirements.cpp)
  add_library(process.o OBJECT src/process.cpp)

  target_compile_definitions(profilers.o PRIVATE ADAPTYST_SCRIPT_PATH="${ADAPTYST_SCRIPT_PATH}")
  target_compile_definitions(main_entrypoint.o PRIVATE ADAPTYST_CONFIG_FILE="${ADAPTYST_CONFIG_PATH}")

  find_package(Boost REQUIRED program_options)

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    message(STATUS "Found Boost: headers inside ${Boost_INCLUDE_DIRS}, version ${Boost_VERSION_STRING}")
  endif()

  target_link_libraries(adaptyst PUBLIC nlohmann_json::nlohmann_json)
  target_link_libraries(adaptyst PUBLIC Poco::Foundation Poco::Net)
  target_link_libraries(adaptyst PUBLIC CLI11::CLI11)
  target_link_libraries(adaptyst PUBLIC Boost::program_options)
  target_link_libraries(adaptyst PUBLIC LibArchive::LibArchive)

  target_include_directories(adaptyst PUBLIC ${Boost_INCLUDE_DIRS})

  find_library(LIBNUMA
    NAMES numa
    DOC "libnuma")

  if(LIBNUMA)
    message(STATUS "Found libnuma: ${LIBNUMA}")

    find_path(LIBNUMA_INCLUDE
      NAMES numa.h
      PATH_SUFFIXES include
      DOC "libnuma header directory"
    )

    if (LIBNUMA_INCLUDE)
      message(STATUS "Found numa.h inside ${LIBNUMA_INCLUDE}")
      set(LIBNUMA_AVAILABLE TRUE)
    else()
      message(STATUS "numa.h not found, compiling without libnuma support")
      set(LIBNUMA_AVAILABLE FALSE)
    endif()
  else()
    message(STATUS "libnuma not found, compiling without libnuma support")
    set(LIBNUMA_AVAILABLE FALSE)
  endif()

  if(LIBNUMA_AVAILABLE)
    target_compile_definitions(requirements.o PRIVATE LIBNUMA_AVAILABLE)
    target_include_directories(requirements.o PRIVATE ${LIBNUMA_INCLUDE})
    target_link_libraries(adaptyst PUBLIC numa)
  endif()

  target_link_libraries(adaptyst PRIVATE adaptystserv)
  target_link_libraries(adaptyst PRIVATE
    profiling.o requirements.o profilers.o print.o archive.o main_entrypoint.o process.o version.o)
else()
  find_package(Boost REQUIRED)

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    message(STATUS "Found Boost: headers inside ${Boost_INCLUDE_DIRS}, version ${Boost_VERSION_STRING}")
  endif()

  target_include_directories(adaptystserv PUBLIC ${Boost_INCLUDE_DIRS})
endif()

if (ENABLE_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/2954cb8d879886403d55343f941ae7d0216e0f6b.zip)

  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(auto-test-server
    test/server/test_server.cpp)
  add_executable(auto-test-client
    test/server/test_client.cpp)
  add_executable(auto-test-subclient
    test/server/test_subclient.cpp)
  add_executable(auto-test-socket
    test/server/test_socket.cpp)

  target_include_directories(auto-test-server PRIVATE ${CMAKE_SOURCE_DIR}/src/server)
  target_include_directories(auto-test-client PRIVATE ${CMAKE_SOURCE_DIR}/src/server)
  target_include_directories(auto-test-subclient PRIVATE ${CMAKE_SOURCE_DIR}/src/server)
  target_include_directories(auto-test-socket PRIVATE ${CMAKE_SOURCE_DIR}/src/server)

  target_link_libraries(auto-test-server PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-server PRIVATE server.o client.o)

  target_link_libraries(auto-test-client PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-client PRIVATE client.o)

  target_link_libraries(auto-test-subclient PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-subclient PRIVATE subclient.o)

  target_link_libraries(auto-test-socket PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-socket PRIVATE socket.o)

  include(GoogleTest)
  gtest_discover_tests(auto-test-server)
  gtest_discover_tests(auto-test-client)
  gtest_discover_tests(auto-test-subclient)
  gtest_discover_tests(auto-test-socket)
endif()
