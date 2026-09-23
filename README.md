#build the project
cmake -S . -B build -G Ninja
cmake --build build

#Run server: Terminal 1
./build/server.exe

#Run Clients: Terminal 2
./build/client.exe

#Demo procedure
#1. Open http://127.0.0.1:8080 before starting the client.
#2. Choose DoS or DDoS and a duration such as 30 seconds.
#3. Click Check Server during the attack to observe the overload.
#4. Click it again after the attack to verify that the server recovered.
