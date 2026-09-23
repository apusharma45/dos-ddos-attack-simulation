#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

const int PORT = 8080;

// Maximum simultaneous requests
const int MAX_WORKERS = 10;

// Simulated processing delay
const int PROCESSING_TIME = 1500;

// Server statistics
atomic<int> activeWorkers(0);
atomic<int> totalRequests(0);
atomic<int> rejectedRequests(0);
atomic<int> completedRequests(0);

// Load HTML file
string loadHTML() {

    ifstream file("index.html");

    if (!file.is_open()) {
        return "<h1>index.html not found</h1>";
    }

    return string(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>()
    );
}

// Send complete HTTP response
void sendResponse(
    SOCKET client,
    int status,
    const string& statusText,
    const string& contentType,
    const string& body
) {

    string response =
        "HTTP/1.1 " +
        to_string(status) + " " +
        statusText + "\r\n" +

        "Content-Type: " +
        contentType + "\r\n" +

        "Content-Length: " +
        to_string(body.size()) + "\r\n" +

        "Connection: close\r\n" +
        "Cache-Control: no-store\r\n" +
        "\r\n" +
        body;

    size_t sent = 0;

    while (sent < response.size()) {

        int result = send(
            client,
            response.data() + sent,
            static_cast<int>(response.size() - sent),
            0
        );

        if (result <= 0) {
            break;
        }

        sent += result;
    }
}

// Handle one client
void handleClient(SOCKET client) {

    char buffer[2048] = {};

    int bytes = recv(
        client,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytes <= 0) {

        closesocket(client);
        activeWorkers--;

        return;
    }

    string request(buffer, bytes);

    if (request.find("GET /health") == 0) {

        sendResponse(
            client,
            200,
            "OK",
            "text/plain",
            "Server is healthy"
        );

    } else if (request.find("GET /work") == 0) {

        // Only the lab workload is deliberately expensive. The website and
        // health endpoint stay fast when the server has free capacity.
        this_thread::sleep_for(
            chrono::milliseconds(PROCESSING_TIME)
        );

        sendResponse(
            client,
            200,
            "OK",
            "text/plain",
            "Work completed"
        );

    } else {

        sendResponse(
            client,
            200,
            "OK",
            "text/html",
            loadHTML()
        );

    }

    closesocket(client);

    completedRequests++;
    activeWorkers--;
}

void printStatistics() {
    int previousTotal = 0;

    while (true) {
        this_thread::sleep_for(chrono::seconds(1));

        int currentTotal = totalRequests.load();

        cout << "[SERVER] rate="
             << currentTotal - previousTotal
             << " req/s | active="
             << activeWorkers.load()
             << "/" << MAX_WORKERS
             << " | completed="
             << completedRequests.load()
             << " | rejected="
             << rejectedRequests.load()
             << endl;

        previousTotal = currentTotal;
    }
}

int main() {

    WSADATA wsa;

    if (WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    ) != 0) {

        cerr << "Winsock initialization failed.\n";
        return 1;
    }

    SOCKET serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (serverSocket == INVALID_SOCKET) {

        cerr << "Socket creation failed.\n";

        WSACleanup();
        return 1;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    // Restrict server to localhost
    inet_pton(
        AF_INET,
        "127.0.0.1",
        &address.sin_addr
    );

    if (bind(
        serverSocket,
        (sockaddr*)&address,
        sizeof(address)
    ) == SOCKET_ERROR) {

        cerr << "Bind failed.\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    if (listen(serverSocket, SOMAXCONN)
        == SOCKET_ERROR) {

        cerr << "Listen failed.\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "================================\n";
    cout << "      SECURITY LAB SERVER\n";
    cout << "================================\n";

    cout << "Server: http://127.0.0.1:8080\n";
    cout << "Maximum workers: " << MAX_WORKERS << endl;
    cout << "Processing time: "
         << PROCESSING_TIME << " ms\n";

    cout << "\nWaiting for connections...\n";

    thread(printStatistics).detach();

    while (true) {

        SOCKET client = accept(
            serverSocket,
            nullptr,
            nullptr
        );

        if (client == INVALID_SOCKET) {
            continue;
        }

        totalRequests++;

        // Atomically reserve a worker
        int current = activeWorkers.load();

        bool accepted = false;

        while (current < MAX_WORKERS) {

            if (activeWorkers.compare_exchange_weak(
                current,
                current + 1
            )) {

                accepted = true;
                break;
            }
        }

        if (!accepted) {

            rejectedRequests++;

            // Read the already-sent HTTP request before closing. On Windows,
            // closing a socket with unread input can reset the connection and
            // hide the intended 503 response from the browser.
            DWORD rejectReadTimeout = 250;
            setsockopt(
                client,
                SOL_SOCKET,
                SO_RCVTIMEO,
                reinterpret_cast<const char*>(&rejectReadTimeout),
                sizeof(rejectReadTimeout)
            );

            char discardedRequest[2048] = {};
            recv(
                client,
                discardedRequest,
                sizeof(discardedRequest) - 1,
                0
            );

            string errorPage =
                "<html>"
                "<body style='font-family:Arial;"
                "text-align:center;margin-top:100px'>"
                "<h1 style='color:red'>"
                "503 Service Unavailable"
                "</h1>"
                "<h2>Server Overloaded</h2>"
                "<p>Please try again later.</p>"
                "</body>"
                "</html>";

            sendResponse(
                client,
                503,
                "Service Unavailable",
                "text/html",
                errorPage
            );

            closesocket(client);

            continue;
        }

        thread(
            handleClient,
            client
        ).detach();
    }

    closesocket(serverSocket);

    WSACleanup();

    return 0;
}
