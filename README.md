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
| Sorted sets | `ZADD`, `ZREM`, `ZRANGE`, `ZSCORE` |

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
