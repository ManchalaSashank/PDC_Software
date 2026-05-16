#include "pmu_worker.h"

#include "command_builder.h"
#include "connection.h"
#include "data_parser.h"
#include <QDateTime>
#include <vector>

#define CMD_SEND_CFG2 0x0005
#define CMD_TURN_ON_TX 0x0002
#define BUFFER_SIZE 4096

namespace {
bool receiveCompleteFrame(SOCKET sock, std::vector<unsigned char>& frame)
{
    unsigned char buffer[BUFFER_SIZE];
    frame.clear();

    while (true)
    {
        int bytes = receiveBytes(sock, buffer, BUFFER_SIZE);
        if (bytes <= 0)
            return false;

        frame.insert(frame.end(), buffer, buffer + bytes);

        if (frame.size() >= 4)
        {
            uint16_t frameSize =
                (static_cast<uint16_t>(frame[2]) << 8) |
                static_cast<uint16_t>(frame[3]);

            if (frameSize < 4 || frameSize > BUFFER_SIZE)
                return false;

            if (frame.size() >= frameSize)
            {
                frame.resize(frameSize);
                return true;
            }
        }
    }
}
}

PMUWorker::PMUWorker(QString ip, int port, int id)
    : ip(ip), port(port), pmuId(id)
{
    qRegisterMetaType<PMUConfig>("PMUConfig");
    qRegisterMetaType<PMUFrame>("PMUFrame");
}

DataManager& PMUWorker::getDataManager()
{
    return dataManager;
}

PMUConfig PMUWorker::getConfig() const
{
    return config;
}

void PMUWorker::setActiveSocket(SOCKET sock)
{
    std::lock_guard<std::mutex> lock(socketMutex);
    activeSocket = sock;
}

void PMUWorker::clearActiveSocket()
{
    std::lock_guard<std::mutex> lock(socketMutex);
    activeSocket = INVALID_SOCKET;
}

bool PMUWorker::closeActiveSocket(SOCKET sock)
{
    std::lock_guard<std::mutex> lock(socketMutex);
    if (activeSocket == sock && activeSocket != INVALID_SOCKET)
    {
        closesocket(activeSocket);
        activeSocket = INVALID_SOCKET;
        return true;
    }

    return false;
}

void PMUWorker::start()
{
    running = true;
    emit statusUpdate("Connecting...");

    if (!initializeWinsock())
    {
        emit statusUpdate("Winsock failed");
        emit finished();
        return;
    }

    SOCKET sock;
    if (!connectToPMU(ip.toStdString().c_str(), port, sock))
    {
        emit statusUpdate("Connection failed");
        cleanupWinsock();
        emit finished();
        return;
    }
    setActiveSocket(sock);

    auto cfgCmd = build_command_frame(pmuId, CMD_SEND_CFG2);
    sendBytes(sock, cfgCmd.data(), cfgCmd.size());

    std::vector<unsigned char> cfgFrame;
    if (!receiveCompleteFrame(sock, cfgFrame))
    {
        emit statusUpdate("Failed to receive CFG");
        closeActiveSocket(sock);
        cleanupWinsock();
        emit finished();
        return;
    }

    if (!parseConfigFrame(cfgFrame.data(), static_cast<int>(cfgFrame.size()), config))
    {
        emit statusUpdate("Failed to parse CFG");
        closeActiveSocket(sock);
        cleanupWinsock();
        emit finished();
        return;
    }

    if (config.pmuID != static_cast<uint16_t>(pmuId))
    {
        emit statusUpdate(QString("Device ID mismatch: expected %1, got %2").arg(pmuId).arg(config.pmuID));
        closeActiveSocket(sock);
        cleanupWinsock();
        emit finished();
        return;
    }

    emit configUpdate(config);
    emit statusUpdate("Connected");

    auto startCmd = build_command_frame(pmuId, CMD_TURN_ON_TX);
    sendBytes(sock, startCmd.data(), startCmd.size());

    unsigned char buffer[BUFFER_SIZE];
    std::vector<unsigned char> tcpBuffer;

    while (running)
    {
        PMUFrame frame;
        int bytes = receiveBytes(sock, buffer, BUFFER_SIZE);
        if (bytes <= 0) break;

        tcpBuffer.insert(tcpBuffer.end(), buffer, buffer + bytes);

        while (tcpBuffer.size() >= 4)
        {
            uint16_t frameSize =
                (static_cast<uint16_t>(tcpBuffer[2]) << 8) |
                static_cast<uint16_t>(tcpBuffer[3]);

            if (tcpBuffer.size() < frameSize)
                break;

            if (parseDataFrame(tcpBuffer.data(), frameSize, config, frame))
            {
                frame.systemUnixMs = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
                frame.latencyMs = static_cast<double>(frame.systemUnixMs) - (frame.pmuTimestampSeconds * 1000.0);

                dataManager.addFrame(frame);

                emit newData(frame.frequency);
                emit newFrame(frame);
            }

            tcpBuffer.erase(tcpBuffer.begin(),
                            tcpBuffer.begin() + frameSize);
        }
    }

    emit statusUpdate("Disconnected");
    closeActiveSocket(sock);
    cleanupWinsock();
    emit finished();
}

void PMUWorker::stop()
{
    running = false;
    std::lock_guard<std::mutex> lock(socketMutex);
    if (activeSocket != INVALID_SOCKET)
    {
        shutdown(activeSocket, SD_BOTH);
        closesocket(activeSocket);
        activeSocket = INVALID_SOCKET;
    }
}
