cmake_minimum_required(VERSION 3.28)
project(tf2aeth LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/CPM.cmake")
    file(DOWNLOAD 
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
        "${CMAKE_CURRENT_LIST_DIR}/cmake/CPM.cmake"
    )
endif()
include(cmake/CPM.cmake)
CPMAddPackage(
    NAME lua
    GITHUB_REPOSITORY lua/lua
    GIT_TAG v5.4.6
    DOWNLOAD_ONLY YES
)
if(lua_ADDED)
    set(LUA_INTERNAL_SRC "${lua_SOURCE_DIR}")
    add_library(lua STATIC
        "${LUA_INTERNAL_SRC}/lapi.c"
        "${LUA_INTERNAL_SRC}/lauxlib.c"
        "${LUA_INTERNAL_SRC}/lbaselib.c"
        "${LUA_INTERNAL_SRC}/lcode.c"
        "${LUA_INTERNAL_SRC}/lcorolib.c"
        "${LUA_INTERNAL_SRC}/lctype.c"
        "${LUA_INTERNAL_SRC}/ldblib.c"
        "${LUA_INTERNAL_SRC}/ldebug.c"
        "${LUA_INTERNAL_SRC}/ldo.c"
        "${LUA_INTERNAL_SRC}/ldump.c"
        "${LUA_INTERNAL_SRC}/lfunc.c"
        "${LUA_INTERNAL_SRC}/lgc.c"
        "${LUA_INTERNAL_SRC}/linit.c"
        "${LUA_INTERNAL_SRC}/liolib.c"
        "${LUA_INTERNAL_SRC}/llex.c"
        "${LUA_INTERNAL_SRC}/lmathlib.c"
        "${LUA_INTERNAL_SRC}/lmem.c"
        "${LUA_INTERNAL_SRC}/loadlib.c"
        "${LUA_INTERNAL_SRC}/lobject.c"
        "${LUA_INTERNAL_SRC}/lopcodes.c"
        "${LUA_INTERNAL_SRC}/loslib.c"
        "${LUA_INTERNAL_SRC}/lparser.c"
        "${LUA_INTERNAL_SRC}/lstate.c"
        "${LUA_INTERNAL_SRC}/lstring.c"
        "${LUA_INTERNAL_SRC}/lstrlib.c"
        "${LUA_INTERNAL_SRC}/ltable.c"
        "${LUA_INTERNAL_SRC}/ltablib.c"
        "${LUA_INTERNAL_SRC}/ltm.c"
        "${LUA_INTERNAL_SRC}/lundump.c"
        "${LUA_INTERNAL_SRC}/lutf8lib.c"
        "${LUA_INTERNAL_SRC}/lvm.c"
        "${LUA_INTERNAL_SRC}/lzio.c"
    )

    target_include_directories(lua PUBLIC "${LUA_INTERNAL_SRC}")
endif()
CPMAddPackage(
    NAME Zydis
    GITHUB_REPOSITORY zyantific/zydis
    GIT_TAG v4.1.1 # Stick to 4.1.1 as 4.3.0 tag doesn't exist yet
)
file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.cc)
add_library(tf2aeth SHARED ${SOURCE_FILES})
target_link_libraries(tf2aeth PRIVATE lua Zydis ffi dl m)