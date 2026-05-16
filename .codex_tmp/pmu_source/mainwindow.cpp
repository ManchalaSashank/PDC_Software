#include "mainwindow.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    populateProfiles();

    connect(&m_service, &PmuSimulatorService::profilesChanged, this, [this]() {
        const QString selected = m_selectedId;
        populateProfiles();
        loadProfile(selected.isEmpty() ? m_selectedId : selected);
    });

    connect(&m_service, &PmuSimulatorService::statusChanged, this, [this](const QString& id) {
        if (id == m_selectedId) {
            renderStatus(id);
        }
    });
}

void MainWindow::buildUi()
{
    setWindowTitle("PMU App - Qt");
    resize(1180, 760);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QHBoxLayout(central);

    auto* sidebarBox = new QGroupBox("PMU Profiles", central);
    auto* sidebarLayout = new QVBoxLayout(sidebarBox);
    m_profileList = new QListWidget(sidebarBox);
    sidebarLayout->addWidget(m_profileList);
    root->addWidget(sidebarBox, 1);

    auto* mainPanel = new QWidget(central);
    auto* mainLayout = new QVBoxLayout(mainPanel);

    auto* configBox = new QGroupBox("PMU Configuration", mainPanel);
    auto* configLayout = new QFormLayout(configBox);

    m_nameEdit = new QLineEdit(configBox);
    m_stationNameEdit = new QLineEdit(configBox);
    m_pmuIdSpin = new QSpinBox(configBox);
    m_portSpin = new QSpinBox(configBox);
    m_rateCombo = new QComboBox(configBox);
    m_ipEdit = new QLineEdit(configBox);

    m_pmuIdSpin->setRange(1, 65535);
    m_portSpin->setRange(1, 65535);
    m_rateCombo->addItems({ "10", "25", "50", "60" });

    configLayout->addRow("PMU Name", m_nameEdit);
    configLayout->addRow("Station Name", m_stationNameEdit);
    configLayout->addRow("PMU ID", m_pmuIdSpin);
    configLayout->addRow("Listen Port", m_portSpin);
    configLayout->addRow("Phasor Rate (fps)", m_rateCombo);
    configLayout->addRow("PDC IP To Accept", m_ipEdit);
    mainLayout->addWidget(configBox);

    auto* channelBox = new QGroupBox("Channels To Stream", mainPanel);
    auto* channelLayout = new QGridLayout(channelBox);
    const QStringList channels = { "VA", "VB", "VC", "IA", "IB", "IC", "F", "DFDT" };
    for (int i = 0; i < channels.size(); ++i) {
        auto* check = new QCheckBox(channels[i] == "DFDT" ? "df/dt" : channels[i], channelBox);
        check->setProperty("channelKey", channels[i]);
        m_channelChecks.append(check);
        channelLayout->addWidget(check, i / 4, i % 4);
    }
    mainLayout->addWidget(channelBox);

    auto* statusBox = new QGroupBox("Runtime Status", mainPanel);
    auto* statusLayout = new QGridLayout(statusBox);
    m_serverStateValue = new QLabel("--", statusBox);
    m_transmissionValue = new QLabel("--", statusBox);
    m_connectedPdcValue = new QLabel("--", statusBox);
    m_framesSentValue = new QLabel("0", statusBox);
    statusLayout->addWidget(new QLabel("Server State", statusBox), 0, 0);
    statusLayout->addWidget(m_serverStateValue, 0, 1);
    statusLayout->addWidget(new QLabel("Transmission", statusBox), 0, 2);
    statusLayout->addWidget(m_transmissionValue, 0, 3);
    statusLayout->addWidget(new QLabel("Connected PDC", statusBox), 1, 0);
    statusLayout->addWidget(m_connectedPdcValue, 1, 1);
    statusLayout->addWidget(new QLabel("Frames Sent", statusBox), 1, 2);
    statusLayout->addWidget(m_framesSentValue, 1, 3);
    mainLayout->addWidget(statusBox);

    auto* buttonRow = new QHBoxLayout();
    m_saveButton = new QPushButton("Save Settings", mainPanel);
    m_startButton = new QPushButton("Start PMU", mainPanel);
    m_stopButton = new QPushButton("Stop PMU", mainPanel);
    buttonRow->addWidget(m_saveButton);
    buttonRow->addWidget(m_startButton);
    buttonRow->addWidget(m_stopButton);
    buttonRow->addStretch();
    mainLayout->addLayout(buttonRow);

    auto* messageBox = new QGroupBox("Connection Rules", mainPanel);
    auto* messageLayout = new QVBoxLayout(messageBox);
    m_statusBox = new QTextEdit(messageBox);
    m_statusBox->setReadOnly(true);
    messageLayout->addWidget(m_statusBox);
    mainLayout->addWidget(messageBox, 1);

    root->addWidget(mainPanel, 3);

    connect(m_profileList, &QListWidget::currentTextChanged, this, [this](const QString&) {
        if (auto* item = m_profileList->currentItem()) {
            loadProfile(item->data(Qt::UserRole).toString());
        }
    });

    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedId.isEmpty()) {
            return;
        }
        m_service.updateProfile(m_selectedId, currentFormProfile());
        renderStatus(m_selectedId);
    });

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedId.isEmpty()) {
            return;
        }
        QString error;
        if (!m_service.startProfile(m_selectedId, &error) && !error.isEmpty()) {
            QMessageBox::warning(this, "Unable To Start PMU", error);
        }
        renderStatus(m_selectedId);
    });

    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedId.isEmpty()) {
            return;
        }
        m_service.stopProfile(m_selectedId);
        renderStatus(m_selectedId);
    });
}

