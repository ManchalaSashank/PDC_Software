#include <iostream>
#include <winsock2.h>
#include <iomanip>
#include <cstdint>
#include "connection.h"
#include "command_builder.h"
#include "data_parser.h"

#pragma comment(lib, "ws2_32.lib")

#define CMD_SEND_CFG2 0x0005
#define CMD_TURN_ON_TX 0x0002

#define PMU_IP "127.0.0.1"
#define PMU_PORT 4712
#define BUFFER_SIZE 4096
#define PMU_ID 1

using namespace std;

void printHex(unsigned char* data, int length)
{
    for (int i = 0; i < length; i++)
    {
        cout << hex << setw(2) << setfill('0')
             << static_cast<int>(data[i]) << " ";
    }
    cout << dec << endl;
}


uint16_t phasorCount = 0;
uint16_t analogCount = 0;

int main()
{

    cout << "=========== PDC STARTING ===========\n";

    // 1?? Initialize Winsock
    if (!initializeWinsock())
    {
        cout << "[PDC] Winsock initialization failed\n";
        return 1;
    }

    SOCKET sock;

    // 2?? Connect to PMU
    if (!connectToPMU(PMU_IP, PMU_PORT, sock))
    {
        cout << "[PDC] Connection to PMU failed\n";
        closeConnection(sock);
        return 1;
    }

    cout << "[PDC] Connected to PMU\n";

    // 3?? Send CFG2 Request
    auto cfgCmd = build_command_frame(PMU_ID, CMD_SEND_CFG2);

    if (!sendBytes(sock, cfgCmd.data(), cfgCmd.size()))
    {
        cout << "[PDC] Failed to send CFG2 command\n";
        closeConnection(sock);
        return 1;
    }

    cout << "[PDC] CMD_SEND_CFG2 sent\n";

    // 4?? Wait for CFG2 frame
    unsigned char buffer[BUFFER_SIZE];
    PMUFrame latestFrame;
    int bytesReceived = receiveBytes(sock, buffer, BUFFER_SIZE);

    if (bytesReceived <= 0)
    {
        cout << "[PDC] Failed to receive CFG2 frame\n";
        closeConnection(sock);
        return 1;
    }

    cout << "[PDC] CFG2 frame received (" << bytesReceived << " bytes)\n";

    if(buffer[0] != 0xAA || buffer[1] != 0x31)
{
    cout << "[PDC] Not a valid CFG2 frame\n";
    closeConnection(sock);
    return 1;
}

    cout << "[PDC] CFG2 HEX DATA:\n";
    printHex(buffer, bytesReceived);

    {
        const size_t len = static_cast<size_t>(bytesReceived);
        size_t pos = 0;

        auto canRead = [&](size_t n) { return pos + n <= len; };
        auto readU16 = [&]() -> uint16_t {
            const uint16_t v =
                (static_cast<uint16_t>(buffer[pos]) << 8) |
                static_cast<uint16_t>(buffer[pos + 1]);
            pos += 2;
            return v;
        };
        auto readU32 = [&]() -> uint32_t {
            const uint32_t v =
                (static_cast<uint32_t>(buffer[pos]) << 24) |
                (static_cast<uint32_t>(buffer[pos + 1]) << 16) |
                (static_cast<uint32_t>(buffer[pos + 2]) << 8) |
                static_cast<uint32_t>(buffer[pos + 3]);
            pos += 4;
            return v;
        };

        if (!canRead(18))
        {
            cout << "[PDC] CFG2 parse failed: frame too short\n";
        }
        else
        {
            const uint16_t sync = readU16();
            const uint16_t frameSize = readU16();
            const uint16_t idCode = readU16();
            const uint32_t soc = readU32();
            const uint32_t fracsec = readU32();
            const uint32_t timeBase = readU32();
            const uint16_t numPmu = readU16();

            cout << "[PDC] CFG2 parsed header:\n";
            cout << "  SYNC=0x" << hex << setw(4) << setfill('0') << sync
                 << dec << ", FRAMESIZE=" << frameSize
                 << ", IDCODE=" << idCode
                 << ", SOC=" << soc
                 << ", FRACSEC=" << fracsec
                 << ", TIME_BASE=" << timeBase
                 << ", NUM_PMU=" << numPmu << "\n";

            if (numPmu > 0)
            {
                if (!canRead(26))
                {
                    cout << "[PDC] CFG2 parse warning: PMU block truncated\n";
                }
                else
                {
                    char stationName[17] = {0};
                    for (int i = 0; i < 16; ++i)
                    {
                        stationName[i] = static_cast<char>(buffer[pos + i]);
                    }
                    pos += 16;

                    const uint16_t pmuIdCode = readU16();
                    const uint16_t format = readU16();

                    const uint16_t phnmr = readU16();
                    const uint16_t annmr = readU16();
                    const uint16_t dgnmr = readU16();

                    phasorCount = phnmr;
                    analogCount = annmr;

                    cout << "[PDC] CFG2 first PMU:\n";
                    cout << "  STN='" << stationName << "'"
                         << ", ID=" << pmuIdCode
                         << ", FORMAT=0x" << hex << setw(4) << setfill('0') << format
                         << dec << ", PHNMR=" << phasorCount
                         << ", ANNMR=" << analogCount
                         << ", DGNMR=" << dgnmr << "\n";
                }
            }
        }
    }



    // 5?? Send START DATA TRANSMISSION Command
    auto startCmd = build_command_frame(PMU_ID, CMD_TURN_ON_TX);

    if (!sendBytes(sock, startCmd.data(), startCmd.size()))
    {
        cout << "[PDC] Failed to send TURN_ON_TX\n";
        closeConnection(sock);
        return 1;
    }

    cout << "[PDC] CMD_TURN_ON_TX sent\n";
    cout << "========== STARTING DATA STREAM ==========\n";

    // 6?? Receive Continuous Data Frames
    while (true)
{
    int bytes = receiveBytes(sock, buffer, BUFFER_SIZE);
    cout << "Received packet size: " << bytes << endl;

    if (bytes > 0)
    {
        if(parseDataFrame(buffer,
                          bytes,
                          phasorCount,
                          analogCount,
                          latestFrame))
        {
            cout << "\n====== LIVE PMU DATA ======\n";

            cout << "PMU ID: " << latestFrame.pmuID << endl;
            cout << "Frequency: " << latestFrame.frequency << " Hz\n";
            cout << "ROCOF: " << latestFrame.rocof << endl;

            for(int i = 0; i < latestFrame.phasors.size(); i++)
            {
            
                cout << "Phasor " << i+1
                     << " Mag=" << latestFrame.phasors[i].magnitude
                     << " Angle=" << latestFrame.phasors[i].angleDeg
                     << endl;
            }

            for(int i = 0; i < latestFrame.analogs.size(); i++)
            {
                cout << "Analog " << i+1
                     << " = "
                     << latestFrame.analogs[i]
                     << endl;
            }

            cout << "===========================\n";
        }
    }
    else
        break;
}

    // 7?? Cleanup
    closeConnection(sock);

    cout << "=========== PDC SHUTDOWN ===========\n";
    return 0;
}
