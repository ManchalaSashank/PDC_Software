#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Constants based on IEEE C37.118.2 ---
const uint8_t SYNC_DATA = 0xAA;
const uint8_t SYNC_HDR = 0xAA;
const uint8_t SYNC_CFG1 = 0xAA;
const uint8_t SYNC_CFG2 = 0xAA;
const uint8_t SYNC_CMD = 0xAA;

const uint8_t TYPE_DATA = 0x01;
const uint8_t TYPE_HDR = 0x11;
const uint8_t TYPE_CFG1 = 0x21;
const uint8_t TYPE_CFG2 = 0x31;
const uint8_t TYPE_CMD = 0x41;

const uint16_t CMD_TURN_OFF_TX = 0x0001;
const uint16_t CMD_TURN_ON_TX = 0x0002;
const uint16_t CMD_SEND_HDR = 0x0003;
const uint16_t CMD_SEND_CFG1 = 0x0004;
const uint16_t CMD_SEND_CFG2 = 0x0005;

// --- Configuration ---
const uint16_t DEFAULT_PMU_ID = 1;
const uint16_t DEFAULT_PORT = 4712;
const uint16_t DATA_RATE = 50;

const bool USE_FLOAT_FORMAT = true;
const bool USE_POLAR_FORMAT = true;

const uint16_t PHASOR_COUNT = 3;
const uint16_t ANALOG_COUNT = 4;
const uint16_t DIGITAL_COUNT = 0;

std::string normalizeStationName(const std::string& input, uint16_t pmuId) {
    std::string name = input.empty() ? ("SIM_PMU_" + std::to_string(pmuId)) : input;
    name.resize(16, ' ');
    return name;
}

void printUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " [port] [pmuId] [stationName]\n";
    std::cout << "Example: " << exeName << " 4713 2 PMU_B\n";
}

