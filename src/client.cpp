#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #include <windows.h>
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

std::atomic<bool> isRunning(true);

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

std::string readConsoleInput() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    wchar_t wbuffer[1024] = {0};
    DWORD read = 0;
    DWORD mode = 0;
    
    GetConsoleMode(hConsole, &mode);
    
    ReadConsoleW(hConsole, wbuffer, 1024, &read, NULL);
    
    if (read >= 2 && wbuffer[read-2] == L'\r' && wbuffer[read-1] == L'\n') {
        wbuffer[read-2] = L'\0';
    } else if (read >= 1 && (wbuffer[read-1] == L'\r' || wbuffer[read-1] == L'\n')) {
        wbuffer[read-1] = L'\0';
    }
    
    int len = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, NULL, 0, NULL, NULL);
    char* buffer = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, len, NULL, NULL);
    
    std::string result(buffer);
    delete[] buffer;
    
    return result;
#else
    std::string input;
    std::getline(std::cin, input);
    return input;
#endif
}

void receiveMessages(SOCKET serverSocket) {
    char buffer[1024];
    
    while (isRunning) {
        int bytesReceived = recv(serverSocket, buffer, sizeof(buffer), 0);
        
        if (bytesReceived <= 0) {
            printMessage("Соединение с сервером потеряно");
            isRunning = false;
            break;
        }
        
        buffer[bytesReceived] = '\0';
        printMessage(buffer);
    }
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
    
    std::string serverIp = "127.0.0.1";
    int port = 1234;
    
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--ip" && i + 1 < argc) {
            serverIp = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "--port" && i + 1 < argc) {
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
    

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        printMessage("Ошибка создания сокета");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
        printMessage("Неверный IP адрес");
        closesocket(clientSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printMessage("Не удалось подключиться к серверу");
        closesocket(clientSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    printMessage("Подключено к серверу " + serverIp + ":" + std::to_string(port));
    printMessage("Введите ваше имя: ");
    
    std::string username = readConsoleInput();
    
    send(clientSocket, username.c_str(), username.length(), 0);
    
    std::thread receiveThread(receiveMessages, clientSocket);
    
    printMessage("Введите сообщение (или /exit для выхода):");
    
    while (isRunning) {
        std::string message = readConsoleInput();
        
        if (message == "/exit") {
            isRunning = false;
            break;
        }
        
        if (send(clientSocket, message.c_str(), message.length(), 0) == SOCKET_ERROR) {
            printMessage("Ошибка отправки сообщения");
            isRunning = false;
            break;
        }
    }
    
    if (receiveThread.joinable()) {
        receiveThread.join();
    }
    
    closesocket(clientSocket);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    return 0;
} 