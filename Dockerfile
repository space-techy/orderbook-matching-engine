# Sample contestant submission Dockerfile for the reference matching engine.
# This is the contract every contestant image must satisfy:
#   * build their engine from source
#   * the final image runs the engine, which serves GET /health and WS /ws
#   * the WebSocket port is taken from argv[1] (the orchestrator passes "3001")
#   * it must run fine as a non-root user with a read-only root filesystem
#     (the orchestrator mounts a writable /tmp) — so don't write outside /tmp.
#
# Build context = this directory (contains CMakeLists.txt, src/, include/).

# ── build stage ───────────────────────────────────────────────────────────────
FROM debian:bookworm AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ cmake make git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include/ include/
COPY src/ src/

# FetchContent pulls asio / Crow / Glaze — the build stage has internet (the
# orchestrator's build phase is the only place contestant code reaches the net).
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

# ── runtime stage ─────────────────────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/matching_server /usr/local/bin/matching_server

EXPOSE 3001
# argv[1] = port. The orchestrator passes the WS port ("3001") as the only arg.
ENTRYPOINT ["matching_server"]
CMD ["3001"]
