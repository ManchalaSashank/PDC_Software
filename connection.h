#pragma once
#include <winsock2.h>

bool initializeWinsock();
bool connectToPMU(const char* ip, int port, SOCKET& sock);
int receiveBytes(SOCKET sock, unsigned char* buffer, int size);
bool sendBytes(SOCKET sock, const unsigned char* data, int size);
void closeConnection(SOCKET sock);
void cleanupWinsock();