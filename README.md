# Local DoS and DDoS Simulation

A controlled C++ security lab that demonstrates how sustained request traffic can affect the availability of a web server. The server and traffic generator operate only on `127.0.0.1`.

## Features

- Local HTTP server with a capacity of 10 simultaneous requests
- DoS simulation using one logical client with multiple connections
- DDoS simulation using multiple virtual bot clients
- Configurable attack duration from 10 to 120 seconds
- Browser-based availability check
- Live request and overload statistics

## Requirements

- Windows
- CMake 3.16 or later
- Ninja
- A C++17-compatible compiler

## Build

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

Start the server in the first terminal:

```powershell
.\build\server.exe
```

Open `http://127.0.0.1:8080` in a browser.

Start the traffic generator in a second terminal:

```powershell
.\build\client.exe
```

Select the DoS or DDoS scenario and enter a duration. During the simulation, click **Check Server** in the browser to observe the overloaded response. Click it again after the simulation finishes to verify that the server has recovered.

## Simulation Modes

- **DoS:** one logical source maintains 15 simultaneous connections.
- **DDoS:** 30 simulated bots each maintain one connection.

All simulated bots use the loopback interface. Their identities represent separate bots for demonstration purposes; they are not separate network hosts.

## Safety

This project is intended for local educational use only. The destination is hard-coded to `127.0.0.1`, and the duration and concurrency are bounded. Do not modify it to target systems without explicit authorization.
