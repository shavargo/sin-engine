cmake_minimum_required(VERSION 3.16)
project(sin_engine C)

set(CMAKE_C_STANDARD 99)

option(SIN_DEDICATED "Build dedicated server only" ON)
option(SIN_MP "Enable SiN Multiplayer protocol/Steam deltas" ON)
option(SIN_BUILD_GAME "Build game library" ON)
option(SIN_STATIC_GAME "Statically link game logic into sin_ded (no .dll/.so required)" OFF)
option(SIN_BUILD_REF_GL "Build ref_gl.dll (Phase 2)" OFF)
option(SIN_BUILD_CLIENT "Build full client exe (Phase 2)" OFF)

if(SIN_DEDICATED)
  set(SIN_BUILD_CLIENT OFF)
endif()

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
# SIN_MP is per-target (not on stock gamex86) so Q2 game keeps Q2 entity_state
if(SIN_DEDICATED)
  add_compile_definitions(DEDICATED_ONLY)
endif()

set(QCOMMON_SOURCES
  qcommon/cmd.c
  qcommon/cmodel.c
  qcommon/common.c
  qcommon/crc.c
  qcommon/cvar.c
  qcommon/files.c
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
    linux/glob.c
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
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/baseq2"
    )
  endif()
endif()

# --- dedicated engine ---
if(SIN_DEDICATED OR NOT SIN_BUILD_CLIENT)
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

if(WIN32 AND SIN_BUILD_CLIENT)
  file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS client/*.c)
  add_executable(sin_client WIN32
    ${QCOMMON_SOURCES}
    ${SERVER_SOURCES}
    ${SIN_SOURCES}
    ${PLATFORM_SOURCES}
    ${CLIENT_SOURCES}
    game/m_flash.c
    win32/cd_win.c
    win32/snd_win.c
    win32/in_win.c
    win32/vid_dll.c
    win32/vid_menu.c
    win32/q2.rc
  )
  target_include_directories(sin_client PRIVATE qcommon server game client win32 sin)
  target_link_libraries(sin_client PRIVATE ws2_32 winmm)
  target_compile_definitions(sin_client PRIVATE SIN_MP)
  if(MSVC)
    target_link_options(sin_client PRIVATE "/STACK:8388608")
  endif()
endif()

if(WIN32 AND SIN_BUILD_REF_GL)
  file(GLOB REF_GL_SOURCES CONFIGURE_DEPENDS ref_gl/*.c)
  add_library(ref_gl SHARED ${REF_GL_SOURCES} game/q_shared.c win32/q_shwin.c win32/qgl_win.c win32/glw_imp.c)
  target_include_directories(ref_gl PRIVATE ref_gl qcommon game win32 sin)
  target_link_libraries(ref_gl PRIVATE opengl32 winmm)
  target_compile_definitions(ref_gl PRIVATE REF_GL SIN_MP)
  target_sources(ref_gl PRIVATE sin/sinmdl.c)
  set_target_properties(ref_gl PROPERTIES
    OUTPUT_NAME "ref_gl"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  # Export GetRefAPI (required — otherwise /OPT:REF strips the renderer)
  target_link_options(ref_gl PRIVATE "/DEF:${CMAKE_SOURCE_DIR}/ref_gl/ref_gl.def")
endif()

message(STATUS "SIN_MP=${SIN_MP} SIN_DEDICATED=${SIN_DEDICATED} SIN_BUILD_GAME=${SIN_BUILD_GAME} SIN_STATIC_GAME=${SIN_STATIC_GAME}")
