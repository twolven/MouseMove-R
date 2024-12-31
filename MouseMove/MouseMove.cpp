#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")

// Global configuration
std::string SERVER_IP = "192.168.1.1";
int SERVER_PORT = 8080;
int HEARTBEAT_INTERVAL = 5;
int POLL_INTERVAL = 2;
int VHUSB_CHECK_INTERVAL = 30;
std::wstring WINDOW_TITLE = L"Moonlight";

// Shutdown handling
std::atomic<bool> running{ true };

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

            if (key == "SERVER_IP") SERVER_IP = value;
            else if (key == "SERVER_PORT") SERVER_PORT = std::stoi(value);
            else if (key == "HEARTBEAT_INTERVAL") HEARTBEAT_INTERVAL = std::stoi(value);
            else if (key == "POLL_INTERVAL") POLL_INTERVAL = std::stoi(value);
            else if (key == "VHUSB_CHECK_INTERVAL") VHUSB_CHECK_INTERVAL = std::stoi(value);
            else if (key == "WINDOW_TITLE") {
                std::wstring wtemp(value.begin(), value.end());
                WINDOW_TITLE = wtemp;
            }
        }
    }

    std::cout << "Loaded configuration:\n"
        << "SERVER_IP: " << SERVER_IP << "\n"
        << "SERVER_PORT: " << SERVER_PORT << "\n"
        << "HEARTBEAT_INTERVAL: " << HEARTBEAT_INTERVAL << "\n"
        << "POLL_INTERVAL: " << POLL_INTERVAL << "\n"
        << "VHUSB_CHECK_INTERVAL: " << VHUSB_CHECK_INTERVAL << "\n";

    return true;
}

bool SendCommandToServer(const std::string& command) {
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return false;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return false;
    }

    // Set timeout for operations
    DWORD timeout = 5000; // 5 seconds
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    serverAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported.\n";
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }
    serverAddr.sin_port = htons(SERVER_PORT);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    send(clientSocket, command.c_str(), static_cast<int>(command.size()), 0);

    char buffer[1024];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        if (std::string(buffer) == "ACK") {
            closesocket(clientSocket);
            WSACleanup();
            return true;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return false;
}

bool IsProcessRunning(const wchar_t* processName) {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create process snapshot. Error: " << GetLastError() << std::endl;
        return false;
    }

    bool exists = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            // Check for both possible VirtualHere process names
            if (_wcsicmp(entry.szExeFile, L"vhusbdwin64.exe") == 0 ||
                _wcsicmp(entry.szExeFile, L"vhusbdwinw64.exe") == 0) {
                exists = true;
                std::wcout << L"Found VirtualHere process: " << entry.szExeFile << std::endl;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return exists;
}

void MonitorWindowFocus() {
    bool isFocused = false;
    HWND moonlightWindow = nullptr;
    auto lastVHUSBCheck = std::chrono::steady_clock::now();

    while (running) {
        moonlightWindow = FindWindow(NULL, WINDOW_TITLE.c_str());
        if (moonlightWindow) {
            HWND foregroundWindow = GetForegroundWindow();
            if (foregroundWindow == moonlightWindow) {
                if (!isFocused) {
                    if (SendCommandToServer("FOCUSED")) {
                        isFocused = true;
                        std::cout << "Status: Moonlight window focused\n";
                    }
                }
            }
            else {
                if (isFocused) {
                    if (SendCommandToServer("NOT_FOCUSED")) {
                        isFocused = false;
                        std::cout << "Status: Moonlight window not focused\n";
                    }
                }
            }

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastVHUSBCheck).count() > VHUSB_CHECK_INTERVAL) {
                std::wcout << L"Checking for process: vhusbdwin64.exe" << std::endl;
                if (!IsProcessRunning(L"vhusbdwin64.exe")) {
                    std::wcout << L"Process check failed" << std::endl;
                    SendCommandToServer("VHUSB_NOT_RUNNING");
                    std::cout << "Warning: vhusbdwin64.exe is not running\n";
                }
                else {
                    std::wcout << L"Process is running" << std::endl;
                }
                lastVHUSBCheck = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else {
            std::cout << "Moonlight window not found, polling...\n";
            SendCommandToServer("NOT_FOCUSED");
            isFocused = false;
            std::this_thread::sleep_for(std::chrono::seconds(POLL_INTERVAL));
        }
    }
}

void SendHeartbeat() {
    while (running) {
        if (!SendCommandToServer("HEARTBEAT")) {
            std::cerr << "Failed to send heartbeat. Attempting to reconnect...\n";
            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
        }
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
    }
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        std::cout << "\nShutting down...\n";
        running = false;
        return TRUE;
    }
    return FALSE;
}

int main() {
    // Load configuration
    LoadConfig();

    // Set up console handler for graceful shutdown
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    std::thread monitorThread(MonitorWindowFocus);
    std::thread heartbeatThread(SendHeartbeat);

    monitorThread.join();
    heartbeatThread.join();

    return 0;
}