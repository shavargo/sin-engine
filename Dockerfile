# Multi-stage Dockerfile for native Linux sin_ded dedicated server
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Use local files if present in the build context, otherwise clone linux-dev branch
COPY . /src/
RUN if [ ! -f "/src/CMakeLists.txt" ]; then \
      git clone -b linux-dev https://github.com/shavargo/sin-engine.git /src; \
    fi

# Build standalone native Linux dedicated server
RUN cmake -B build \
    -DSIN_DEDICATED=ON \
    -DSIN_MP=ON \
    -DSIN_STATIC_GAME=ON \
    -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --config Release -j$(nproc)

# Final minimal runtime image (~60MB)
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && mkdir -p /sin/base

COPY --from=builder /src/build/sin_ded /usr/local/bin/sin_ded

# Embed self-contained entrypoint script
RUN cat << 'EOF' > /usr/local/bin/docker-entrypoint.sh
#!/bin/sh
set -e

SERVER_NAME="${SERVER_NAME:-SiN Multiplayer Dedicated Server}"
PORT="${PORT:-27910}"
MAP="${MAP:-biodm}"
MAX_CLIENTS="${MAX_CLIENTS:-8}"
DEATHMATCH="${DEATHMATCH:-1}"
SV_MASTER1="${SV_MASTER1:-master.shavargo.com:27900}"
BASEDIR="${BASEDIR:-/sin}"

# Fallback: if pak0.sin is in /sin directly instead of /sin/base
if [ ! -f "$BASEDIR/base/pak0.sin" ] && [ -f "$BASEDIR/pak0.sin" ]; then
    mkdir -p "$BASEDIR/base"
    ln -sf "$BASEDIR"/*.sin "$BASEDIR/base/" 2>/dev/null || true
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
EOF

RUN chmod +x /usr/local/bin/docker-entrypoint.sh /usr/local/bin/sin_ded

WORKDIR /sin
EXPOSE 27910/udp

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
