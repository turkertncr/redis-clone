# redis-clone

A Redis server clone written in C, with its own RESP parser, poll-based TCP event loop, and an interactive REPL client. Implemented from scratch, including hash tables, sets, sorted sets (skip list backed), quick lists, and zip lists.

## Features

- RESP protocol parsing (`resp_parser.c`), including both multibulk and inline commands
- Poll-based TCP event loop (`server.c`) that pipelines multiple commands per read and sets `TCP_NODELAY`
- Built-in interactive client (`client.c`)
- Data types: strings, hashes, sets, sorted sets (skip list), lists (quick list / zip list)
- Lazy key expiration via `EXPIRE`/`TTL` (checked on access, no background sweep)
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
- **No active expiration.** Expired keys are only cleaned up lazily, on the next access to that key.
