#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <map>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #include <windows.h>
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

std::mutex clientsMutex;
std::mutex logMutex;
std::vector<std::pair<SOCKET, std::string>> clients;
std::ofstream logFile;

void printMessage(const std::string& message) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    wchar_t* wtext = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wtext, len);
    WriteConsoleW(hConsole, wtext, wcslen(wtext), NULL, NULL);
    WriteConsoleW(hConsole, L"\n", 1, NULL, NULL);
    delete[] wtext;
#else
    std::cout << message << std::endl;
#endif
}

std::string getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << '[' << (tm.tm_hour < 10 ? "0" : "") << tm.tm_hour << ':'
        << (tm.tm_min < 10 ? "0" : "") << tm.tm_min << ']';
    return oss.str();
}

void logMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::string logMsg = getCurrentTime() + " " + message;
    printMessage(logMsg);
    logFile << logMsg << std::endl;
    logFile.flush();
}

void broadcastMessage(const std::string& message, SOCKET senderSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& client : clients) {
        if (senderSocket == INVALID_SOCKET || client.first != senderSocket) {
            send(client.first, message.c_str(), message.length(), 0);
        }
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[1024];
    std::string username;
    
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        username = buffer;
        
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back({clientSocket, username});
        }
        
        logMessage(username + " присоединился к чату");
        
        std::string connectMsg = getCurrentTime() + " " + username + " присоединился к чату";
        broadcastMessage(connectMsg, clientSocket);
    }
    
    while (true) {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            break;
        }
        
        buffer[bytesReceived] = '\0';
        std::string message = buffer;
        
        if (message.substr(0, 6) == "/kick " && message.length() > 6) {
            std::string userToKick = message.substr(6);
            std::lock_guard<std::mutex> lock(clientsMutex);
            
            auto it = std::find_if(clients.begin(), clients.end(), 
                [&userToKick](const std::pair<SOCKET, std::string>& client) {
                    return client.second == userToKick;
                });
                
            if (it != clients.end()) {
                SOCKET kickedSocket = it->first;
                
                std::string kickMessage = "Вы были исключены из чата";
                send(kickedSocket, kickMessage.c_str(), kickMessage.length(), 0);
                
                std::string kickNotification = getCurrentTime() + " " + userToKick + " был исключен из чата";
                logMessage(kickNotification);
                
                closesocket(kickedSocket);
                
                clients.erase(it);
                
                broadcastMessage(kickNotification, INVALID_SOCKET);
            }
        } else {
            std::string formattedMessage = getCurrentTime() + " " + username + ": " + message;
            logMessage(username + ": " + message);
            broadcastMessage(formattedMessage, clientSocket);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::find_if(clients.begin(), clients.end(),
            [clientSocket](const std::pair<SOCKET, std::string>& client) {
                return client.first == clientSocket;
            });
            
        if (it != clients.end()) {
            std::string disconnectMsg = getCurrentTime() + " " + it->second + " покинул чат";
            logMessage(it->second + " покинул чат");
            clients.erase(it);
            broadcastMessage(disconnectMsg, clientSocket);
        }
    }
    
    closesocket(clientSocket);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
    
    int port = 1234;
    
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--port" && i + 1 < argc) {
            port = std::stoi(argv[i + 1]);
            i++;
        }
    }
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printMessage("Ошибка инициализации Winsock");
        return 1;
    }
#endif
    
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        printMessage("Ошибка создания сокета");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printMessage("Ошибка привязки сокета");
        closesocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        printMessage("Ошибка при прослушивании");
        closesocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    logFile.open("chat.log", std::ios::app);
    if (!logFile.is_open()) {
        printMessage("Ошибка открытия файла логов");
        closesocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    printMessage("Сервер запущен на порту " + std::to_string(port));
    logMessage("Сервер запущен на порту " + std::to_string(port));
    
    std::vector<std::thread> threads;
    
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        
        if (clientSocket == INVALID_SOCKET) {
            printMessage("Ошибка принятия подключения");
            continue;
        }
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        logMessage("Новое подключение с IP: " + std::string(clientIP));
        
        threads.push_back(std::thread(handleClient, clientSocket));
        threads.back().detach();
    }
    
    closesocket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    logFile.close();
    
    return 0;
} 