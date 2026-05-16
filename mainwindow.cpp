#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QMessageBox>
#include <QScrollBar>
#include <QSizePolicy>
#include <QString>
#include <QThread>
#include <QVBoxLayout>

#include "datawindow.h"
#include "pmu_worker.h"
#include "pdc_dashboardwindow.h"

namespace
{
QString statusBadgeStyle(const QString& background, const QString& border, const QString& text)
{
    return QString(
        "QLabel {"
        "background: %1;"
        "border: 1px solid %2;"
        "border-radius: 9px;"
        "color: %3;"
        "font-weight: 800;"
        "padding: 7px 11px;"
        "}")
        .arg(background, border, text);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("PDC Monitor - PMU Data Concentrator");

    ui->widget->setObjectName("topBar");
    ui->scrollArea->setObjectName("connectionScroll");
    ui->scrollAreaWidgetContents->setObjectName("connectionCanvas");
    ui->connectButton->setObjectName("primaryConnectButton");
    ui->addConnectionButton->setObjectName("addPmuButton");
    ui->label->setObjectName("appTitle");
    ui->widget->setStyleSheet("");
    ui->connectButton->setStyleSheet("");
    ui->addConnectionButton->setStyleSheet("");
    ui->scrollArea->setFrameShape(QFrame::NoFrame);

    if (auto* topLayout = qobject_cast<QHBoxLayout*>(ui->widget->layout()))
    {
        topLayout->setContentsMargins(30, 18, 30, 18);
        topLayout->setSpacing(14);

        QWidget* brandBlock = new QWidget(ui->widget);
        brandBlock->setObjectName("brandBlock");
        QVBoxLayout* brandLayout = new QVBoxLayout(brandBlock);
        brandLayout->setContentsMargins(0, 0, 0, 0);
        brandLayout->setSpacing(4);

        topLayout->removeWidget(ui->label);
        ui->label->setParent(brandBlock);
        ui->label->setText("PDC Monitor");
        brandLayout->addWidget(ui->label);

        QLabel* subtitle = new QLabel("PMU data concentrator and real-time grid telemetry", brandBlock);
        subtitle->setObjectName("appSubtitle");
        brandLayout->addWidget(subtitle);

        topLayout->insertWidget(0, brandBlock, 1);

        QPushButton* centralPdcButton = new QPushButton("Open Central PDC", ui->widget);
        centralPdcButton->setObjectName("centralPdcButton");
        centralPdcButton->setMinimumHeight(46);
        int addButtonIndex = topLayout->indexOf(ui->addConnectionButton);
        topLayout->insertWidget(addButtonIndex >= 0 ? addButtonIndex : topLayout->count(), centralPdcButton);
        connect(centralPdcButton, &QPushButton::clicked, this, &MainWindow::openCentralPdcDashboard);
    }

    ui->verticalLayout_2->setContentsMargins(0, 0, 0, 24);
    ui->verticalLayout_2->setSpacing(0);
    ui->verticalLayout_2->setAlignment(ui->connectButton, Qt::Alignment());
    ui->verticalLayout->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout->setSpacing(0);

    setStyleSheet(R"(
QMainWindow,
QWidget#centralwidget {
    background: #0d1117;
    color: #edf4fb;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 13px;
}

QWidget#topBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                stop:0 #121b28,
                                stop:0.55 #111821,
                                stop:1 #0f1722);
    border: none;
}

QWidget#brandBlock {
    background: transparent;
}

QLabel#appTitle {
    background: transparent;
    color: #f8fafc;
    font-size: 25px;
    font-weight: 800;
    letter-spacing: 0px;
}

QLabel#appSubtitle {
    background: transparent;
    color: #91a6ba;
    font-size: 12px;
    font-weight: 600;
}

QScrollArea#connectionScroll {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                stop:0 #0d1117,
                                stop:0.52 #101722,
                                stop:1 #0c1415);
    border: none;
}

QWidget#connectionCanvas {
    background: transparent;
}

