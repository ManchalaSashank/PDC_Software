#include "connection.h"
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

bool initializeWinsock()
{
    WSADATA wsaData;
    return (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
}

bool connectToPMU(const char* ip, int port, SOCKET& sock)
{
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return false;

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serverAddress.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    return true;
}

int receiveBytes(SOCKET sock, unsigned char* buffer, int size)
{
    return recv(sock, (char*)buffer, size, 0);
}

bool sendBytes(SOCKET sock, const unsigned char* data, int size)
{
    return (send(sock, (const char*)data, size, 0) != SOCKET_ERROR);
}

void closeConnection(SOCKET sock)
{
    closesocket(sock);
}

void cleanupWinsock()
{
    WSACleanup();
}
