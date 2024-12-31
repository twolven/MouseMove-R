#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <fstream>
#include <map>
#pragma comment(lib, "ws2_32.lib")

#define IPC_PATH "\\\\.\\pipe\\vhclient"

// Global variables
bool isFocused = false;
bool isConnected = false;
std::chrono::steady_clock::time_point lastHeartbeat = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point lastWarning = std::chrono::steady_clock::now();

// Global config
std::string DEVICE_ID = "VH.DeviceNo";  // Default value
int SERVER_PORT = 8080;              // Default value
int POLL_INTERVAL = 5;               // Default value
int HEARTBEAT_TIMEOUT = 15;          // Default value
int WARNING_INTERVAL = 60;           // Default value

bool LoadConfig() {
    std::ifstream file("config.txt");
    if (!file.is_open()) {
        std::cout << "No config.txt found, using default values\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (key == "DEVICE_ID") DEVICE_ID = value;
            else if (key == "SERVER_PORT") SERVER_PORT = std::stoi(value);
            else if (key == "POLL_INTERVAL") POLL_INTERVAL = std::stoi(value);
            else if (key == "HEARTBEAT_TIMEOUT") HEARTBEAT_TIMEOUT = std::stoi(value);
            else if (key == "WARNING_INTERVAL") WARNING_INTERVAL = std::stoi(value);
        }
    }

    std::cout << "Loaded configuration:\n"
        << "DEVICE_ID: " << DEVICE_ID << "\n"
        << "SERVER_PORT: " << SERVER_PORT << "\n"
        << "POLL_INTERVAL: " << POLL_INTERVAL << "\n"
        << "HEARTBEAT_TIMEOUT: " << HEARTBEAT_TIMEOUT << "\n"
        << "WARNING_INTERVAL: " << WARNING_INTERVAL << "\n";

    return true;
}

bool SendToIPC(const std::string& command, std::string& response) {
    HANDLE hPipe = CreateFile(TEXT(IPC_PATH), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open IPC pipe. Error: " << GetLastError() << std::endl;
        return false;
    }

    DWORD bytesWritten;
    if (!WriteFile(hPipe, command.c_str(), static_cast<DWORD>(command.size()), &bytesWritten, NULL)) {
        std::cerr << "Failed to write to IPC pipe. Error: " << GetLastError() << std::endl;
        CloseHandle(hPipe);
        return false;
    }

    char buffer[1024];
    DWORD bytesRead;
    if (!ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        std::cerr << "Failed to read from IPC pipe. Error: " << GetLastError() << std::endl;
        CloseHandle(hPipe);
        return false;
    }
    buffer[bytesRead] = '\0';
    response = buffer;

    CloseHandle(hPipe);
    return true;
}

void ShowWarning(const std::string& message) {
    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    wchar_t* wstr = new wchar_t[wchars_num];
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wstr, wchars_num);

    MessageBoxW(NULL, wstr, L"MouseMoveR Warning", MB_ICONWARNING | MB_OK);

    delete[] wstr;
}

void HandleClientConnection(SOCKET clientSocket) {
    char buffer[1024] = { 0 };
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string command(buffer);
        std::cout << "Received command: " << command << std::endl;

        std::string response;
        if (command == "FOCUSED") {
            isFocused = true;
            isConnected = true;
            lastHeartbeat = std::chrono::steady_clock::now();
            if (SendToIPC("USE," + DEVICE_ID, response)) {
                std::cout << "Sent USE command to VirtualHere Client API. Response: " << response << std::endl;
                send(clientSocket, "ACK", 3, 0);
            }
            else {
                std::cerr << "Failed to send USE command to VirtualHere Client API" << std::endl;
            }
        }
        else if (command == "NOT_FOCUSED") {
            isFocused = false;
            isConnected = true;
            lastHeartbeat = std::chrono::steady_clock::now();
            if (SendToIPC("DEVICE INFO," + DEVICE_ID, response)) {
                std::cout << "DEVICE INFO Response: " << response << std::endl;
                if (response.find("IN USE BY: NO ONE") == std::string::npos) {
                    if (SendToIPC("STOP USING," + DEVICE_ID, response)) {
                        std::cout << "Sent STOP USING command to VirtualHere Client API. Response: " << response << std::endl;
                        send(clientSocket, "ACK", 3, 0);
                    }
                    else {
                        std::cerr << "Failed to send STOP USING command to VirtualHere Client API" << std::endl;
                    }
                }
                else {
                    std::cout << "Device " << DEVICE_ID << " is not in use, no need to send STOP USING command\n";
                    send(clientSocket, "ACK", 3, 0);
                }
            }
            else {
                std::cerr << "Failed to get DEVICE INFO via IPC.\n";
            }
        }
        else if (command == "HEARTBEAT") {
            lastHeartbeat = std::chrono::steady_clock::now();
            isConnected = true;
            send(clientSocket, "ACK", 3, 0);
        }
        else if (command == "VHUSB_NOT_RUNNING") {
            ShowWarning("vhusbdwin64.exe is not running on the Moonlight computer. Please start it to ensure proper functionality.");
            send(clientSocket, "ACK", 3, 0);
        }
    }

    closesocket(clientSocket);
}

void StartServer() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "Server started, waiting for connections...\n";

    std::thread([&]() {
        while (true) {
            clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed.\n";
                continue;
            }
            std::thread(HandleClientConnection, clientSocket).detach();
        }
        }).detach();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() > HEARTBEAT_TIMEOUT) {
            isConnected = false;
        }

        if (!isConnected && std::chrono::duration_cast<std::chrono::seconds>(now - lastWarning).count() > WARNING_INTERVAL) {
            ShowWarning("No connection to Moonlight computer program. Please ensure MouseMove.exe is running.");
            lastWarning = now;
        }

        if (isFocused) {
            std::string response;
            if (SendToIPC("DEVICE INFO," + DEVICE_ID, response)) {
                std::cout << "DEVICE INFO Response: " << response << std::endl;
                if (response.find("IN USE BY: NO ONE") != std::string::npos) {
                    std::cerr << "Device " << DEVICE_ID << " not in use, attempting to re-use...\n";
                    SendToIPC("USE," + DEVICE_ID, response);
                }
            }
            else {
                std::cerr << "Failed to check device status via IPC.\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(POLL_INTERVAL));
    }

    closesocket(serverSocket);
    WSACleanup();
}

int main() {
    LoadConfig();  // Load configuration from file
    StartServer(); // Start the server with loaded configuration
    return 0;
}