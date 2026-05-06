# C++ LAN Chat — Encrypted, Lightweight, High-Concurrency

A real-time LAN chat server built in pure C++ with automatic message encryption, no external dependencies.

## Project Structure

```
socket/
├── src/                    # Source files
│   ├── main.cpp           # Server entry, LAN IP detection, thread management
│   ├── handlers.cpp       # API handlers, HTTP routing, session key generation
│   ├── utils.cpp          # JSON utilities, timestamps, ID generation
│   └── models.cpp         # Data models & global state
├── include/               # Header files
│   ├── handlers.h         # Handler declarations
│   ├── models.h           # Model structures & server limits
│   └── utils.h            # Utility function declarations
├── templates/             # Frontend
│   └── index.html         # Web UI with transparent E2E encryption
├── Makefile              # Build configuration (-Os, strip)
└── server               # Compiled binary (~76KB)
```

## Quick Start

```bash
cd socket
make rebuild && make run
```

Kill a stuck server: `lsof -ti:8080 | xargs kill -9`

## Features

| Feature | Details |
|---|---|
| **Encryption** | Automatic — server generates a session key, messages encrypted in transit |
| **Concurrency** | 100+ users, 64KB thread stacks, listen backlog 128, connection limiter |
| **Memory** | 76KB binary, capped message history (200), no `stringstream` allocations |
| **LAN Access** | Auto-detects local IP, displays URL at startup |
| **Security** | Duplicate username rejection, heartbeat-based stale user pruning |

## Architecture

### MVC Pattern

- **Models** (`models.cpp/h`): `User`, `Message` structs, global state with mutex protection
- **Controllers** (`handlers.cpp/h`): API handlers with lock-optimized status endpoint
- **View** (`templates/index.html`): Responsive dark-mode UI with transparent encryption

### Server Optimizations

- **64KB thread stacks** (vs default 512KB) — 8× less memory per connection
- **Template caching** — HTML read once from disk, served from memory
- **Lock-optimized status** — data snapshot under lock, JSON built outside lock
- **TCP_NODELAY** — faster response delivery on client sockets
- **File descriptor limit** raised to 4096 via `setrlimit`
- **Connection limiter** — returns HTTP 503 above 200 concurrent connections

### Encryption Flow

```
Browser                    Network Wire              Server Memory
────────────────      ──────────────────      ──────────────────
Types: "Hello!"   →   Sends: "dG8xOmFi..."  →  Stores: "dG8xOmFi..."
Sees:  "Hello!"       (encrypted base64)        (encrypted ciphertext)
```

Session key is auto-generated at server startup, embedded in the served HTML page.
All chat members get the key by loading the page. Network sniffers see only ciphertext.

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Serve HTML UI (with session key injected) |
| POST | `/api/join` | Join chat with username |
| POST | `/api/message` | Send encrypted message |
| GET | `/api/status` | Get users and messages (with heartbeat) |
| POST | `/api/leave` | Leave chat |

## Build Options

```bash
make              # Build
make run          # Build and run
make clean        # Remove build artifacts
make rebuild      # Clean and rebuild
```

## Server Limits (configurable in `models.h`)

| Constant | Default | Purpose |
|---|---|---|
| `MAX_MESSAGE_HISTORY` | 200 | Messages kept in memory |
| `HEARTBEAT_TIMEOUT_SECONDS` | 10 | Stale user timeout |
| `MAX_CONNECTIONS` | 200 | Max concurrent connections |
