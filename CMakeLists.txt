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
option(PERF "Compile patched \"perf\"" ON)
set(ADAPTYST_SCRIPT_PATH "/opt/adaptyst" CACHE STRING "Path where Adaptyst helper scripts should be installed into")
set(ADAPTYST_CONFIG_PATH "/etc/adaptyst.conf" CACHE STRING "Path where Adaptyst config file should be stored in")
set(PERF_TAG "dev-20250408" CACHE STRING "Patched \"perf\" git tag which should be used for setting up \"perf\"")

if(SERVER_ONLY)
  project(adaptyst-server
    DESCRIPTION "Processing server for Adaptyst"
    LANGUAGES CXX)
else()
  project(adaptyst
    DESCRIPTION "Performance analysis tool"
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

add_library(adaptystserv SHARED
  src/server/server.cpp
  src/server/client.cpp
  src/server/subclient.cpp
  src/server/socket.cpp
  src/archive.cpp
  version.cpp)

target_link_libraries(adaptystserv PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(adaptystserv PUBLIC Poco::Foundation Poco::Net)
target_link_libraries(adaptystserv PUBLIC LibArchive::LibArchive)

if(SERVER_ONLY)
  target_compile_definitions(adaptystserv PRIVATE SERVER_ONLY)
endif()

target_include_directories(adaptystserv PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(adaptyst-server
  src/main.cpp
  src/server/entrypoint.cpp)

target_compile_definitions(adaptyst-server PRIVATE SERVER_ONLY)
target_link_libraries(adaptyst-server PUBLIC Poco::Foundation Poco::Net CLI11::CLI11)
target_link_libraries(adaptyst-server PUBLIC adaptystserv)
target_include_directories(adaptyst-server PUBLIC
  ${CMAKE_SOURCE_DIR}/src/cmd)

install(TARGETS adaptystserv LIBRARY)
install(TARGETS adaptyst-server RUNTIME)

if(NOT SERVER_ONLY)
  # Patched "perf" setup
  if(PERF)
    if(PERF_DIR)
      install(CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/config_setup.sh ${ADAPTYST_CONFIG_PATH} ${PERF_DIR})")
    else()
      include(ExternalProject)

      if(PERF_REPOSITORY_DIR)
        ExternalProject_Add(perf
          SOURCE_DIR ${PERF_REPOSITORY_DIR}
          CONFIGURE_COMMAND ""
          BUILD_IN_SOURCE TRUE
          BUILD_COMMAND sh -c "make -C tools/perf install BUILD_BPF_SKEL=1 prefix=\"${CMAKE_BINARY_DIR}/perf-install\""
          INSTALL_COMMAND "")
      else()
        ExternalProject_Add(perf
          SOURCE_DIR linux
          GIT_REPOSITORY https://gitlab.cern.ch/adaptyst/linux
          GIT_TAG ${PERF_TAG}
          GIT_SHALLOW TRUE
          GIT_PROGRESS TRUE
          CONFIGURE_COMMAND ""
          BUILD_IN_SOURCE TRUE
          BUILD_COMMAND sh -c "make -C tools/perf install BUILD_BPF_SKEL=1 prefix=\"${CMAKE_BINARY_DIR}/perf-install\""
          INSTALL_COMMAND "")
      endif()

      install(DIRECTORY ${CMAKE_BINARY_DIR}/perf-install/ DESTINATION ${ADAPTYST_SCRIPT_PATH}/perf USE_SOURCE_PERMISSIONS)
      install(CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/config_setup.sh ${ADAPTYST_CONFIG_PATH} ${ADAPTYST_SCRIPT_PATH}/perf)")
    endif()
  endif()

  # Main Adaptyst setup
  add_executable(adaptyst
    src/main.cpp
    src/print.cpp
    src/profilers.cpp
    src/profiling.cpp
    src/requirements.cpp
    src/process.cpp
    src/entrypoint.cpp)

  target_compile_definitions(adaptyst PRIVATE ADAPTYST_SCRIPT_PATH="${ADAPTYST_SCRIPT_PATH}")
  target_compile_definitions(adaptyst PRIVATE ADAPTYST_CONFIG_FILE="${ADAPTYST_CONFIG_PATH}")

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    find_package(Boost REQUIRED program_options CONFIG
                 OPTIONAL_COMPONENTS process)
    message(STATUS "Found Boost: headers inside ${Boost_INCLUDE_DIRS}, version ${Boost_VERSION_STRING}")
  else()
    find_package(Boost REQUIRED program_options
                 OPTIONAL_COMPONENTS process)
  endif()

  target_link_libraries(adaptyst PUBLIC nlohmann_json::nlohmann_json)
  target_link_libraries(adaptyst PUBLIC Poco::Foundation Poco::Net)
  target_link_libraries(adaptyst PUBLIC CLI11::CLI11)
  target_link_libraries(adaptyst PUBLIC Boost::headers Boost::program_options $<TARGET_NAME_IF_EXISTS:Boost::process>)
  target_link_libraries(adaptyst PUBLIC LibArchive::LibArchive)

  target_include_directories(adaptyst PUBLIC
    ${CMAKE_SOURCE_DIR}/src/cmd)

  target_include_directories(adaptyst PUBLIC ${Boost_INCLUDE_DIRS})
  target_include_directories(adaptystserv PUBLIC ${Boost_INCLUDE_DIRS})

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(NUMA numa)
  if(NUMA_FOUND)
    message(STATUS "Found libnuma: ${NUMA_LINK_LIBRARIES}  ${NUMA_INCLUDE_DIRS}")
    target_compile_definitions(adaptyst PRIVATE LIBNUMA_AVAILABLE)
    target_include_directories(adaptyst PRIVATE ${NUMA_INCLUDE_DIRS})
    target_link_libraries(adaptyst PUBLIC ${NUMA_LINK_LIBRARIES})
  else()
    message(STATUS "numa not found, compiling without libnuma support")
  endif()

  target_link_libraries(adaptyst PUBLIC adaptystserv)

  install(TARGETS adaptyst RUNTIME)
  install(PROGRAMS src/utils/adaptyst-code.py TYPE BIN RENAME adaptyst-code)
  install(FILES src/scripts/adaptyst-syscall-process.py src/scripts/adaptyst-process.py
    DESTINATION ${ADAPTYST_SCRIPT_PATH})
else()
  find_package(Boost REQUIRED)

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    message(STATUS "Found Boost: headers inside ${Boost_INCLUDE_DIRS}, version ${Boost_VERSION_STRING}")
  endif()

  target_link_libraries(adaptystserv PUBLIC Boost::headers)
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
  target_link_libraries(auto-test-server PRIVATE adaptystserv)

  target_link_libraries(auto-test-client PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-client PRIVATE adaptystserv)

  target_link_libraries(auto-test-subclient PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-subclient PRIVATE adaptystserv)

  target_link_libraries(auto-test-socket PUBLIC GTest::gtest_main GTest::gmock_main Poco::Foundation Poco::Net)
  target_link_libraries(auto-test-socket PRIVATE adaptystserv)

  include(GoogleTest)
  gtest_discover_tests(auto-test-server)
  gtest_discover_tests(auto-test-client)
  gtest_discover_tests(auto-test-subclient)
  gtest_discover_tests(auto-test-socket)
endif()
