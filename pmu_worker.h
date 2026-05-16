#pragma once

#include <QObject>
#include <QString>
#include <mutex>
#include <winsock2.h>
#include "data_manager.h"

class PMUWorker : public QObject
{
    Q_OBJECT

public:
    PMUWorker(QString ip, int port, int id);
    DataManager& getDataManager();
    PMUConfig getConfig() const;
    bool running = true;

public slots:
    void start();
    void stop();

signals:
    void newData(float frequency);
    void statusUpdate(QString status);
    void configUpdate(PMUConfig config);
    void finished();

private:
    void setActiveSocket(SOCKET sock);
    void clearActiveSocket();
    bool closeActiveSocket(SOCKET sock);

    QString ip;
    int port;
    int pmuId;
    SOCKET activeSocket = INVALID_SOCKET;
    std::mutex socketMutex;

    PMUConfig config;
    DataManager dataManager;
signals:
    void newFrame(const PMUFrame& frame);
};
