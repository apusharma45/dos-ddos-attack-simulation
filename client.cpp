#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

// Safety boundary: this lab client can only target the local machine.
const char* HOST = "127.0.0.1";
const int PORT = 8080;

// Each scenario exceeds the server's 10-worker capacity while remaining
// small and bounded for a local classroom demonstration.
const int DOS_CONNECTIONS = 15;
const int DDOS_BOTS = 30;
const int MIN_DURATION_SECONDS = 10;
const int MAX_DURATION_SECONDS = 120;
const int RETRY_DELAY_MS = 25;

atomic<int> successful(0);
atomic<int> rejected(0);
atomic<int> failed(0);

enum class RequestResult {
    Successful,
    Rejected,
    Failed
};

RequestResult makeRequest(const string& botId) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET) {
        return RequestResult::Failed;
    }

    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &address.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) == SOCKET_ERROR) {
        closesocket(sock);
        return RequestResult::Failed;
    }

    // /work deliberately represents an expensive application operation.
    // The identity header is only a label for this local simulation.
    string request =
        "GET /work HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: SecurityLabBot/1.0\r\n"
        "X-Lab-Client-ID: " + botId + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    int sent = send(sock, request.c_str(), static_cast<int>(request.size()), 0);
    if (sent <= 0) {
        closesocket(sock);
        return RequestResult::Failed;
    }

    char buffer[2048] = {};
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    closesocket(sock);

    if (bytes <= 0) {
        return RequestResult::Failed;
    }

    string response(buffer, bytes);
    if (response.find("HTTP/1.1 200") == 0) {
        return RequestResult::Successful;
    }
    if (response.find("HTTP/1.1 503") == 0) {
        return RequestResult::Rejected;
    }

    return RequestResult::Failed;
}

void runBot(const string& botId,
            chrono::steady_clock::time_point deadline) {
    while (chrono::steady_clock::now() < deadline) {
        switch (makeRequest(botId)) {
            case RequestResult::Successful:
                successful++;
                break;
            case RequestResult::Rejected:
                rejected++;
                break;
            case RequestResult::Failed:
                failed++;
                break;
        }

        // Prevent an unbounded tight loop while still keeping pressure steady.
        this_thread::sleep_for(chrono::milliseconds(RETRY_DELAY_MS));
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "Winsock initialization failed.\n";
        return 1;
    }

    cout << "========================================\n";
    cout << "  LOCAL AVAILABILITY ATTACK SIMULATION\n";
    cout << "========================================\n";
    cout << "1. DoS   - one client, multiple connections\n";
    cout << "2. DDoS  - multiple simulated bot clients\n";
    cout << "\nSelect mode: ";

    int mode = 0;
    cin >> mode;
    if (mode != 1 && mode != 2) {
        cout << "Invalid selection.\n";
        WSACleanup();
        return 1;
    }

    cout << "Attack duration in seconds ("
         << MIN_DURATION_SECONDS << "-" << MAX_DURATION_SECONDS << "): ";

    int durationSeconds = 0;
    cin >> durationSeconds;
    if (!cin || durationSeconds < MIN_DURATION_SECONDS ||
        durationSeconds > MAX_DURATION_SECONDS) {
        cout << "Invalid duration.\n";
        WSACleanup();
        return 1;
    }

    const int workerCount = (mode == 1) ? DOS_CONNECTIONS : DDOS_BOTS;
    const string scenario = (mode == 1) ? "DoS" : "DDoS";

    cout << "\nScenario: " << scenario << '\n';
    cout << "Target: http://127.0.0.1:8080 (loopback only)\n";
    if (mode == 1) {
        cout << "Logical clients: 1\n";
        cout << "Simultaneous connections: " << workerCount << '\n';
    } else {
        cout << "Simulated bots: " << workerCount << '\n';
        cout << "Connections per bot: 1\n";
    }
    cout << "\nKeep http://127.0.0.1:8080 open in a browser.\n";
    cout << "Click Check Server during the attack to observe the overload.\n\n";

    auto deadline = chrono::steady_clock::now() +
                    chrono::seconds(durationSeconds);

    vector<thread> workers;
    workers.reserve(workerCount);

    for (int i = 1; i <= workerCount; ++i) {
        string botId = (mode == 1)
            ? "dos-source"
            : "bot-" + to_string(i);
        workers.emplace_back(runBot, botId, deadline);
    }

    int previousTotal = 0;
    for (int elapsed = 1; elapsed <= durationSeconds; ++elapsed) {
        this_thread::sleep_for(chrono::seconds(1));

        int currentTotal = successful.load() + rejected.load() + failed.load();
        int requestsThisSecond = currentTotal - previousTotal;
        previousTotal = currentTotal;

        cout << "[" << elapsed << "/" << durationSeconds << " sec] "
             << "rate=" << requestsThisSecond << " req/s"
             << " | accepted=" << successful.load()
             << " | overloaded=" << rejected.load()
             << " | failed=" << failed.load() << '\n';
    }

    for (auto& worker : workers) {
        worker.join();
    }

    int total = successful.load() + rejected.load() + failed.load();

    cout << "\n========================================\n";
    cout << "              FINAL RESULTS\n";
    cout << "========================================\n";
    cout << "Total requests: " << total << '\n';
    cout << "Accepted (200): " << successful.load() << '\n';
    cout << "Overloaded (503): " << rejected.load() << '\n';
    cout << "Connection failures/timeouts: " << failed.load() << '\n';
    cout << "\nAttack traffic stopped. The browser should recover automatically.\n";

    WSACleanup();
    return 0;
}