QLineEdit {
    background: rgba(9, 17, 29, 210);
    border: 1px solid #2c4055;
    border-radius: 8px;
    color: #edf4fb;
    padding: 10px 12px;
    selection-background-color: #0ea5e9;
}

QLineEdit:focus {
    border: 1px solid #38bdf8;
    background: rgba(13, 26, 42, 235);
}

QLineEdit:disabled {
    color: #748294;
    background: rgba(18, 25, 35, 180);
}

QPushButton {
    background: #0ea5e9;
    border: none;
    border-radius: 8px;
    color: #f8fafc;
    font-weight: 700;
    padding: 10px 16px;
}

QPushButton:hover {
    background: #0284c7;
}

QPushButton:pressed {
    background: #0369a1;
}

QPushButton:disabled {
    background: #253142;
    color: #7a8797;
}

QPushButton#addPmuButton {
    background: #2563eb;
    border: 1px solid #4d7df0;
    border-radius: 9px;
    min-height: 24px;
    padding: 10px 18px;
}

QPushButton#addPmuButton:hover {
    background: #1d4ed8;
}

QPushButton#centralPdcButton {
    background: #172235;
    border: 1px solid #3b82f6;
    border-radius: 9px;
    color: #dbeafe;
    min-height: 24px;
    padding: 10px 18px;
}

QPushButton#centralPdcButton:hover {
    background: #1d3554;
    border-color: #60a5fa;
}

QPushButton#centralPdcButton:pressed {
    background: #132238;
}

QPushButton#primaryConnectButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                stop:0 #10b981,
                                stop:1 #14b8a6);
    border: 1px solid #35d39f;
    border-radius: 9px;
    color: #ffffff;
    font-size: 14px;
    font-weight: 800;
    margin-left: 38px;
    margin-right: 38px;
    padding: 11px 18px;
}

QPushButton#primaryConnectButton:hover {
    background: #059669;
}

QPushButton#primaryConnectButton:pressed {
    background: #047857;
}

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 4px;
}

QScrollBar::handle:vertical {
    background: #303b4f;
    border-radius: 5px;
    min-height: 32px;
}

QScrollBar::handle:vertical:hover {
    background: #4b5a73;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    height: 0;
    background: transparent;
}
)");

    ui->connectButton->setMinimumHeight(46);
    ui->connectButton->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->connectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->gridLayoutConnections->setHorizontalSpacing(24);
    ui->gridLayoutConnections->setVerticalSpacing(24);
    ui->gridLayoutConnections->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    ui->gridLayoutConnections->setContentsMargins(34, 46, 34, 34);
    ui->scrollArea->setWidgetResizable(true);

    addConnectionCard();
}

MainWindow::~MainWindow()
{
    for (auto& card : cards)
    {
        if (card.worker)
        {
            card.worker->stop();
        }
        if (card.thread)
        {
            card.thread->quit();
            card.thread->wait(3000);
        }
    }

    delete ui;
}

int MainWindow::cardIndexForWidget(QObject* widget) const
{
    for (int i = 0; i < cards.size(); ++i)
    {
        if (cards[i].dashboardButton == widget || cards[i].removeButton == widget || cards[i].disconnectButton == widget)
            return i;
    }

    return -1;
}

int MainWindow::cardIndexForWorker(PMUWorker* worker) const
{
    for (int i = 0; i < cards.size(); ++i)
    {
        if (cards[i].worker == worker)
            return i;
    }

    return -1;
}

int MainWindow::cardIndexForThread(QThread* thread) const
{
    for (int i = 0; i < cards.size(); ++i)
    {
        if (cards[i].thread == thread)
            return i;
    }

    return -1;
}

void MainWindow::updateCardTitles()
{
    for (int i = 0; i < cards.size(); ++i)
    {
        cards[i].titleLabel->setText(QString("PMU #%1").arg(i + 1));
    }
}

