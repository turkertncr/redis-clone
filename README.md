# redis-clone

A small Redis server clone written in C, with its own RESP parser, TCP server, and an interactive REPL client. Implemented from scratch, including hash tables, sets, sorted sets (skip list backed), quick lists, and zip lists.

## Features

- RESP protocol parsing (`resp_parser.c`)
- TCP server with a simple event loop (`server.c`)
- Built-in interactive client (`client.c`)
- Data types: strings, hashes, sets, sorted sets, lists (quick list / zip list)

### Supported commands

| Category | Commands |
|---|---|
| Generic | `PING`, `ECHO`, `EXISTS`, `DEL`, `EXPIRE`, `TTL` |
| Strings | `SET`, `MSET`, `GET`, `MGET`, `INCR`, `DECR`, `INCRBY`, `APPEND` |
| Hashes | `HSET`, `HGET`, `HGETALL`, `HDEL` |
| Sets | `SADD`, `SREM`, `SISMEMBER`, `SMEMBERS` |
| Sorted sets | `ZADD`, `ZREM`, `ZRANGEBYSCORE`, `ZSCORE` |
| Lists | `LPUSH`, `RPUSH`, `LPUSHX`, `RPUSHX`, `LPOP`, `RPOP`, `LINSERT`, `LLEN`, `LREM`, `LRANGE`, `LINDEX` |

## Building

Requires CMake (>= 3.16) and a C99 compiler.

```sh
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug
```

On Windows this links against `ws2_32`; on other platforms it uses standard POSIX sockets.

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

Since the server speaks standard RESP, you can also connect with the official `redis-cli`:

```sh
redis-cli -p 6379
```


## Benchmarking

Uses the official `redis-benchmark` (on Windows, run it from WSL). Run the server with the empty `redis.conf` (no password), because `redis-benchmark` sends `AUTH` and its first command in one write, and the server reads one command per request.

```sh
# 8 connections; the built-in `-t` tests send inline commands, so pass the commands explicitly
for cmd in "PING" "SET k:__rand_int__ value" "GET k:__rand_int__" "INCR c:__rand_int__" \
           "HSET h:__rand_int__ f v" "SADD s:__rand_int__ m" "ZADD z:__rand_int__ 5 m" \
           "LPUSH l:__rand_int__ v" "LRANGE l:__rand_int__ 0 9"; do
  redis-benchmark -p 6379 -c 8 -n 100000 -r 1000 -q $cmd
done
```

Example results (WSL, loopback, no pipelining): about 7.5k op/s with `-c 1` (limited by round-trip latency) and about 50k op/s with `-c 8` for every command above.

Don't use `-P` above 1: the server handles one command per read. The server accepts at most 10 connections at a time, so keep `-c` at 9 or below.
