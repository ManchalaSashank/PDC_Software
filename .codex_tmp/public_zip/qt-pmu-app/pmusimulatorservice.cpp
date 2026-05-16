#include "pmusimulatorservice.h"

#include <QDataStream>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QtMath>

namespace {
constexpr quint8 TYPE_DATA = 0x01;
constexpr quint8 TYPE_CFG2 = 0x31;
constexpr quint8 TYPE_CMD = 0x41;
constexpr quint16 CMD_TURN_OFF_TX = 0x0001;
constexpr quint16 CMD_TURN_ON_TX = 0x0002;
constexpr quint16 CMD_SEND_CFG2 = 0x0005;

const QStringList kPhasorChannels = { "VA", "VB", "VC", "IA", "IB", "IC" };

void appendUInt16(QByteArray& buffer, quint16 value)
{
    QDataStream stream(&buffer, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << value;
}

void appendUInt32(QByteArray& buffer, quint32 value)
{
    QDataStream stream(&buffer, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << value;
}

void appendFloat(QByteArray& buffer, float value)
{
    QDataStream stream(&buffer, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << value;
}

void appendText(QByteArray& buffer, const QString& text, int width)
{
    QByteArray padded(width, ' ');
    const QByteArray ascii = text.left(width).toLatin1();
    for (int i = 0; i < ascii.size() && i < width; ++i) {
        padded[i] = ascii[i];
    }
    buffer.append(padded);
}
}

PmuSimulatorService::PmuSimulatorService(QObject* parent)
    : QObject(parent)
{
    seedProfiles();
}

QList<PmuProfile> PmuSimulatorService::profiles() const
{
    QList<PmuProfile> result;
    for (const Entry& entry : m_entries) {
        result.append(entry.profile);
    }
    return result;
}

PmuProfile PmuSimulatorService::profile(const QString& id) const
{
    const Entry* entry = findEntry(id);
    return entry ? entry->profile : PmuProfile{};
}

PmuRuntimeStatus PmuSimulatorService::status(const QString& id) const
{
    const Entry* entry = findEntry(id);
    return entry ? entry->status : PmuRuntimeStatus{};
}

void PmuSimulatorService::updateProfile(const QString& id, const PmuProfile& profile)
{
    Entry* entry = findEntry(id);
    if (!entry) {
        return;
    }

    entry->profile = profile;
    entry->profile.acceptedPdcIp = normalizeIp(entry->profile.acceptedPdcIp);
    entry->status.lastConfigUpdatedAt = QDateTime::currentDateTimeUtc();
    entry->status.message = QString("Updated %1 configuration.").arg(entry->profile.name);
    entry->status.lastError.clear();

    if (entry->transmitting) {
        startTransmission(*entry);
    }

    disconnectIfUnauthorized(*entry);

    if (entry->socket && entry->socket->state() == QAbstractSocket::ConnectedState) {
        entry->socket->write(buildCfg2Frame(*entry));
    }

    emit profilesChanged();
    emit statusChanged(id);
}

bool PmuSimulatorService::startProfile(const QString& id, QString* errorMessage)
{
    Entry* entry = findEntry(id);
    if (!entry) {
        if (errorMessage) {
            *errorMessage = "PMU profile not found.";
        }
        return false;
    }

    if (entry->server && entry->server->isListening()) {
        entry->status.serverState = "running";
        entry->status.message = QString("PMU already listening on port %1.").arg(entry->profile.port);
        emit statusChanged(id);
        return true;
    }

    if (!entry->server) {
        entry->server = new QTcpServer(this);
        configureSignals(*entry);
    }

    if (!entry->server->listen(QHostAddress::Any, entry->profile.port)) {
        entry->status.serverState = "stopped";
        entry->status.lastError = entry->server->errorString();
        entry->status.message = QString("Unable to start PMU: %1").arg(entry->server->errorString());
        if (errorMessage) {
            *errorMessage = entry->status.message;
        }
        emit statusChanged(id);
        return false;
    }

    entry->status.serverState = "running";
    entry->status.message = QString("Listening for PDC %1 on port %2.")
                                .arg(entry->profile.acceptedPdcIp, QString::number(entry->profile.port));
    entry->status.lastError.clear();
    emit statusChanged(id);
    return true;
}

void PmuSimulatorService::stopProfile(const QString& id)
{
    Entry* entry = findEntry(id);
    if (!entry) {
        return;
    }

    disconnectSocket(*entry, "PMU simulator stopped.");
    if (entry->server) {
        entry->server->close();
    }
    entry->status.serverState = "stopped";
    entry->status.message = "PMU simulator stopped.";
    entry->status.lastError.clear();
    emit statusChanged(id);
}

PmuSimulatorService::Entry* PmuSimulatorService::findEntry(const QString& id)
{
    for (Entry& entry : m_entries) {
        if (entry.profile.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

const PmuSimulatorService::Entry* PmuSimulatorService::findEntry(const QString& id) const
{
    for (const Entry& entry : m_entries) {
        if (entry.profile.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

void PmuSimulatorService::seedProfiles()
{
    const QList<PmuProfile> defaults = {
        { "pmu-1", "Substation A", "SUB_A", 1, 4712, 50, "127.0.0.1", { "VA", "VB", "VC", "IA", "IB", "IC", "F", "DFDT" } },
        { "pmu-2", "Substation B", "SUB_B", 2, 4713, 25, "127.0.0.1", { "VA", "VB", "VC", "F" } },
        { "pmu-3", "Feeder C", "FEEDER_C", 3, 4714, 10, "127.0.0.1", { "IA", "IB", "IC", "DFDT" } }
    };

    for (const PmuProfile& profile : defaults) {
        Entry entry;
        entry.profile = profile;
        entry.status.serverState = "stopped";
        entry.status.transmissionState = "idle";
        entry.status.deniedRequests = 0;
        entry.status.framesSent = 0;
        entry.status.message = "Ready to start.";
        entry.status.lastConfigUpdatedAt = QDateTime::currentDateTimeUtc();
        entry.transmitTimer = new QTimer(this);
        entry.transmitTimer->setSingleShot(false);
        m_entries.append(entry);
    }
}

void PmuSimulatorService::configureSignals(Entry& entry)
{
    connect(entry.server, &QTcpServer::newConnection, this, [this, &entry]() {
        while (entry.server->hasPendingConnections()) {
            QTcpSocket* socket = entry.server->nextPendingConnection();
            const QString remoteIp = normalizeIp(socket->peerAddress().toString());

            if (remoteIp != entry.profile.acceptedPdcIp) {
                entry.status.deniedRequests += 1;
                entry.status.lastError = QString("Denied connection from %1; only %2 is accepted.")
                                             .arg(remoteIp, entry.profile.acceptedPdcIp);
                entry.status.message = entry.status.lastError;
                emit statusChanged(entry.profile.id);
                socket->disconnectFromHost();
                socket->deleteLater();
                continue;
            }

            if (entry.socket && entry.socket->state() == QAbstractSocket::ConnectedState) {
                entry.status.deniedRequests += 1;
                entry.status.lastError = QString("Denied extra connection from %1; one PDC at a time is allowed.")
                                             .arg(remoteIp);
                entry.status.message = entry.status.lastError;
                emit statusChanged(entry.profile.id);
                socket->disconnectFromHost();
                socket->deleteLater();
                continue;
            }

            entry.socket = socket;
            entry.receiveBuffer.clear();
            entry.status.connectedPdcIp = remoteIp;
            entry.status.lastClientAt = QDateTime::currentDateTimeUtc();
            entry.status.message = QString("PDC %1 connected. Waiting for command.").arg(remoteIp);
            entry.status.lastError.clear();
            emit statusChanged(entry.profile.id);

            connect(socket, &QTcpSocket::readyRead, this, [this, &entry]() {
                if (!entry.socket) {
                    return;
                }
                processIncomingData(entry, entry.socket->readAll());
            });

            connect(socket, &QTcpSocket::disconnected, this, [this, &entry]() {
                disconnectSocket(entry, "PDC disconnected from PMU simulator.");
            });

            connect(socket, &QTcpSocket::errorOccurred, this, [this, &entry](QAbstractSocket::SocketError) {
                if (!entry.socket) {
                    return;
                }
                entry.status.lastError = entry.socket->errorString();
                disconnectSocket(entry, QString("PDC socket error: %1").arg(entry.socket->errorString()), true);
            });
        }
    });
}

void PmuSimulatorService::startTransmission(Entry& entry)
{
    stopTransmission(entry);
    if (!entry.socket || entry.socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    const int intervalMs = qMax(20, qRound(1000.0 / qMax(1, entry.profile.dataRate)));
    connect(entry.transmitTimer, &QTimer::timeout, this, [this, &entry]() {
        if (!entry.socket || entry.socket->state() != QAbstractSocket::ConnectedState) {
            stopTransmission(entry);
            entry.status.transmissionState = "idle";
            emit statusChanged(entry.profile.id);
            return;
        }

        entry.socket->write(buildDataFrame(entry));
        entry.status.framesSent += 1;
        entry.status.transmissionState = "streaming";
        entry.status.message = QString("Streaming at %1 fps to %2.")
                                   .arg(QString::number(entry.profile.dataRate), entry.status.connectedPdcIp);
        emit statusChanged(entry.profile.id);
    }, Qt::UniqueConnection);
    entry.transmitTimer->start(intervalMs);
}

void PmuSimulatorService::stopTransmission(Entry& entry)
{
    entry.transmitting = false;
    if (entry.transmitTimer) {
        entry.transmitTimer->stop();
    }
}

void PmuSimulatorService::disconnectSocket(Entry& entry, const QString& message, bool keepError)
{
    stopTransmission(entry);
    if (entry.socket) {
        entry.socket->blockSignals(true);
        entry.socket->disconnectFromHost();
        entry.socket->deleteLater();
        entry.socket = nullptr;
    }
    entry.receiveBuffer.clear();
    entry.status.connectedPdcIp.clear();
    entry.status.transmissionState = "idle";
    entry.status.message = message;
    if (!keepError) {
        entry.status.lastError.clear();
    }
    emit statusChanged(entry.profile.id);
}

void PmuSimulatorService::disconnectIfUnauthorized(Entry& entry)
{
    if (!entry.socket) {
        return;
    }

    const QString remoteIp = normalizeIp(entry.socket->peerAddress().toString());
    if (!remoteIp.isEmpty() && remoteIp != entry.profile.acceptedPdcIp) {
        disconnectSocket(entry, QString("Disconnected %1 because accepted PDC IP changed to %2.")
                                    .arg(remoteIp, entry.profile.acceptedPdcIp));
    }
}

void PmuSimulatorService::processIncomingData(Entry& entry, const QByteArray& chunk)
{
    entry.receiveBuffer.append(chunk);

    while (entry.receiveBuffer.size() >= 4) {
        if (static_cast<quint8>(entry.receiveBuffer[0]) != 0xaa) {
            entry.receiveBuffer.remove(0, 1);
            continue;
        }

        const quint16 frameSize =
            (static_cast<quint8>(entry.receiveBuffer[2]) << 8) |
            static_cast<quint8>(entry.receiveBuffer[3]);

        if (frameSize < 10 || frameSize > 2048) {
            entry.receiveBuffer.remove(0, 1);
            continue;
        }

        if (entry.receiveBuffer.size() < frameSize) {
            return;
        }

        const QByteArray frame = entry.receiveBuffer.left(frameSize);
        entry.receiveBuffer.remove(0, frameSize);
        handleCommandFrame(entry, frame);
    }
}

void PmuSimulatorService::handleCommandFrame(Entry& entry, const QByteArray& frame)
{
    if (static_cast<quint8>(frame[1]) != TYPE_CMD) {
        return;
    }

    const quint16 expected = (static_cast<quint8>(frame[frame.size() - 2]) << 8) |
                             static_cast<quint8>(frame[frame.size() - 1]);
    if (crc16(frame.left(frame.size() - 2)) != expected) {
        entry.status.lastError = "Rejected command with invalid CRC.";
        entry.status.message = entry.status.lastError;
        emit statusChanged(entry.profile.id);
        return;
    }

    const quint16 targetPmuId = (static_cast<quint8>(frame[4]) << 8) | static_cast<quint8>(frame[5]);
    if (targetPmuId != entry.profile.pmuId && targetPmuId != 0xffff) {
        return;
    }

    quint16 commandCode = 0;
    if (frame.size() >= 16) {
        commandCode = (static_cast<quint8>(frame[14]) << 8) | static_cast<quint8>(frame[15]);
    }

    if (commandCode == CMD_SEND_CFG2) {
        entry.socket->write(buildCfg2Frame(entry));
        entry.status.message = "CFG2 sent to connected PDC.";
        entry.status.lastError.clear();
        emit statusChanged(entry.profile.id);
        return;
    }

    if (commandCode == CMD_TURN_ON_TX) {
        entry.transmitting = true;
        entry.status.transmissionState = "starting";
        entry.status.message = "Transmission enabled by connected PDC.";
        startTransmission(entry);
        emit statusChanged(entry.profile.id);
        return;
    }

    if (commandCode == CMD_TURN_OFF_TX) {
        stopTransmission(entry);
        entry.status.transmissionState = "idle";
        entry.status.message = "Transmission disabled by connected PDC.";
        emit statusChanged(entry.profile.id);
    }
}

QByteArray PmuSimulatorService::buildCfg2Frame(const Entry& entry) const
{
    const QStringList phasorLabels = entry.profile.selectedChannels.filter(QRegularExpression("^(VA|VB|VC|IA|IB|IC)$"));
    QByteArray frame;
    frame.append(char(0xaa));
    frame.append(char(TYPE_CFG2));
    appendUInt16(frame, 0);
    appendUInt16(frame, entry.profile.pmuId);
    appendUInt32(frame, QDateTime::currentSecsSinceEpoch());
    appendUInt32(frame, 0);
    appendUInt32(frame, 1000000);
    appendUInt16(frame, 1);
    appendText(frame, entry.profile.stationName, 16);
    appendUInt16(frame, entry.profile.pmuId);
    appendUInt16(frame, 0x001f);
    appendUInt16(frame, phasorLabels.size());
    appendUInt16(frame, 0);
    appendUInt16(frame, 0);
    for (const QString& label : phasorLabels) {
        appendText(frame, label, 16);
    }
    for (const QString& label : phasorLabels) {
        appendUInt32(frame, label.startsWith('I') ? 0x01000001 : 0x00000001);
    }
    appendUInt16(frame, entry.profile.dataRate == 60 ? 1 : 0);
    appendUInt16(frame, 1);
    appendUInt16(frame, static_cast<quint16>(entry.profile.dataRate));
    frame[2] = char((frame.size() + 2) >> 8);
    frame[3] = char((frame.size() + 2) & 0xff);
    appendUInt16(frame, crc16(frame));
    return frame;
}

QByteArray PmuSimulatorService::buildDataFrame(const Entry& entry) const
{
    const QStringList channels = entry.profile.selectedChannels;
    QByteArray frame;
    frame.append(char(0xaa));
    frame.append(char(TYPE_DATA));
    appendUInt16(frame, 0);
    appendUInt16(frame, entry.profile.pmuId);
    appendUInt32(frame, QDateTime::currentSecsSinceEpoch());
    appendUInt32(frame, 0);
    appendUInt16(frame, 0xc000);

    auto randomRange = [](double min, double max) {
        return min + QRandomGenerator::global()->generateDouble() * (max - min);
    };

    for (const QString& channel : channels) {
        if (!kPhasorChannels.contains(channel)) {
            continue;
        }

        double magnitudeBase = channel.startsWith('V') ? 230.0 : 12.0;
        double magnitudeSwing = channel.startsWith('V') ? 4.0 : 0.8;
        double angleDeg = 0.0;
        if (channel == "VB" || channel == "IB") {
            angleDeg = -120.0;
        } else if (channel == "VC" || channel == "IC") {
            angleDeg = 120.0;
        } else if (channel == "IA") {
            angleDeg = -8.0;
        }

        appendFloat(frame, static_cast<float>(magnitudeBase + randomRange(-magnitudeSwing, magnitudeSwing)));
        appendFloat(frame, static_cast<float>(qDegreesToRadians(angleDeg + randomRange(-2.0, 2.0))));
    }

    const float nominalFrequency = entry.profile.dataRate == 60 ? 60.0f : 50.0f;
    appendFloat(frame, channels.contains("F") ? nominalFrequency + static_cast<float>(randomRange(-0.04, 0.04)) : 0.0f);
    appendFloat(frame, channels.contains("DFDT") ? static_cast<float>(randomRange(-0.2, 0.2)) : 0.0f);
    frame[2] = char((frame.size() + 2) >> 8);
    frame[3] = char((frame.size() + 2) & 0xff);
    appendUInt16(frame, crc16(frame));
    return frame;
}

QString PmuSimulatorService::normalizeIp(const QString& value) const
{
    QString ip = value.trimmed().toLower();
    if (ip == "localhost" || ip == "::1") {
        return "127.0.0.1";
    }
    if (ip.startsWith("::ffff:")) {
        ip.remove(0, 7);
    }
    return ip;
}

quint16 PmuSimulatorService::crc16(const QByteArray& buffer) const
{
    quint16 crc = 0xffff;
    for (char rawByte : buffer) {
        crc ^= (static_cast<quint8>(rawByte) << 8);
        for (int i = 0; i < 8; ++i) {
            crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
        }
    }
    return crc;
}
