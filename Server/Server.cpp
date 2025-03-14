#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define PORT "8080"
#define BUFLEN 512
#define SPAM_TIMEOUT 1

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <map>
#include <vector>
#include <string>
#include <ctime>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

WSADATA wsaData;
SOCKET ListenSocket = INVALID_SOCKET;
int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
struct addrinfo* result = NULL, * ptr = NULL, hints;

std::map<int, std::string> clients;
std::map<int, time_t> lastMessageTime;
std::vector<std::string> chatHistory;
HANDLE hMutex;

bool isNameTaken(const std::string& name) {
    for (const auto& client : clients) {
        if (client.second == name) {
            return true;
        }
    }
    return false;
}

void logToFile(const std::string& message) {
    FILE* logFile = fopen("chat_log.txt", "a");
    if (logFile != NULL) {
        fprintf(logFile, "%s\n", message.c_str());
        fclose(logFile);
    }
    else {
        printf("[Сервер] Не удалось открыть файл для записи!\n");
    }
}

void sendToAllClients(const std::string& message, int senderSocket = -1) {
    WaitForSingleObject(hMutex, INFINITE);

    logToFile(message);

    for (const auto& client : clients) {
        if (client.first != senderSocket) {
            send(client.first, message.c_str(), message.size(), 0);
        }
    }

    ReleaseMutex(hMutex);
}

bool isSpam(int clientSocket) {
    time_t currentTime = time(0);
    if (lastMessageTime.find(clientSocket) != lastMessageTime.end()) {
        double timeDifference = difftime(currentTime, lastMessageTime[clientSocket]);
        if (timeDifference < SPAM_TIMEOUT) {
            return true;
        }
    }
    lastMessageTime[clientSocket] = currentTime;
    return false;
}

DWORD WINAPI ClientThread(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)lpParam;
    char recvbuf[BUFLEN];
    int iResult;
    std::string clientName;

    while (true) {
        const char* prompt = "Введите ваше имя: ";
        send(clientSocket, prompt, (int)strlen(prompt), 0);

        iResult = recv(clientSocket, recvbuf, BUFLEN, 0);
        if (iResult <= 0) {
            closesocket(clientSocket);
            return 0;
        }

        clientName = std::string(recvbuf, iResult);
        clientName.erase(clientName.find_last_not_of(" \n\r") + 1);

        WaitForSingleObject(hMutex, INFINITE);
        if (!isNameTaken(clientName)) {
            clients[clientSocket] = clientName;
            ReleaseMutex(hMutex);
            break;
        }
        ReleaseMutex(hMutex);

        const char* nameTakenMsg = "Этот ник уже занят. Попробуйте другой.\n";
        send(clientSocket, nameTakenMsg, (int)strlen(nameTakenMsg), 0);
    }

    printf("[%s] присоединился в чат\n", clientName.c_str());
    logToFile("[" + clientName + "] присоединился в чат");
    sendToAllClients(clientName + " присоединился в чат.");

    iResult = recv(clientSocket, recvbuf, BUFLEN, 0);
    if (iResult == SOCKET_ERROR) {
        printf("[Сервер] Ошибка при получении имени: %d\n", WSAGetLastError());
        logToFile("[Сервер] Ошибка при получении имени: " + WSAGetLastError());
        closesocket(clientSocket);
    }
    else {

        WaitForSingleObject(hMutex, INFINITE);
        clients[clientSocket] = clientName;
        ReleaseMutex(hMutex);

        for (const auto& msg : chatHistory) {
            send(clientSocket, msg.c_str(), msg.size(), 0);
        }

        do {
            iResult = recv(clientSocket, recvbuf, BUFLEN, 0);
            if (iResult >= 0) {
                std::string message(recvbuf, iResult);

                if (isSpam(clientSocket)) {
                    printf("[Сервер]-[%s]: Вы отправляете сообщения слишком быстро. Попробуйте позже\n", clientName.c_str());
                    logToFile("[Сервер]-[" + clientName + "]: Вы отправляете сообщения слишком быстро. Попробуйте позже");
                    send(clientSocket, "Вы отправляете сообщения слишком быстро. Попробуйте позже.\n", 59, 0);
                    continue;
                }

                if (message[0] == '/') {
                    if (message == "/users") {
                        printf("[%s]-[Сервер]: /users\n", clientName.c_str(), message.c_str());
                        logToFile("[" + clientName + "] - [Сервер]: /users присоединился в чат");
                        std::string userList = "Активные пользователи: ";
                        for (const auto& client : clients) {
                            userList += client.second + ", ";
                        }
                        if (userList.length() > 0) {
                            userList.pop_back();
                            userList.pop_back();
                        }
                        logToFile("[Сервер] - [" + clientName + "]: Активные пользователи: " + clientName.c_str() + " " + userList.c_str());
                        printf("[Сервер] - [%s]: Активные пользователи: \n", clientName.c_str(), userList.c_str());
                        send(clientSocket, userList.c_str(), userList.length(), 0);
                    }
                    else {
                        send(clientSocket, "Неизвестная команда.\n", 21, 0);
                        printf("[Сервер] - [%s]: Неизвестная команда\n", clientName.c_str());
                        logToFile("[Сервер] - [" + clientName + "] присоединился в чат");
                    }
                }
                else {
                    char chatMessage[BUFLEN];
                    printf("[%s]: %s\n", clientName.c_str(), message);
                    logToFile("[" + clientName + "]: " + message);
                    sendToAllClients(chatMessage, clientSocket);
                }
            }
        } while (iResult > 0);

        printf("[Сервер] Клиент[%s] отключён.\n", clientName);
        logToFile("[Сервер] Клиент[" + clientName + "] отключён.");
        closesocket(clientSocket);

        WaitForSingleObject(hMutex, INFINITE);
        clients.erase(clientSocket);
        ReleaseMutex(hMutex);
        return 0;
    }
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
