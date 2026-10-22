cmake_minimum_required(VERSION 3.16)
project(sin_engine C)

set(CMAKE_C_STANDARD 99)

option(SIN_DEDICATED "Build dedicated server only" ON)
option(SIN_MP "Enable SiN Multiplayer protocol/Steam deltas" ON)
option(SIN_BUILD_GAME "Build stock Q2 gamex86.dll" ON)
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

set(WIN32_SOURCES
  win32/sys_win.c
  win32/net_wins.c
  win32/conproc.c
  win32/q_shwin.c
)

set(DED_NULL_SOURCES
  null/cl_null.c
  null/cd_null.c
)

# --- gamex86 (stock Quake 2) ---
if(SIN_BUILD_GAME)
  file(GLOB GAME_SOURCES CONFIGURE_DEPENDS
    game/g_*.c
    game/m_*.c
    game/p_*.c
    game/q_shared.c
  )
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
endif()

# --- dedicated / client engine ---
if(SIN_DEDICATED OR NOT SIN_BUILD_CLIENT)
  add_executable(sin_ded WIN32
    ${QCOMMON_SOURCES}
    ${SERVER_SOURCES}
    ${SIN_SOURCES}
    ${WIN32_SOURCES}
    ${DED_NULL_SOURCES}
    win32/q2.rc
  )
  target_include_directories(sin_ded PRIVATE qcommon server game win32 sin null)
  target_link_libraries(sin_ded PRIVATE ws2_32 winmm)
  if(SIN_MP)
    target_compile_definitions(sin_ded PRIVATE SIN_MP)
  endif()
  if(MSVC)
    target_compile_options(sin_ded PRIVATE /W3)
  endif()
  set_target_properties(sin_ded PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif()

if(SIN_BUILD_CLIENT)
  file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS client/*.c)
  add_executable(sin_client WIN32
    ${QCOMMON_SOURCES}
    ${SERVER_SOURCES}
    ${SIN_SOURCES}
    ${WIN32_SOURCES}
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

if(SIN_BUILD_REF_GL)
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

message(STATUS "SIN_MP=${SIN_MP} SIN_DEDICATED=${SIN_DEDICATED} SIN_BUILD_GAME=${SIN_BUILD_GAME}")
