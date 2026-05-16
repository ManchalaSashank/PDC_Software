#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

struct PmuProfile {
    QString id;
    QString name;
    QString stationName;
    quint16 pmuId;
    quint16 port;
    int dataRate;
    QString acceptedPdcIp;
    QStringList selectedChannels;
};

struct PmuRuntimeStatus {
    QString serverState;
    QString transmissionState;
    QString connectedPdcIp;
    int deniedRequests;
    quint64 framesSent;
    QString message;
    QString lastError;
    QDateTime lastClientAt;
    QDateTime lastConfigUpdatedAt;
};

class PmuSimulatorService : public QObject {
    Q_OBJECT

public:
    explicit PmuSimulatorService(QObject* parent = nullptr);

    QList<PmuProfile> profiles() const;
    PmuProfile profile(const QString& id) const;
    PmuRuntimeStatus status(const QString& id) const;

    void updateProfile(const QString& id, const PmuProfile& profile);
    bool startProfile(const QString& id, QString* errorMessage = nullptr);
    void stopProfile(const QString& id);

signals:
    void profilesChanged();
    void statusChanged(const QString& id);

private:
    struct Entry {
        PmuProfile profile;
        PmuRuntimeStatus status;
        QTcpServer* server = nullptr;
        QTcpSocket* socket = nullptr;
        QByteArray receiveBuffer;
        QTimer* transmitTimer = nullptr;
        bool transmitting = false;
    };

    QList<Entry> m_entries;

    Entry* findEntry(const QString& id);
    const Entry* findEntry(const QString& id) const;

    void seedProfiles();
    void configureSignals(Entry& entry);
    void startTransmission(Entry& entry);
    void stopTransmission(Entry& entry);
    void disconnectSocket(Entry& entry, const QString& message, bool keepError = false);
    void disconnectIfUnauthorized(Entry& entry);
    void processIncomingData(Entry& entry, const QByteArray& chunk);
    void handleCommandFrame(Entry& entry, const QByteArray& frame);
    QByteArray buildCfg2Frame(const Entry& entry) const;
    QByteArray buildDataFrame(const Entry& entry) const;
    QString normalizeIp(const QString& value) const;
    quint16 crc16(const QByteArray& buffer) const;
};
