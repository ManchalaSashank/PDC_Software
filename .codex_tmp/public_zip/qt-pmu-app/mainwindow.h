#pragma once

#include "pmusimulatorservice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    PmuSimulatorService m_service;
    QString m_selectedId;

    QListWidget* m_profileList = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_stationNameEdit = nullptr;
    QSpinBox* m_pmuIdSpin = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QComboBox* m_rateCombo = nullptr;
    QLineEdit* m_ipEdit = nullptr;
    QList<QCheckBox*> m_channelChecks;

    QLabel* m_serverStateValue = nullptr;
    QLabel* m_transmissionValue = nullptr;
    QLabel* m_connectedPdcValue = nullptr;
    QLabel* m_framesSentValue = nullptr;
    QTextEdit* m_statusBox = nullptr;

    QPushButton* m_saveButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;

    void buildUi();
    void populateProfiles();
    void loadProfile(const QString& id);
    void renderStatus(const QString& id);
    PmuProfile currentFormProfile() const;
};