void MainWindow::resetCardConnectionState(int index)
{
    if (index < 0 || index >= cards.size())
        return;

    cards[index].worker = nullptr;
    cards[index].thread = nullptr;
    cards[index].dashboardButton->hide();
    cards[index].disconnectButton->hide();
    cards[index].removeButton->setEnabled(true);
    cards[index].removeButton->show();
    cards[index].removeButton->setText("Remove PMU");
    cards[index].deviceIdEdit->setEnabled(true);
    cards[index].ipEdit->setEnabled(true);
    cards[index].portEdit->setEnabled(true);
}

void MainWindow::disconnectCard(int index)
{
    if (index < 0 || index >= cards.size() || !cards[index].worker)
        return;

    cards[index].statusLabel->setText("Disconnecting...");
    cards[index].statusLabel->setStyleSheet(statusBadgeStyle("#302612", "#d9a441", "#ffe5a6"));
    cards[index].disconnectButton->setEnabled(false);
    cards[index].dashboardButton->setEnabled(false);
    cards[index].worker->stop();
}

void MainWindow::openCentralPdcDashboard()
{
    if (cards.empty())
    {
        QMessageBox::information(this, "Central PDC", "Add at least one PMU connection first.");
        return;
    }

    QVector<PdcPmuSource> sources;
    sources.reserve(static_cast<int>(cards.size()));

    for (int i = 0; i < cards.size(); ++i)
    {
        const QString deviceId = cards[i].deviceIdEdit->text().trimmed();
        QString deviceName = deviceId.isEmpty()
                                 ? QString("PMU #%1").arg(i + 1)
                                 : QString("PMU ID %1").arg(deviceId);

        sources.push_back({deviceName, cards[i].worker});
    }

    auto* window = new PdcDashboardWindow(sources);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void MainWindow::on_connectButton_clicked()
{
    bool anyConnected = false;

    for (int i = 0; i < cards.size(); i++)
    {
        QString ip = cards[i].ipEdit->text().trimmed();
        int port = cards[i].portEdit->text().toInt();
        bool deviceIdOk = false;
        int deviceId = cards[i].deviceIdEdit->text().trimmed().toInt(&deviceIdOk);

        if (!deviceIdOk || deviceId <= 0 || deviceId > 65535 || ip.isEmpty() || port == 0)
        {
            cards[i].statusLabel->setText("Invalid input");
            cards[i].statusLabel->setStyleSheet(statusBadgeStyle("#381922", "#ef6f7b", "#ffd8dd"));
            continue;
        }

        if (cards[i].worker != nullptr)
        {
            cards[i].statusLabel->setText("Already connected");
            cards[i].statusLabel->setStyleSheet(statusBadgeStyle("#302612", "#d9a441", "#ffe5a6"));
            continue;
        }

        QThread* thread = new QThread();
        PMUWorker* worker = new PMUWorker(ip, port, deviceId);

        cards[i].worker = worker;
        cards[i].thread = thread;

        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &PMUWorker::start);
        connect(worker, &PMUWorker::finished, thread, &QThread::quit);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        connect(thread, &QThread::finished, this, [this, thread]()
                {
                    int index = cardIndexForThread(thread);
                    if (index >= 0)
                    {
                        resetCardConnectionState(index);
                    }
                });

        connect(worker, &PMUWorker::statusUpdate, this, [this, worker](QString status)
                {
                    int index = cardIndexForWorker(worker);
                    if (index < 0) return;

                    cards[index].statusLabel->setText(status);

                    if (status == "Connected")
                    {
                        cards[index].statusLabel->setStyleSheet(
                            statusBadgeStyle("#102821", "#28c081", "#8ee6bd"));
                        cards[index].dashboardButton->show();
                        cards[index].dashboardButton->setEnabled(true);
                        cards[index].disconnectButton->show();
                        cards[index].disconnectButton->setEnabled(true);
                        cards[index].removeButton->hide();
                        cards[index].deviceIdEdit->setEnabled(false);
                        cards[index].ipEdit->setEnabled(false);
                        cards[index].portEdit->setEnabled(false);
                    }
                    else if (status.contains("failed", Qt::CaseInsensitive) ||
                             status.contains("mismatch", Qt::CaseInsensitive) ||
                             status.contains("Disconnected"))
                    {
                        cards[index].statusLabel->setStyleSheet(
                            statusBadgeStyle("#381922", "#ef6f7b", "#ffd8dd"));
                        cards[index].dashboardButton->hide();
                        cards[index].dashboardButton->setEnabled(true);
                        cards[index].disconnectButton->hide();
                        cards[index].disconnectButton->setEnabled(true);
                        cards[index].removeButton->show();
                        cards[index].removeButton->setEnabled(true);
                        cards[index].deviceIdEdit->setEnabled(true);
                        cards[index].ipEdit->setEnabled(true);
                        cards[index].portEdit->setEnabled(true);
                    }
                    else
                    {
                        cards[index].statusLabel->setStyleSheet(
                            statusBadgeStyle("#302612", "#d9a441", "#ffe5a6"));
                    }
                });

        connect(worker, &PMUWorker::newData, this, [i](float freq)
                {
                    qDebug() << "PMU" << (i + 1) << "Frequency:" << freq;
                });

        thread->start();

        anyConnected = true;
    }

    if (anyConnected)
    {
        QMessageBox::information(this, "Connection", "Connection requests sent.");
    }
}

