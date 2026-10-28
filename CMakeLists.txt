cmake_minimum_required(VERSION 3.16)
project(sin_engine C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SIN_BUILD_DEDICATED "Build dedicated server executable (sin_ded)" ON)
option(SIN_BUILD_CLIENT "Build full client executable (sin_client)" ON)
option(SIN_MP "Enable SiN Multiplayer protocol/Steam deltas" ON)
option(SIN_BUILD_SIN_GAME "Build authentic SiN game library (gamex86) from source" OFF)
option(SIN_BUILD_WOS_GAME "Build Wages of SiN game library (gamex86_wos) from source" OFF)
option(SIN_BUILD_GAME "Build stock Quake 2 game library" ON)
option(SIN_STATIC_GAME "Statically link game logic into sin_ded (no .dll/.so required)" OFF)
option(SIN_BUILD_REF_GL "Build ref_gl.dll" ON)
option(USE_SDL2 "Build client with modern SDL2 platform layer" ON)

if(USE_SDL2)
  find_package(SDL2 QUIET)
  if(NOT SDL2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      SDL2
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-2.30.8
      GIT_SHALLOW TRUE
    )
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
  endif()
endif()

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
if(NOT WIN32)
  add_compile_definitions(
    stricmp=strcasecmp
    strnicmp=strncasecmp
    _strdup=strdup
    _stricmp=strcasecmp
    _strnicmp=strncasecmp
    _vsnprintf=vsnprintf
  )
  add_compile_options(
    -Wno-incompatible-pointer-types
    -Wno-implicit-function-declaration
    -Wno-discarded-qualifiers
  )
endif()

set(QCOMMON_SOURCES
  qcommon/cmd.c
  qcommon/cmodel.c
  qcommon/common.c
  qcommon/crc.c
  qcommon/cvar.c
  qcommon/files.c
  qcommon/glob.c
  qcommon/md4.c
  qcommon/net_chan.c
  qcommon/pmove.c
  game/q_shared.c
)

set(SERVER_SOURCES
  server/sv_ccmds.c
  server/sv_ents.c
  server/sv_game.c
  server/sv_init.c
  server/sv_main.c
  server/sv_send.c
  server/sv_user.c
  server/sv_world.c
)

set(SIN_SOURCES
  sin/steam_server.c
  sin/sin_client.c
  sin/sinmdl.c
  sin/sin_sv_game.c
)

if(WIN32)
  set(PLATFORM_SOURCES
    win32/sys_win.c
    win32/net_wins.c
    win32/conproc.c
    win32/q_shwin.c
  )
  set(PLATFORM_INCLUDES win32)
  set(PLATFORM_LIBS ws2_32 winmm)
  set(DED_RC win32/q2.rc)
  set(DED_SUBSYSTEM WIN32)
else()
  set(PLATFORM_SOURCES
    linux/sys_linux.c
    linux/net_udp.c
    linux/q_shlinux.c
  )
  set(PLATFORM_INCLUDES linux)
  set(PLATFORM_LIBS m ${CMAKE_DL_LIBS})
  set(DED_RC)
  set(DED_SUBSYSTEM)
endif()

set(DED_NULL_SOURCES
  null/cl_null.c
  null/cd_null.c
)

# --- game library (stock Quake 2 / SiN game logic) ---
if(SIN_BUILD_GAME AND NOT SIN_STATIC_GAME)
  file(GLOB GAME_SOURCES CONFIGURE_DEPENDS
    game/g_*.c
    game/m_*.c
    game/p_*.c
    game/q_shared.c
  )
  if(WIN32)
    add_library(gamex86 SHARED ${GAME_SOURCES} win32/q_shwin.c)
    target_include_directories(gamex86 PRIVATE game qcommon)
    target_compile_definitions(gamex86 PRIVATE GAME_DLL)
    target_link_libraries(gamex86 PRIVATE winmm)
    target_link_options(gamex86 PRIVATE "/DEF:${CMAKE_SOURCE_DIR}/game/game.def")
    set_target_properties(gamex86 PROPERTIES
      OUTPUT_NAME "gamex86"
      PREFIX ""
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/baseq2"
    )
  else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(GAME_LIB_NAME "gamex86_64")
    else()
      set(GAME_LIB_NAME "gamei386")
    endif()
    add_library(game SHARED ${GAME_SOURCES} linux/q_shlinux.c linux/glob.c)
    target_include_directories(game PRIVATE game qcommon linux)
    target_compile_definitions(game PRIVATE GAME_DLL)
    target_link_libraries(game PRIVATE m)
    set_target_properties(game PROPERTIES
      OUTPUT_NAME "${GAME_LIB_NAME}"
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/base"
    )
  endif()
