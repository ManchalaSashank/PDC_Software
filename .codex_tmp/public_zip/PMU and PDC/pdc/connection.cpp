// #include <iostream>
// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <iomanip>

// #pragma comment(lib, "ws2_32.lib")

// using namespace std;

// #define PMU_IP "127.0.0.1"   // Change to actual PMU IP
// #define PMU_PORT 4712        // Must match PMU listening port
// #define BUFFER_SIZE 4096

// int main()
// {
//     WSADATA wsaData;
//     SOCKET clientSocket = INVALID_SOCKET;

//     // 1️⃣ Initialize Winsock
//     if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
//     {
//         cerr << "[PDC] WSAStartup failed. Error: " << WSAGetLastError() << endl;
//         return 1;
//     }

//     cout << "[PDC] Winsock initialized\n";

//     // 2️⃣ Create socket
//     clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//     if (clientSocket == INVALID_SOCKET)
//     {
//         cerr << "[PDC] Socket creation failed. Error: " << WSAGetLastError() << endl;
//         WSACleanup();
//         return 1;
//     }

//     cout << "[PDC] Socket created\n";

//     // 3️⃣ Setup PMU address
//     sockaddr_in serverAddress{};
//     serverAddress.sin_family = AF_INET;
//     serverAddress.sin_port = htons(PMU_PORT);
//     inet_pton(AF_INET, PMU_IP, &serverAddress.sin_addr);

//     cout << "[PDC] Connecting to PMU...\n";

//     // 4️⃣ Connect
//     if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
//     {
//         cerr << "[PDC] Connection failed. Error: " << WSAGetLastError() << endl;
//         closesocket(clientSocket);
//         WSACleanup();
//         return 1;
//     }

//     cout << "[PDC] Connected successfully\n";

//     // 🚫 REMOVE sending garbage cmdframe for now
//     // We will add proper IEEE CMD frame later

//     unsigned char buffer[BUFFER_SIZE];

//     // 5️⃣ Safe receive loop
//     while (true)
//     {
//         int bytesReceived = recv(clientSocket, (char*)buffer, BUFFER_SIZE, 0);

//         if (bytesReceived > 0)
//         {
//             cout << "[PDC] Received " << bytesReceived << " bytes\n";

//             // Print in hex for debugging
//             for (int i = 0; i < bytesReceived; i++)
//             {
//                 cout << hex << setw(2) << setfill('0')
//                      << static_cast<int>(buffer[i]) << " ";
//             }
//             cout << dec << endl;
//         }
//         else if (bytesReceived == 0)
//         {
//             cout << "[PDC] PMU disconnected gracefully\n";
//             break;
//         }
//         else
//         {
//             cerr << "[PDC] recv failed. Error: "
//                  << WSAGetLastError() << endl;
//             break;
//         }
//     }

//     // 6️⃣ Cleanup
//     closesocket(clientSocket);
//     WSACleanup();

//     cout << "[PDC] Shutdown complete\n";
//     return 0;
// }





#include "connection.h"
#include <iostream>
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

    return (connect(sock, (sockaddr*)&serverAddress, sizeof(serverAddress)) != SOCKET_ERROR);
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
    WSACleanup();
}