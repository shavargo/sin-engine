#!/bin/sh
set -e

SERVER_NAME="${SERVER_NAME:-SiN Multiplayer Dedicated Server}"
PORT="${PORT:-27910}"
MAP="${MAP:-biodm}"
MAX_CLIENTS="${MAX_CLIENTS:-8}"
DEATHMATCH="${DEATHMATCH:-1}"
SV_MASTER1="${SV_MASTER1:-master.shavargo.com:27900}"
BASEDIR="${BASEDIR:-/sin}"

# Ensure base directory exists
if [ ! -d "$BASEDIR/base" ]; then
    echo "[WARNING] $BASEDIR/base not found! Ensure your Steam pak0.sin / pak1.sin are mounted to $BASEDIR/base"
fi

exec /usr/local/bin/sin_ded \
  +set basedir "$BASEDIR" \
  +set dedicated 1 \
  +set sv_steamauth 0 \
  +set deathmatch "$DEATHMATCH" \
  +set maxclients "$MAX_CLIENTS" \
  +set hostname "$SERVER_NAME" \
  +set port "$PORT" \
  +set sv_master1 "$SV_MASTER1" \
  +set public 1 \
  +map "$MAP" \
  "$@"