endif()

# --- dedicated engine ---
if(SIN_BUILD_DEDICATED)
  set(DED_EXTRA_SOURCES)
  set(DED_EXTRA_DEFS)
  if(SIN_STATIC_GAME)
    # Compiled into sin_ded without SIN_MP (set_source_files_properties below).
    file(GLOB STATIC_GAME_SOURCES CONFIGURE_DEPENDS
      game/g_*.c
      game/m_*.c
      game/p_*.c
    )
    set(DED_EXTRA_SOURCES ${STATIC_GAME_SOURCES} server/sv_static_q2game.c)
    set(DED_EXTRA_DEFS STATIC_GAME GAME_DLL GAME_HARD_LINKED)
  endif()

  add_executable(sin_ded ${DED_SUBSYSTEM}
    ${QCOMMON_SOURCES}
    ${SERVER_SOURCES}
    ${SIN_SOURCES}
    ${PLATFORM_SOURCES}
    ${DED_NULL_SOURCES}
    ${DED_EXTRA_SOURCES}
    ${DED_RC}
  )
  target_include_directories(sin_ded PRIVATE qcommon server game ${PLATFORM_INCLUDES} sin null)
  target_link_libraries(sin_ded PRIVATE ${PLATFORM_LIBS})
  target_compile_definitions(sin_ded PRIVATE DEDICATED_ONLY)
  if(SIN_MP)
    target_compile_definitions(sin_ded PRIVATE SIN_MP)
  endif()
  if(DED_EXTRA_DEFS)
    target_compile_definitions(sin_ded PRIVATE ${DED_EXTRA_DEFS})
  endif()
  # Game TUs must not see SIN_MP (Q2 CONTENTS_SLIME / entity_state). Strip it per-file.
  if(SIN_STATIC_GAME AND SIN_MP)
    set_property(SOURCE ${STATIC_GAME_SOURCES} APPEND PROPERTY COMPILE_OPTIONS "-USIN_MP")
  endif()
  if(NOT WIN32)
    target_compile_definitions(sin_ded PRIVATE stricmp=strcasecmp)
  endif()
  if(MSVC)
    target_compile_options(sin_ded PRIVATE /W3)
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sin_ded PRIVATE -Wall -Wno-unused-result -Wno-pointer-sign -fno-strict-aliasing)
  endif()
  set_target_properties(sin_ded PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif()

if(SIN_BUILD_CLIENT)
  file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS client/*.c)
  if(USE_SDL2)
    set(CLIENT_PLATFORM_SOURCES
      sdl/in_sdl.c
      sdl/snd_sdl.c
      sdl/cd_sdl.c
      sdl/vid_sdl.c
    )
    set(CLIENT_PLATFORM_DEFS USE_SDL2 SDL_MAIN_HANDLED)
    set(CLIENT_PLATFORM_INCLUDES sdl)
    set(CLIENT_PLATFORM_LIBS SDL2::SDL2)
  else()
    set(CLIENT_PLATFORM_SOURCES
      win32/cd_win.c
      win32/snd_win.c
      win32/in_win.c
      win32/vid_dll.c
    )
    set(CLIENT_PLATFORM_DEFS)
    set(CLIENT_PLATFORM_INCLUDES)
    set(CLIENT_PLATFORM_LIBS)
  endif()

  add_executable(sin_client WIN32
    ${QCOMMON_SOURCES}
    ${SERVER_SOURCES}
    ${SIN_SOURCES}
    ${PLATFORM_SOURCES}
    ${CLIENT_SOURCES}
    game/m_flash.c
    ${CLIENT_PLATFORM_SOURCES}
    ${DED_RC}
  )
  target_include_directories(sin_client PRIVATE qcommon server game client ${PLATFORM_INCLUDES} ${CLIENT_PLATFORM_INCLUDES} sin)
  target_link_libraries(sin_client PRIVATE ${PLATFORM_LIBS} ${CLIENT_PLATFORM_LIBS})
  target_compile_definitions(sin_client PRIVATE SIN_MP ${CLIENT_PLATFORM_DEFS})
  if(MSVC)
    target_link_options(sin_client PRIVATE "/STACK:8388608")
  endif()

  if(USE_SDL2 AND TARGET SDL2)
    add_custom_command(TARGET sin_client POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL2>" "$<TARGET_FILE_DIR:sin_client>/$<TARGET_FILE_NAME:SDL2>"
      COMMENT "Deploying SDL2.dll to client output directory"
    )
  endif()
endif()

if(SIN_BUILD_REF_GL)
  file(GLOB REF_GL_SOURCES CONFIGURE_DEPENDS ref_gl/*.c)
  if(USE_SDL2)
    set(REF_GL_PLATFORM_SOURCES
      sdl/glimp_sdl.c
      sdl/qgl_sdl.c
    )
    set(REF_GL_PLATFORM_DEFS USE_SDL2)
    set(REF_GL_PLATFORM_INCLUDES sdl)
    set(REF_GL_PLATFORM_LIBS SDL2::SDL2)
  else()
    set(REF_GL_PLATFORM_SOURCES
      win32/qgl_win.c
      win32/glw_imp.c
    )
    set(REF_GL_PLATFORM_DEFS)
    set(REF_GL_PLATFORM_INCLUDES win32)
    set(REF_GL_PLATFORM_LIBS)
  endif()

  if(WIN32)
    set(REF_GL_SYSTEM_SOURCES win32/q_shwin.c)
    set(REF_GL_SYSTEM_LIBS opengl32 winmm)
  else()
    set(REF_GL_SYSTEM_SOURCES linux/q_shlinux.c qcommon/glob.c)
    set(REF_GL_SYSTEM_LIBS GL m)
  endif()

  add_library(ref_gl SHARED
    ${REF_GL_SOURCES}
    game/q_shared.c
    ${REF_GL_SYSTEM_SOURCES}
    ${REF_GL_PLATFORM_SOURCES}
    sin/sinmdl.c
  )
  target_include_directories(ref_gl PRIVATE ref_gl qcommon game ${PLATFORM_INCLUDES} ${REF_GL_PLATFORM_INCLUDES} sin)
  target_link_libraries(ref_gl PRIVATE ${REF_GL_SYSTEM_LIBS} ${REF_GL_PLATFORM_LIBS})
  target_compile_definitions(ref_gl PRIVATE REF_GL SIN_MP ${REF_GL_PLATFORM_DEFS})
  set_target_properties(ref_gl PROPERTIES
    OUTPUT_NAME "ref_gl"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  if(WIN32)
    target_link_options(ref_gl PRIVATE "/DEF:${CMAKE_SOURCE_DIR}/ref_gl/ref_gl.def")
  endif()
endif()

# --- SiN Game Libraries (Authentic SiN Base and Wages of SiN) ---
if(SIN_BUILD_SIN_GAME OR SIN_BUILD_WOS_GAME)
  if(EXISTS "${CMAKE_SOURCE_DIR}/game_sin/src/code")
    set(SIN_GAME_DIR "${CMAKE_SOURCE_DIR}/game_sin/src/code")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/game_sin/game")
    set(SIN_GAME_DIR "${CMAKE_SOURCE_DIR}/game_sin")
  else()
    set(SIN_GAME_DIR "")
    message(WARNING "game_sin submodule not initialized. Run 'git submodule update --init' to build native SiN game modules.")
  endif()

  if(SIN_GAME_DIR)
    set(ELIB_SOURCES
      ${SIN_GAME_DIR}/elib/emisc.cpp
      ${SIN_GAME_DIR}/elib/qstring.cpp
      ${SIN_GAME_DIR}/elib/zone.cpp
    )
    if(WIN32)
      set(SIN_GAME_NAME "gamex86")
    elseif(APPLE)
      set(SIN_GAME_NAME "game")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(SIN_GAME_NAME "gamex86_64")
    else()
      set(SIN_GAME_NAME "gamei386")
    endif()

    if(SIN_BUILD_SIN_GAME)
      file(GLOB SIN_GAME_SOURCES CONFIGURE_DEPENDS
        ${SIN_GAME_DIR}/game/*.cpp
      )
      list(REMOVE_ITEM SIN_GAME_SOURCES "${SIN_GAME_DIR}/game/arcade_comm.cpp")
      list(APPEND SIN_GAME_SOURCES
        ${ELIB_SOURCES}
        ${SIN_GAME_DIR}/game/q_shared.c
      )

      add_library(sin_game SHARED ${SIN_GAME_SOURCES})
      target_include_directories(sin_game PRIVATE
        ${SIN_GAME_DIR}/game
        ${SIN_GAME_DIR}/elib
        ${SIN_GAME_DIR}
      )
      target_compile_definitions(sin_game PRIVATE
        GAMEX86_EXPORTS
        BUILDING_REF_GL
        SIN
        ELIB_GAMEDLL
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
      )
      if(MSVC)
        target_compile_options(sin_game PRIVATE "/MP")
      endif()
      if(WIN32)
        target_link_libraries(sin_game PRIVATE winmm)
        target_link_options(sin_game PRIVATE "/DEF:${SIN_GAME_DIR}/game/game.def")
      else()
        target_link_libraries(sin_game PRIVATE m)
      endif()
      set_target_properties(sin_game PROPERTIES
        OUTPUT_NAME "${SIN_GAME_NAME}"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sin_base"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sin_base"
      )
      if(TARGET sin_client)
        add_custom_command(TARGET sin_game POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sin_client>/sinroot/base"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:sin_game>" "$<TARGET_FILE_DIR:sin_client>/sinroot/base/$<TARGET_FILE_NAME:sin_game>"
          COMMENT "Deploying sin_game to sinroot/base"
        )
      endif()
    endif()

    if(SIN_BUILD_WOS_GAME)
      file(GLOB WOS_GAME_SOURCES CONFIGURE_DEPENDS
        ${SIN_GAME_DIR}/game2015/*.cpp
      )
      list(APPEND WOS_GAME_SOURCES
        ${ELIB_SOURCES}
        ${SIN_GAME_DIR}/game2015/q_shared.c
      )

      add_library(sin_game_wos SHARED ${WOS_GAME_SOURCES})
      target_include_directories(sin_game_wos PRIVATE
        ${SIN_GAME_DIR}/game2015
        ${SIN_GAME_DIR}/elib
        ${SIN_GAME_DIR}
      )
      target_compile_definitions(sin_game_wos PRIVATE
        GAMEX86_WOS_EXPORTS
        GAMEX86_EXPORTS
        BUILDING_REF_GL
        SIN
        ELIB_GAMEDLL
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
      )
      if(MSVC)
        target_compile_options(sin_game_wos PRIVATE "/MP")
      endif()
      if(WIN32)
        target_link_libraries(sin_game_wos PRIVATE winmm)
        target_link_options(sin_game_wos PRIVATE "/DEF:${SIN_GAME_DIR}/game2015/game.def")
      else()
        target_link_libraries(sin_game_wos PRIVATE m)
      endif()
      set_target_properties(sin_game_wos PROPERTIES
        OUTPUT_NAME "${SIN_GAME_NAME}"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sin_2015"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sin_2015"
      )
      if(TARGET sin_client)
        add_custom_command(TARGET sin_game_wos POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sin_client>/sinroot/2015"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:sin_game_wos>" "$<TARGET_FILE_DIR:sin_client>/sinroot/2015/$<TARGET_FILE_NAME:sin_game_wos>"
          COMMENT "Deploying sin_game_wos to sinroot/2015"
        )
      endif()
    endif()
  endif()
endif()

message(STATUS "SIN_MP=${SIN_MP} SIN_DEDICATED=${SIN_DEDICATED} SIN_BUILD_GAME=${SIN_BUILD_GAME} SIN_STATIC_GAME=${SIN_STATIC_GAME}")