void MainWindow::on_addConnectionButton_clicked()
{
    if (cards.size() >= 12)
    {
        QMessageBox::warning(this, "Limit Reached", "Maximum 12 PMU connections allowed.");
        return;
    }

    addConnectionCard();
}

void MainWindow::addConnectionCard()
{
    ConnectionCard card;

    card.container = new QWidget();
    card.container->setObjectName("connectionCard");
    card.container->setFixedSize(346, 292);

    QVBoxLayout* layout = new QVBoxLayout(card.container);
    layout->setSpacing(9);
    layout->setContentsMargins(18, 18, 18, 18);

    card.container->setStyleSheet(
        "QWidget#connectionCard {"
        "background-color: rgba(18, 27, 39, 222);"
        "border-radius: 12px;"
        "border: 1px solid #2c4054;"
        "}"
        "QLineEdit {"
        "background-color: rgba(9, 17, 29, 215);"
        "border: 1px solid #31475e;"
        "border-radius: 8px;"
        "padding: 8px 10px;"
        "color: #edf4fb;"
        "font-size: 13px;"
        "min-height: 22px;"
        "}"
        "QLineEdit:focus { border: 1px solid #38bdf8; background-color: rgba(12, 26, 43, 240); }"
        "QLineEdit:disabled { color: #7a8798; background: rgba(17, 24, 34, 185); }"
        "QPushButton {"
        "background-color: #0ea5e9;"
        "border-radius: 8px;"
        "padding: 8px 10px;"
        "color: #ffffff;"
        "font-weight: 800;"
        "border: 1px solid transparent;"
        "min-height: 22px;"
        "}"
        "QPushButton:hover { background-color: #0284c7; }");

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(26);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 88));
    card.container->setGraphicsEffect(shadow);

    card.titleLabel = new QLabel(QString("PMU #%1").arg(cards.size() + 1));
    card.titleLabel->setStyleSheet("color: #f8fafc; font-weight: 800; font-size: 18px; background: transparent;");
    layout->addWidget(card.titleLabel);

    card.deviceIdEdit = new QLineEdit();
    card.deviceIdEdit->setPlaceholderText("Device ID (required)");
    card.deviceIdEdit->setValidator(new QIntValidator(1, 65535, card.deviceIdEdit));
    card.deviceIdEdit->setText(QString::number(cards.size() + 1));
    card.deviceIdEdit->setMinimumHeight(38);
    layout->addWidget(card.deviceIdEdit);

    card.ipEdit = new QLineEdit();
    card.ipEdit->setPlaceholderText("IP address");
    card.ipEdit->setText("127.0.0.1");
    card.ipEdit->setMinimumHeight(38);
    layout->addWidget(card.ipEdit);

    card.portEdit = new QLineEdit();
    card.portEdit->setPlaceholderText("Port");
    card.portEdit->setText("4712");
    card.portEdit->setMinimumHeight(38);
    layout->addWidget(card.portEdit);

    card.statusLabel = new QLabel("Disconnected");
    card.statusLabel->setMinimumHeight(30);
    card.statusLabel->setStyleSheet(statusBadgeStyle("#202b3b", "#33485f", "#adbbcc"));
    layout->addWidget(card.statusLabel);

    card.dashboardButton = new QPushButton("Open Dashboard");
    card.dashboardButton->setStyleSheet(
        "QPushButton { background-color: #10b981; border: 1px solid #35d39f; color: white; }"
        "QPushButton:hover { background-color: #059669; }");
    card.dashboardButton->hide();

    card.disconnectButton = new QPushButton("Disconnect");
    card.disconnectButton->setStyleSheet(
        "QPushButton { background-color: #f4b84e; border: 1px solid #f7cb73; color: #111827; }"
        "QPushButton:hover { background-color: #c49439; }"
        "QPushButton:disabled { background-color: #263142; color: #7b8494; }");
    card.disconnectButton->hide();

    QWidget* connectedActions = new QWidget(card.container);
    connectedActions->setObjectName("connectedActions");
    connectedActions->setStyleSheet("QWidget#connectedActions { background: transparent; border: none; }");
    QHBoxLayout* actionLayout = new QHBoxLayout(connectedActions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(10);
    actionLayout->addWidget(card.dashboardButton);
    actionLayout->addWidget(card.disconnectButton);
    layout->addWidget(connectedActions);

    card.removeButton = new QPushButton("Remove PMU");
    card.removeButton->setStyleSheet(
        "QPushButton { background-color: #ef6f7b; border: 1px solid #f28b95; color: white; }"
        "QPushButton:hover { background-color: #d85e6b; }");
    layout->addWidget(card.removeButton);

    int cardIndex = cards.size();
    cards.push_back(card);

    connect(cards[cardIndex].dashboardButton, &QPushButton::clicked, this,
            [this]()
            {
                int cardIndex = cardIndexForWidget(sender());
                if (cardIndex < 0 || !cards[cardIndex].worker)
                {
                    QMessageBox::warning(this, "Error", "Worker not available.");
                    return;
                }

                auto& card = cards[cardIndex];
                QString deviceName = QString("PMU ID %1").arg(card.deviceIdEdit->text().trimmed());

                DataWindow* window = new DataWindow(&card.worker->getDataManager(), card.worker, deviceName);
                window->setWindowTitle(deviceName + " - Dashboard");
                window->setAttribute(Qt::WA_DeleteOnClose);
                window->show();
            });

    connect(cards[cardIndex].removeButton, &QPushButton::clicked, this,
            [this]()
            {
                int cardIndex = cardIndexForWidget(sender());
                if (cardIndex < 0) return;

                auto reply = QMessageBox::question(this, "Confirm Removal",
                                                   "Are you sure you want to remove this PMU connection?",
                                                   QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::No) return;

                if (cards[cardIndex].worker)
                {
                    cards[cardIndex].worker->stop();
                }

                if (cards[cardIndex].thread)
                {
                    cards[cardIndex].thread->quit();
                    cards[cardIndex].thread->wait(2000);
                }

                cards[cardIndex].container->deleteLater();
                cards.erase(cards.begin() + cardIndex);
                refreshGrid();
                updateCardTitles();
            });

    connect(cards[cardIndex].disconnectButton, &QPushButton::clicked, this,
            [this]()
            {
                int cardIndex = cardIndexForWidget(sender());
                if (cardIndex < 0) return;
                disconnectCard(cardIndex);
            });

    int row = cardIndex / 3;
    int col = cardIndex % 3;
    ui->gridLayoutConnections->addWidget(cards[cardIndex].container, row, col);
}

void MainWindow::refreshGrid()
{
    QLayoutItem* item;
    while ((item = ui->gridLayoutConnections->takeAt(0)) != nullptr)
    {
    }

    for (int i = 0; i < cards.size(); i++)
    {
        int row = i / 3;
        int col = i % 3;
        ui->gridLayoutConnections->addWidget(cards[i].container, row, col);
    }

    updateCardTitles();
}
