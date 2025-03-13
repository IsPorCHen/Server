#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define PORT "8080"
#define BUFLEN 512

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <map>
#include <vector>
#include <string>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

WSADATA wsaData;
SOCKET ListenSocket = INVALID_SOCKET;
int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
struct addrinfo* result = NULL, * ptr = NULL, hints;

std::map<int, std::string> clients;
std::vector<std::string> chatHistory;
HANDLE hMutex;

std::string getCurrentTime() {
    time_t now = time(0);
    char buf[80];
    struct tm* timeinfo = localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buf);
}

void sendToAllClients(const std::string& message, int senderSocket = -1) {
    WaitForSingleObject(hMutex, INFINITE);
    for (const auto& client : clients) {
        if (client.first != senderSocket) {
            send(client.first, message.c_str(), message.size(), 0);
        }
    }
    ReleaseMutex(hMutex);
}

DWORD WINAPI ClientThread(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)lpParam;
    char recvbuf[BUFLEN];
    int iResult;

    const char* prompt = "Введите ваше имя: ";
    send(clientSocket, prompt, (int)strlen(prompt), 0);

    iResult = recv(clientSocket, recvbuf, BUFLEN, 0);
    std::string clientName(recvbuf, iResult);

    WaitForSingleObject(hMutex, INFINITE);
    clients[clientSocket] = clientName;
    ReleaseMutex(hMutex);

    sendToAllClients(clientName + " присоединился в чат.");

    for (const auto& msg : chatHistory) {
        send(clientSocket, msg.c_str(), msg.size(), 0);
    }

    do {
        iResult = recv(clientSocket, recvbuf, BUFLEN, 0);
        if (iResult > 0) {
            std::string message(recvbuf, iResult);
            char chatMessage[BUFLEN];
            sprintf(chatMessage, "[%s]: %s", clientName.c_str(), message.c_str());
            sendToAllClients(chatMessage, clientSocket);
        }
    } while (iResult > 0);

    printf("[Сервер] Клиент отключён.\n");
    closesocket(clientSocket);

    WaitForSingleObject(hMutex, INFINITE);
    clients.erase(clientSocket);
    ReleaseMutex(hMutex);
    return 0;
}


int main(int argc, char* argv[]) {
    setlocale(0, "rus");

    if (iResult != 0) {
        printf("Ошибка загрузки библиотеки");
        WSACleanup();
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    iResult = getaddrinfo(NULL, PORT, &hints, &result);

    if (iResult != 0) {
        printf("Ошибка получения адреса: %d\n", iResult);
        WSACleanup();
        return 2;
    }

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("Ошибка подключения сокета: %d\n", WSAGetLastError());
        WSACleanup();
        return 3;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("[Сервер] Ошибка привязки сокета: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 4;
    }

    freeaddrinfo(result);

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("[Сервер] Ошибка прослушивания порта: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 5;
    }

    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
        printf("[Сервер] Ошибка создания мьютекса: %d\n", GetLastError());
        WSACleanup();
        return 6;
    }

    printf("Сервер работает на порту %s...\n", PORT);

    while (true) {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("Ошибка accept: %d\n", WSAGetLastError());
            continue;
        }


        printf("[Сервер] Новый клиент подключился.\n");

        HANDLE hThread = CreateThread(NULL, 0, ClientThread, (LPVOID)ClientSocket, 0, NULL);
        if (hThread == NULL) {
            printf("Ошибка создания потока для клиента: %d\n", GetLastError());
            closesocket(ClientSocket);
        }
        else {
            CloseHandle(hThread);
        }
    }

    closesocket(ListenSocket);
    CloseHandle(hMutex);
    WSACleanup();
    return 0;
}
