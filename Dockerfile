FROM debian:bookworm-slim AS builder
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake pkg-config libjemalloc-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt main.c client_main.c redis.conf ./
COPY include/ include/
COPY src/ src/
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target redis_server redis_cli

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libjemalloc2 \
 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/redis_server /usr/local/bin/redis_server
COPY --from=builder /src/build/redis_cli /usr/local/bin/redis_cli
COPY redis.conf /etc/redis/redis.conf
# AOF file is written relative to here unless the config gives an absolute path
WORKDIR /data
EXPOSE 6379
ENTRYPOINT ["redis_server"]
CMD ["/etc/redis/redis.conf"]
