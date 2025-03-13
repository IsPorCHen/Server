#define WIN32_LEAN_AND_MEAN
#define PORT "8080"
#define BUFLEN 512

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

HANDLE hMutex;
WSADATA wsaData;
SOCKET ConnectSocket = INVALID_SOCKET;
SECURITY_ATTRIBUTES sa;
int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
struct addrinfo* result = NULL,
    * ptr = NULL,
    hints;

DWORD WINAPI ReceiveMessages(LPVOID lpParam) {
    SOCKET clientSocket = *(SOCKET*)lpParam;
    char recvbuf[BUFLEN];
    int iResult;

    while (true) {
        iResult = recv(clientSocket, recvbuf, BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';  // Добавляем нулевой символ в конце строки

            // Печатаем полученное сообщение с новой строки
            cout << "\r" << recvbuf << endl; // Для корректного вывода на новую строку
            cout << "> ";  // Снова выводим приглашение для ввода
        }
        else if (iResult == 0) {
            cout << "Соединение с сервером закрыто." << endl;
            break;
        }
        else {
            cout << "Ошибка получения данных: " << WSAGetLastError() << endl;
            break;
        }
    }
    return 0;
}


int main(int argc, char* argv[]) {
    setlocale(0, "rus");

    if (iResult != 0) {
        printf("Ошибка загрузки бибилиотеки\n");
        WSACleanup();
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(argv[1], PORT, &hints, &result);
    if (iResult != 0) {
        printf("Ошибка\n");
        WSACleanup();
        return 2;
    }

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    ptr = result;

    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Ошибка подключения сокета: %d", WSAGetLastError());
        WSACleanup();
        return 3;
    }

    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("Ошибка подключения к сокету: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        ConnectSocket = INVALID_SOCKET;
        return 10;
    }

    // Получаем имя
    char recvbuf[BUFLEN];
    iResult = recv(ConnectSocket, recvbuf, BUFLEN, 0);
    if (iResult > 0) {
        printf("%.*s", iResult, recvbuf);

        string name;
        getline(cin, name);

        iResult = send(ConnectSocket, name.c_str(), (int)name.length(), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Ошибка отправки имени: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }
    }

    // Запускаем поток для получения сообщений
    DWORD threadId;
    CreateThread(NULL, 0, ReceiveMessages, &ConnectSocket, 0, &threadId);

    // Основной цикл для отправки сообщений
    while (true)
    {
        cout << "> ";
        fflush(stdout);

        string input;
        getline(cin, input);

        const char* sendbuf = input.c_str();
        iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Ошибка отправки сообщения: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 4;
        }

        cout << endl;
    }

    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("Программа завершилась с ошибкой: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 5;
    }
}
