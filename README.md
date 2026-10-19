# redis-clone

A Redis server clone written in C, with its own RESP parser, poll-based TCP event loop, and an interactive REPL client. Implemented from scratch, including hash tables, sets, sorted sets (skip list backed), quick lists, and zip lists.

## Features

- RESP protocol parsing (`resp_parser.c`), including both multibulk and inline commands
- Poll-based TCP event loop (`server.c`) that pipelines multiple commands per read and sets `TCP_NODELAY`
- Built-in interactive client (`client.c`)
- Data types: strings, hashes, sets, sorted sets (skip list), lists (quick list / zip list)
- Key expiration via `EXPIRE`/`TTL`: lazy (checked on access) plus a sampled background sweep every 100 ms
- Password authentication via `requirepass` in `redis.conf`
- Append-only file logging: write commands are queued and flushed to disk by a background thread (see [Known limitations](#known-limitations))
- Centralized allocator wrapper (`alloc.c`), optionally backed by jemalloc

### Supported commands

| Category | Commands |
|---|---|
| Generic | `PING`, `ECHO`, `EXISTS`, `DEL`, `EXPIRE`, `TTL`, `AUTH` |
| Strings | `SET`, `MSET`, `GET`, `MGET`, `INCR`, `DECR`, `INCRBY`, `APPEND` |
| Hashes | `HSET`, `HGET`, `HGETALL`, `HDEL` |
| Sets | `SADD`, `SREM`, `SISMEMBER`, `SMEMBERS` |
| Sorted sets | `ZADD`, `ZREM`, `ZRANGEBYSCORE`, `ZSCORE`, `ZCOUNT` |
| Lists | `LPUSH`, `RPUSH`, `LPUSHX`, `RPUSHX`, `LPOP`, `RPOP`, `LINSERT`, `LLEN`, `LREM`, `LRANGE`, `LINDEX` |

## Building

Requires CMake (>= 3.16) and a C99 compiler. By default the build links against jemalloc via `pkg-config`, so either have jemalloc's development package installed, or disable it:

```sh
# with jemalloc (default)
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug

# without jemalloc
cmake -B cmake-build-debug -S . -DJEMALLOC=OFF
cmake --build cmake-build-debug
```

On Windows this links against `ws2_32`; on other platforms it uses standard POSIX sockets.

## Configuration

The server reads `redis.conf` (plain `key value` lines) from the current working directory on startup. Both keys are optional:

```
requirepass yourpassword
aofpath appendonly.aof
```

- `requirepass` — if set, clients must `AUTH <password>` before running any command other than `PING`/`AUTH`.
- `aofpath` — if set, write commands are appended to this file as they're executed. If omitted, AOF logging is disabled entirely.

If `redis.conf` is missing, startup fails, so run the binary from a directory containing it (or an empty `redis.conf` for a no-auth, no-AOF setup).

## Usage

### Start the server

Run the built binary with no arguments to start the server on port `6379`:

```sh
./redis_clone
```

### Use the built-in client

Run the binary with any argument to start the interactive client, which connects to `127.0.0.1:6379`:

```sh
./redis_clone client
```

Then type commands as you would with `redis-cli`:

```
SET foo bar
GET foo
HSET myhash field1 value1
SADD myset a b c
ZADD myzset 1 a
quit
```

Type `quit` to exit the client.

### Using redis-cli or another RESP client

Since the server speaks standard RESP (and inline commands), you can also connect with the official `redis-cli`:

```sh
redis-cli -p 6379
```

## Benchmarking

Uses the official `redis-benchmark` (on Windows, run it from WSL). The built-in `-t` test suite works out of the box now that inline commands are supported:

```sh
redis-benchmark -q -n 100000 -c 15 \
  -t ping_inline,ping_mbulk,set,get,incr,lpush,rpush,lpop,rpop,sadd,hset,zadd,mset,lrange_100,lrange_300,lrange_500,lrange_600
```

(`spop` and `zpopmin` aren't implemented, so they're left out of `-t`.) On a WSL2 loopback setup, most commands land within roughly 5-10% of real Redis's throughput under the same benchmark — that gap is mostly WSL2 scheduling noise rather than a structural difference. `LRANGE` is the exception: it's consistently slower across all range sizes because `lrange_command` currently traverses the list twice (once to count, once to collect) instead of once.

The server accepts up to `MAX_CONNECTIONS` concurrent connections (see `server.h`), so keep `-c` at or below that.

## Known limitations

- **AOF is write-only.** Write commands are logged to the AOF file as they happen, but nothing replays that file back on startup, so data doesn't currently survive a restart.
- **No RDB/snapshotting.** The only persistence is the AOF log described above.
- **Approximate active expiration.** The background sweep checks 20 random slots of the expires table every 100 ms. Unlike Redis it doesn't repeat when many sampled keys are expired, so expired keys that are never accessed can stay in memory for a while.

### Networking

- **No SIGPIPE handling (Linux/WSL).** If a client closes its connection while the server is sending to it, the kernel raises `SIGPIPE`, which by default kills the server. Fix: ignore `SIGPIPE` at startup or pass `MSG_NOSIGNAL` to `send`. Windows is not affected.
- **`POLLERR` / `POLLHUP` aren't handled.** The event loop only checks `POLLIN` and `POLLOUT`. A socket that reports only an error or hang-up is never dropped, so `poll` returns immediately on every pass and the server spins at 100% CPU.
- **Unbounded output buffers.** A client that stops reading without closing the connection (e.g. network loss with no FIN/RST) makes the server buffer every reply in memory with no limit. There's no output buffer limit, TCP keepalive, or idle timeout to detect and drop it.
- **No retry on failed sends (intentional).** A send that fails with anything other than "would block" drops the client and discards its unsent replies, as Redis does. Writes have already been applied and logged to the AOF, so the client must reconnect and check state itself.
- **Fixed connection limit.** At most `MAX_CONNECTIONS` (16) clients at once. Extra connections are accepted and then closed immediately.