void MainWindow::populateProfiles()
{
    m_profileList->clear();
    const QList<PmuProfile> profiles = m_service.profiles();
    for (const PmuProfile& profile : profiles) {
        auto* item = new QListWidgetItem(QString("%1 (PMU %2)").arg(profile.name, QString::number(profile.pmuId)));
        item->setData(Qt::UserRole, profile.id);
        m_profileList->addItem(item);
    }

    if (m_profileList->count() > 0 && !m_selectedId.isEmpty()) {
        for (int i = 0; i < m_profileList->count(); ++i) {
            if (m_profileList->item(i)->data(Qt::UserRole).toString() == m_selectedId) {
                m_profileList->setCurrentRow(i);
                return;
            }
        }
    }

    if (m_profileList->count() > 0) {
        m_profileList->setCurrentRow(0);
    }
}

void MainWindow::loadProfile(const QString& id)
{
    const PmuProfile profile = m_service.profile(id);
    if (profile.id.isEmpty()) {
        return;
    }

    m_selectedId = id;
    m_nameEdit->setText(profile.name);
    m_stationNameEdit->setText(profile.stationName);
    m_pmuIdSpin->setValue(profile.pmuId);
    m_portSpin->setValue(profile.port);
    m_rateCombo->setCurrentText(QString::number(profile.dataRate));
    m_ipEdit->setText(profile.acceptedPdcIp);
    for (QCheckBox* check : m_channelChecks) {
        check->setChecked(profile.selectedChannels.contains(check->property("channelKey").toString()));
    }
    renderStatus(id);
}

void MainWindow::renderStatus(const QString& id)
{
    const PmuRuntimeStatus status = m_service.status(id);
    m_serverStateValue->setText(status.serverState);
    m_transmissionValue->setText(status.transmissionState);
    m_connectedPdcValue->setText(status.connectedPdcIp.isEmpty() ? "--" : status.connectedPdcIp);
    m_framesSentValue->setText(QString::number(status.framesSent));

    QStringList lines;
    lines << QString("Message: %1").arg(status.message);
    lines << QString("Accepted PDC IP: %1").arg(m_ipEdit->text());
    lines << QString("Denied Requests: %1").arg(status.deniedRequests);
    lines << QString("Last Client: %1").arg(status.lastClientAt.isValid() ? status.lastClientAt.toString(Qt::ISODate) : "--");
    lines << QString("Last Config Update: %1").arg(status.lastConfigUpdatedAt.isValid() ? status.lastConfigUpdatedAt.toString(Qt::ISODate) : "--");
    if (!status.lastError.isEmpty()) {
        lines << QString("Last Error: %1").arg(status.lastError);
    }
    m_statusBox->setPlainText(lines.join("\n"));
}

PmuProfile MainWindow::currentFormProfile() const
{
    PmuProfile profile = m_service.profile(m_selectedId);
    profile.name = m_nameEdit->text().trimmed();
    profile.stationName = m_stationNameEdit->text().trimmed().left(16);
    profile.pmuId = static_cast<quint16>(m_pmuIdSpin->value());
    profile.port = static_cast<quint16>(m_portSpin->value());
    profile.dataRate = m_rateCombo->currentText().toInt();
    profile.acceptedPdcIp = m_ipEdit->text().trimmed();
    profile.selectedChannels.clear();
    for (QCheckBox* check : m_channelChecks) {
        if (check->isChecked()) {
            profile.selectedChannels.append(check->property("channelKey").toString());
        }
    }
    return profile;
}