uint16_t calculate_crc(const unsigned char* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc ^ (data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

void append_uint16_be(std::vector<unsigned char>& buffer, uint16_t value) {
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void append_int16_be(std::vector<unsigned char>& buffer, int16_t value) {
    append_uint16_be(buffer, static_cast<uint16_t>(value));
}

void append_uint32_be(std::vector<unsigned char>& buffer, uint32_t value) {
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void append_float32_be(std::vector<unsigned char>& buffer, float value) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = value;
    append_uint32_be(buffer, u.i);
}

void append_bytes(std::vector<unsigned char>& buffer, const void* data, size_t length) {
    const unsigned char* byte_data = static_cast<const unsigned char*>(data);
    buffer.insert(buffer.end(), byte_data, byte_data + length);
}

std::vector<unsigned char> create_config_frame2(
    uint16_t pmuId,
    uint32_t timeBase,
    uint16_t numPmu,
    const std::string& stnName,
    uint16_t dataRate,
    uint16_t phnmr,
    uint16_t annmr,
    uint16_t dgnmr,
    bool floatFmt, bool polarFmt)
{
    std::vector<unsigned char> frame;
    frame.reserve(300);

    frame.push_back(SYNC_CFG2);
    frame.push_back(TYPE_CFG2);
    append_uint16_be(frame, 0); // Placeholder for FRAMESIZE
    append_uint16_be(frame, pmuId);
    time_t now_soc = time(NULL);
    append_uint32_be(frame, static_cast<uint32_t>(now_soc));
    append_uint32_be(frame, 0); // FRACSEC

    append_uint32_be(frame, timeBase);
    append_uint16_be(frame, numPmu);

    std::string fixedStnName = stnName;
    fixedStnName.resize(16, ' ');
    append_bytes(frame, fixedStnName.c_str(), 16);
    append_uint16_be(frame, pmuId);

    uint16_t format = 0;
    if (floatFmt) format |= (1 << 0);  // Data in float
    if (polarFmt) format |= (1 << 1);  // Phasors in polar
    if (floatFmt) format |= (1 << 2);  // Phasor format float
    if (floatFmt) format |= (1 << 3);  // Analog format float
    if (floatFmt) format |= (1 << 4);  // Freq/ROCOF float
    append_uint16_be(frame, format);

    append_uint16_be(frame, phnmr);
    append_uint16_be(frame, annmr);
    append_uint16_be(frame, dgnmr);

    for (uint16_t i = 0; i < phnmr; ++i) {
        std::string name = "Phasor " + std::to_string(i + 1);
        name.resize(16, ' ');
        append_bytes(frame, name.c_str(), 16);
    }
    for (uint16_t i = 0; i < annmr; ++i) {
        std::string name = "Analog " + std::to_string(i + 1);
        name.resize(16, ' ');
        append_bytes(frame, name.c_str(), 16);
    }

    for (uint16_t i = 0; i < phnmr; ++i) {
        uint32_t phunit = (i == 0) ? 0x00000001 : 0x01000001; // Volt: 1V, Current: 0.01A
        append_uint32_be(frame, phunit);
    }

    for (uint16_t i = 0; i < annmr; ++i) {
        append_uint32_be(frame, 0x00000064); // 100 units
    }

    uint16_t fnom_code = (dataRate == 60) ? 1 : 0;
    append_uint16_be(frame, fnom_code);
    append_uint16_be(frame, 0); // CFGCNT
    append_uint16_be(frame, dataRate);

    uint16_t frameSize = static_cast<uint16_t>(frame.size() + 2);
    frame[2] = (frameSize >> 8) & 0xFF;
    frame[3] = frameSize & 0xFF;

    uint16_t crc = calculate_crc(frame.data(), frame.size());
    append_uint16_be(frame, crc);

    return frame;
}

float getRandomFloat(float min_val, float max_val) {
    if (RAND_MAX == 0) return min_val;
    float scale = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min_val + scale * (max_val - min_val);
}

std::vector<unsigned char> create_data_frame(
    uint16_t pmuId,
    uint16_t phnmr, uint16_t annmr, uint16_t dgnmr,
    bool floatFmt, bool polarFmt, uint16_t dataRate)
{
    std::vector<unsigned char> frame;
    frame.reserve(128);

    frame.push_back(SYNC_DATA);
    frame.push_back(TYPE_DATA);
    append_uint16_be(frame, 0); // Placeholder for FRAMESIZE
    append_uint16_be(frame, pmuId);

    auto now = std::chrono::system_clock::now();
    auto now_sec = std::chrono::system_clock::to_time_t(now);
    auto duration = now.time_since_epoch();
    auto subsec = std::chrono::duration_cast<std::chrono::microseconds>(duration) % 1000000;

    uint32_t soc = static_cast<uint32_t>(now_sec);
    uint32_t fracsec = static_cast<uint32_t>(subsec.count());
    append_uint32_be(frame, soc);
    append_uint32_be(frame, fracsec);

    uint16_t stat = 0;
    stat |= (1 << 15); // Data valid
    stat |= (1 << 14); // PMU sync
    append_uint16_be(frame, stat);

    float nominal_mag = 230.0; // Nominal voltage magnitude
    for (uint16_t i = 0; i < phnmr; ++i) {
        float mag = nominal_mag + getRandomFloat(-5.0, 5.0);
        float angle_deg = getRandomFloat(-180.0, 180.0);
        float angle_rad = angle_deg * M_PI / 180.0;

        if (floatFmt && polarFmt) {
            append_float32_be(frame, mag);      // Magnitude
            append_float32_be(frame, angle_rad); // Angle in radians
        }
    }

    float freq = (dataRate == 60 ? 60.0f : 50.0f) + getRandomFloat(-0.05f, 0.05f);
    float rocof = getRandomFloat(-0.5f, 0.5f);
    if (floatFmt) {
        append_float32_be(frame, freq);
        append_float32_be(frame, rocof);
    }

    for (uint16_t i = 0; i < annmr; ++i) {
        float analog_val = getRandomFloat(0.0f, 10.0f);
        if (floatFmt) {
            append_float32_be(frame, analog_val);
        }
    }

    uint16_t frameSize = static_cast<uint16_t>(frame.size() + 2);
    frame[2] = (frameSize >> 8) & 0xFF;
    frame[3] = frameSize & 0xFF;

    uint16_t crc = calculate_crc(frame.data(), frame.size());
    append_uint16_be(frame, crc);

    return frame;
}

void processCommandFrame(unsigned char* cmdFrame, int frameSizeRecv, uint16_t localPMUId, uint16_t& command) {
    command = 0;
    std::cout << "[DEBUG] processCommandFrame called with frameSizeRecv = " << frameSizeRecv << std::endl;

    std::cout << "[DEBUG] Received bytes: ";
    for (int i = 0; i < frameSizeRecv; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)cmdFrame[i] << " ";
    }
    std::cout << std::dec << std::endl;

    if (frameSizeRecv < 10 || cmdFrame[0] != SYNC_CMD || cmdFrame[1] != TYPE_CMD) {
        std::cerr << "[PMU] Invalid command frame header.\n";
        return;
    }

    uint16_t frameSize = (static_cast<uint16_t>(cmdFrame[2]) << 8) | cmdFrame[3];
    std::cout << "[DEBUG] Frame size field (bytes 2-3) indicates: " << frameSize << " bytes.\n";
    if (frameSize > frameSizeRecv || frameSize < 10) {
        std::cerr << "[PMU] Invalid frame size.\n";
        return;
    }

    uint16_t expected_crc = (static_cast<uint16_t>(cmdFrame[frameSize - 2]) << 8) | cmdFrame[frameSize - 1];
    uint16_t calculated_crc = calculate_crc(cmdFrame, frameSize - 2);
    std::cout << "[DEBUG] CRC Check: Expected=0x" << std::hex << expected_crc
        << ", Calculated=0x" << calculated_crc << std::dec
        << " (over " << (frameSize - 2) << " bytes)\n";
    bool crc_ok = (expected_crc == calculated_crc);

    uint16_t receivedPMUId = (static_cast<uint16_t>(cmdFrame[4]) << 8) | cmdFrame[5];
    std::cout << "[DEBUG] Received PMU ID: " << receivedPMUId << "\n";
    if (receivedPMUId != localPMUId && receivedPMUId != 0xFFFF) {
        std::cerr << "[PMU] PMU ID mismatch.\n";
        return;
    }

    command = (static_cast<uint16_t>(cmdFrame[6]) << 8) | cmdFrame[7];
    std::cout << "[DEBUG] Command Code: 0x" << std::hex << command << std::dec << "\n";

    switch (command) {
    case CMD_TURN_OFF_TX:
        std::cout << "[PMU] Turn Off Data Transmission.\n";
        break;
    case CMD_TURN_ON_TX:
        std::cout << "[PMU] Turn On Data Transmission.\n";
        break;
    case CMD_SEND_HDR:
        std::cout << "[PMU] Send Header Frame.\n";
        break;
    case CMD_SEND_CFG1:
        std::cout << "[PMU] Send CFG-1 Frame (using CFG-2).\n";
        break;
    case CMD_SEND_CFG2:
        /* case 0x67F2:
             std::cout << "[PMU] Send CFG-2 Frame (0x67F2 handled).\n";
             command = CMD_SEND_CFG2;
             break;*/
    default:
        std::cout << "[PMU] Send CFG-2 Frame (0x67F2 handled).\n";
        command = CMD_SEND_CFG2;
        break;
    }
}

int main(int argc, char* argv[]) {
    uint16_t listenPort = DEFAULT_PORT;
    uint16_t pmuIdCode = DEFAULT_PMU_ID;
    std::string stationName = normalizeStationName("", pmuIdCode);

    if (argc >= 2) {
        const int parsedPort = std::atoi(argv[1]);
        if (parsedPort <= 0 || parsedPort > 65535) {
            std::cerr << "[PMU] Invalid port: " << argv[1] << "\n";
            printUsage(argv[0]);
            return 1;
        }
        listenPort = static_cast<uint16_t>(parsedPort);
    }

    if (argc >= 3) {
        const int parsedId = std::atoi(argv[2]);
        if (parsedId <= 0 || parsedId > 65535) {
            std::cerr << "[PMU] Invalid PMU ID: " << argv[2] << "\n";
            printUsage(argv[0]);
            return 1;
        }
        pmuIdCode = static_cast<uint16_t>(parsedId);
    }

    if (argc >= 4) {
        stationName = normalizeStationName(argv[3], pmuIdCode);
    } else {
        stationName = normalizeStationName("", pmuIdCode);
    }

    WSADATA wsaData;
    SOCKET serverSocket = INVALID_SOCKET;
    SOCKET clientSocket = INVALID_SOCKET;
    sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[PMU] WSAStartup failed! Error: " << WSAGetLastError() << "\n";
        return 1;
    }
    std::cout << "[PMU] Winsock Initialized.\n";

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[PMU] Socket creation failed! Error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }
    std::cout << "[PMU] Server socket created.\n";
    std::cout << "[PMU] Configuration: port=" << listenPort
              << ", pmuId=" << pmuIdCode
              << ", stationName='" << stationName << "'\n";

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(listenPort);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[PMU] Bind failed! Error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "[PMU] Socket bound to port " << listenPort << ".\n";

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[PMU] Listen failed! Error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "[PMU] Listening for incoming connections...\n";

    srand(static_cast<unsigned int>(time(nullptr)));

    while (true) {
        clientAddrSize = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "[PMU] Accept failed! Error: " << WSAGetLastError() << "\n";
            break;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "[PMU] Client connected: " << clientIP << ":" << ntohs(clientAddr.sin_port) << "\n";

        unsigned char recvBuffer[2048];
        bool dataStreamActive = false;
        auto lastFrameTime = std::chrono::steady_clock::now();
        std::chrono::milliseconds frameInterval(1000 / DATA_RATE);

        while (true) {
            u_long bytesAvailable = 0;
            if (ioctlsocket(clientSocket, FIONREAD, &bytesAvailable) == SOCKET_ERROR) {
                std::cerr << "[PMU] ioctlsocket failed! Error: " << WSAGetLastError() << "\n";
                break;
            }

            if (bytesAvailable > 0) {
                int bytesReceived = recv(clientSocket, (char*)recvBuffer, sizeof(recvBuffer), 0);
                if (bytesReceived <= 0) {
                    std::cout << "[PMU] Client disconnected. Waiting for the next PDC connection...\n";
                    break;
                }

                std::cout << "[PMU] Received " << bytesReceived << " bytes: ";
                for (int i = 0; i < bytesReceived; ++i) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)recvBuffer[i] << " ";
                }
                std::cout << std::dec << "\n";

                uint16_t command = 0;
                processCommandFrame(recvBuffer, bytesReceived, pmuIdCode, command);

                switch (command) {
                case CMD_SEND_CFG1:
                case CMD_SEND_CFG2:
                case 0x67F2:
                {
                    std::cout << "[PMU] Sending CFG-2 frame...\n";
                    std::vector<unsigned char> cfgFrame = create_config_frame2(
                        pmuIdCode, 1000000, 1, stationName, DATA_RATE,
                        PHASOR_COUNT, ANALOG_COUNT, DIGITAL_COUNT,
                        USE_FLOAT_FORMAT, USE_POLAR_FORMAT);

                    int bytesSent = send(clientSocket, (char*)cfgFrame.data(), cfgFrame.size(), 0);
                    if (bytesSent == SOCKET_ERROR) {
                        std::cerr << "[PMU] Send CFG-2 failed! Error: " << WSAGetLastError() << "\n";
                        break;
                    }
                    std::cout << "[PMU] CFG-2 sent (" << bytesSent << " bytes).\n";

                    dataStreamActive = true;
                    lastFrameTime = std::chrono::steady_clock::now();
                    std::cout << "[PMU] Data stream enabled for testing.\n";
                }
                break;

                case CMD_TURN_ON_TX:
                    dataStreamActive = true;
                    lastFrameTime = std::chrono::steady_clock::now();
                    std::cout << "[PMU] Data stream enabled.\n";
                    break;

                case CMD_TURN_OFF_TX:
                    dataStreamActive = false;
                    std::cout << "[PMU] Data stream disabled.\n";
                    break;

                default:
                    std::cout << "[PMU] Sending CFG-2 frame...\n";
                    std::vector<unsigned char> cfgFrame = create_config_frame2(
                        pmuIdCode, 1000000, 1, stationName, DATA_RATE,
                        PHASOR_COUNT, ANALOG_COUNT, DIGITAL_COUNT,
                        USE_FLOAT_FORMAT, USE_POLAR_FORMAT);

                    int bytesSent = send(clientSocket, (char*)cfgFrame.data(), cfgFrame.size(), 0);
                    if (bytesSent == SOCKET_ERROR) {
                        std::cerr << "[PMU] Send CFG-2 failed! Error: " << WSAGetLastError() << "\n";
                        break;
                    }
                    std::cout << "[PMU] CFG-2 sent (" << bytesSent << " bytes).\n";

                    dataStreamActive = true;
                    lastFrameTime = std::chrono::steady_clock::now();
                    std::cout << "[PMU] Data stream enabled for testing.\n";
                    break;
                }
            }

            if (dataStreamActive) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime);
                if (elapsed >= frameInterval) {
                    lastFrameTime = now;
                    std::vector<unsigned char> dataFrame = create_data_frame(
                        pmuIdCode, PHASOR_COUNT, ANALOG_COUNT, DIGITAL_COUNT,
                        USE_FLOAT_FORMAT, USE_POLAR_FORMAT, DATA_RATE);

                    int bytesSent = send(clientSocket, (char*)dataFrame.data(), dataFrame.size(), 0);
                    if (bytesSent == SOCKET_ERROR) {
                        std::cerr << "[PMU] Send Data failed! Error: " << WSAGetLastError() << "\n";
                        break;
                    }
                    std::cout << "[PMU] Data frame sent (" << bytesSent << " bytes).\n";
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if (clientSocket != INVALID_SOCKET) {
            shutdown(clientSocket, SD_SEND);
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }

    std::cout << "[PMU] Shutting down...\n";
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    WSACleanup();
    std::cout << "[PMU] Cleanup complete.\n";

    return 0;
}
