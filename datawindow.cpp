#include "datawindow.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <QTableWidget>
#include <QHeaderView>
#include <QGraphicsDropShadowEffect>
#include <QDateTime>
#include <QFrame>
#include <QTimeZone>
#include <QToolTip>
#include <cmath>
#include <limits>

namespace {
// Helper to compute a value from frame based on current selection
double valueFromFrame(const PMUFrame& f, const QString& sel, int chIdx)
{
    if (sel == "Frequency") return f.frequency;
    if (sel == "ROCOF") return f.rocof;

    if ((sel == "Voltage Magnitude" || sel == "Current Magnitude") && f.phasors.size() > chIdx)
        return f.phasors[chIdx].magnitude;

    if ((sel == "Voltage Angle" || sel == "Current Angle") && f.phasors.size() > chIdx)
        return f.phasors[chIdx].angleDeg;

    return 0.0;
}

bool isVoltageSignal(const QString& signal)
{
    return signal == "Voltage Magnitude" || signal == "Voltage Angle";
}

bool isCurrentSignal(const QString& signal)
{
    return signal == "Current Magnitude" || signal == "Current Angle";
}

bool isMagnitudeSignal(const QString& signal)
{
    return signal == "Voltage Magnitude" || signal == "Current Magnitude";
}

bool isAngleSignal(const QString& signal)
{
    return signal == "Voltage Angle" || signal == "Current Angle";
}

bool isPhasorSignal(const QString& signal)
{
    return isMagnitudeSignal(signal) || isAngleSignal(signal);
}

double phasorValueForSignal(const PhasorData& phasor, const QString& signal)
{
    return isAngleSignal(signal) ? phasor.angleDeg : phasor.magnitude;
}

bool phasorMatchesSignal(const PhasorData& phasor, const QString& signal)
{
    Q_UNUSED(phasor);
    return isPhasorSignal(signal);
}

bool configPhasorMatchesSignal(const PMUConfig& config, int index, const QString& signal)
{
    Q_UNUSED(config);
    Q_UNUSED(index);
    return isPhasorSignal(signal);
}

bool hasConfigPhasorType(const PMUConfig& config, bool voltage)
{
    Q_UNUSED(voltage);
    return config.phasorCount > 0;
}

QFrame* createSection(const QString& titleText, QLayout* contentLayout)
{
    QFrame* section = new QFrame();
    section->setObjectName("sectionBox");

    QVBoxLayout* wrapper = new QVBoxLayout(section);
    wrapper->setSpacing(12);
    wrapper->setContentsMargins(18, 16, 18, 18);

    QLabel* title = new QLabel(titleText);
    title->setObjectName("sectionTitle");
    wrapper->addWidget(title);
    wrapper->addLayout(contentLayout);

    return section;
}
}

DataWindow::DataWindow(DataManager* manager, PMUWorker* worker, const QString& deviceName, QWidget* parent)
    : QWidget(parent), dataManager(manager), worker(worker), deviceName(deviceName)
{
    qDebug() << "DataWindow created with worker:" << worker;
    currentConfig = worker ? worker->getConfig() : PMUConfig();

    // Connect to worker's newFrame signal
    connect(worker, &PMUWorker::newFrame,
            this, &DataWindow::onNewFrame,
            Qt::QueuedConnection);
    connect(worker, &PMUWorker::configUpdate, this, [this](PMUConfig config) {
        currentConfig = config;
        applyConfigToUi();
    }, Qt::QueuedConnection);
    connect(worker, &QObject::destroyed, this, [this]() {
        this->worker = nullptr;
        dataManager = nullptr;
        statusIndicator->setText("DISCONNECTED");
        statusIndicator->setStyleSheet("color: #ef6f7b; font-weight: 800; background: transparent;");
    });

    this->setStyleSheet(R"(
QWidget {
    background-color: #10131a;
    color: #e6edf3;
    font-family: 'Segoe UI', Arial, sans-serif;
    font-size: 13px;
}

QFrame#sectionBox {
    background-color: #171d27;
    border: 1px solid #2c374a;
    border-radius: 10px;
}

QLabel#sectionTitle {
    background: transparent;
    color: #f3f6fb;
    font-size: 15px;
    font-weight: 700;
}

QComboBox, QLineEdit {
    background-color: #111827;
    border: 1px solid #2b3648;
    border-radius: 8px;
    padding: 8px 10px;
    color: #e6edf3;
    min-height: 18px;
}

QComboBox:focus, QLineEdit:focus {
    border: 1px solid #5d9cec;
}

QComboBox::drop-down {
    border: none;
}

QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 6px solid #5d9cec;
}

QPushButton {
    background-color: #2f80ed;
    border: none;
    border-radius: 8px;
    padding: 9px 16px;
    color: white;
    font-weight: 600;
}

QPushButton:hover {
    background-color: #256fd0;
}

QPushButton:pressed {
    background-color: #1e5fb5;
}

QCheckBox {
    spacing: 8px;
    color: #d7dde7;
}

QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border-radius: 4px;
    border: 2px solid #2b3648;
    background-color: #111827;
}

QCheckBox::indicator:checked {
    background-color: #2f80ed;
    border-color: #2f80ed;
}

QTableWidget {
    background-color: #111827;
    alternate-background-color: #151d2b;
    gridline-color: #2c374a;
    border: 1px solid #2c374a;
    border-radius: 8px;
    color: #e6edf3;
    selection-background-color: #244a78;
}

QHeaderView::section {
    background-color: #1a2230;
    padding: 8px;
    border: none;
    color: #e6edf3;
    font-weight: 700;
}

QTableWidget::item {
    padding: 6px;
}

/* Modern Scrollbar */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 4px 2px 4px 2px;
}

QScrollBar::handle:vertical {
    background: #303b4f;
    border-radius: 5px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: #4b5a73;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: none;
}
)");

    // Main scroll area for content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* container = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setSpacing(18);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    scrollArea->setWidget(container);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);

    // ===== HEADER =====
    QHBoxLayout* headerLayout = new QHBoxLayout();

    titleLabel = new QLabel(deviceName + " Dashboard");
    titleLabel->setStyleSheet("font-size: 24px; font-weight: 800; color: #f3f6fb; background: transparent;");

    statusIndicator = new QLabel("LIVE");
    statusIndicator->setStyleSheet("color: #28c081; font-weight: 800; font-size: 14px; background: transparent;");

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusIndicator);

    mainLayout->addLayout(headerLayout);

    // ===== METRICS CARDS =====
    QHBoxLayout* metricsLayout = new QHBoxLayout();
    metricsLayout->setSpacing(16);

    freqLabel = new QLabel("Frequency\n--");
    rocofLabel = new QLabel("ROCOF\n--");
    pmuTimeLabel = new QLabel("PMU Time\n--");
    latencyLabel = new QLabel("Latency\n--");

    QString cardStyle =
        "QLabel {"
        "background-color: #171d27;"
        "border-radius: 10px;"
        "padding: 20px;"
        "font-size: 18px;"
        "font-weight: 700;"
        "border: 1px solid #2c374a;"
        "color: #e6edf3;"
        "min-width: 160px;"
        "}";

    freqLabel->setStyleSheet(cardStyle);
    rocofLabel->setStyleSheet(cardStyle);
    pmuTimeLabel->setStyleSheet(cardStyle);
    latencyLabel->setStyleSheet(cardStyle);
    freqLabel->setAlignment(Qt::AlignCenter);
    rocofLabel->setAlignment(Qt::AlignCenter);
    pmuTimeLabel->setAlignment(Qt::AlignCenter);
    latencyLabel->setAlignment(Qt::AlignCenter);

    // Add subtle shadows
    QGraphicsDropShadowEffect* shadow1 = new QGraphicsDropShadowEffect();
    shadow1->setBlurRadius(22);
    shadow1->setOffset(0, 8);
    shadow1->setColor(QColor(0, 0, 0, 80));
    freqLabel->setGraphicsEffect(shadow1);

    QGraphicsDropShadowEffect* shadow2 = new QGraphicsDropShadowEffect();
    shadow2->setBlurRadius(22);
    shadow2->setOffset(0, 8);
    shadow2->setColor(QColor(0, 0, 0, 80));
    rocofLabel->setGraphicsEffect(shadow2);

    metricsLayout->addWidget(freqLabel);
    metricsLayout->addWidget(rocofLabel);
    metricsLayout->addWidget(pmuTimeLabel);
    metricsLayout->addWidget(latencyLabel);
    metricsLayout->addStretch();

    mainLayout->addLayout(metricsLayout);

    alertBanner = new QLabel("No active grid events");
    alertBanner->setStyleSheet(
        "QLabel {"
        "background: #12251f;"
        "border: 1px solid #245f4b;"
        "border-radius: 10px;"
        "color: #8ee6bd;"
        "font-weight: 800;"
        "padding: 12px 16px;"
        "}"
        );
    mainLayout->addWidget(alertBanner);

    // ===== CHART CONTROLS =====
    QHBoxLayout* controlsRow = new QHBoxLayout();
    controlsRow->setSpacing(12);

    QLabel* signalLabel = new QLabel("Signal:");
    signalLabel->setStyleSheet("font-weight: 600;");

    signalSelector = new QComboBox();
    signalSelector->addItems({
        "Frequency",
        "ROCOF",
        "Voltage Magnitude",
        "Voltage Angle",
        "Current Magnitude",
        "Current Angle"
    });
    signalSelector->setMinimumWidth(180);

    QLabel* channelLabel = new QLabel("Channel:");
    channelLabel->setStyleSheet("font-weight: 600;");
    channelLabel->hide();

    channelSelector = new QComboBox();
    channelSelector->addItems({"V1", "V2", "V3"});
    channelSelector->setMinimumWidth(100);
    channelSelector->hide();

    pauseBtn = new QPushButton("Pause");
    pauseBtn->setCheckable(true);
    pauseBtn->setStyleSheet(
        "QPushButton { background-color: #d9a441; color: #111827; }"
        "QPushButton:hover { background-color: #c49439; }"
        "QPushButton:checked { background-color: #21a67a; color: white; }"
        );

    exportBtn = new QPushButton("Export CSV");

    controlsRow->addWidget(signalLabel);
    controlsRow->addWidget(signalSelector);
    controlsRow->addWidget(channelLabel);
    controlsRow->addWidget(channelSelector);
    controlsRow->addStretch();
    controlsRow->addWidget(pauseBtn);
    controlsRow->addWidget(exportBtn);

    QVBoxLayout* controlsSectionLayout = new QVBoxLayout();
    controlsSectionLayout->setSpacing(12);
    controlsSectionLayout->addLayout(controlsRow);

    // ===== PHASOR CHECKBOXES =====
    QHBoxLayout* checkLayout = new QHBoxLayout();
    checkLayout->setSpacing(16);

    v1Check = new QCheckBox("VA");
    v2Check = new QCheckBox("VB");
    v3Check = new QCheckBox("VC");
    phasorCheckLayout = checkLayout;
    phasorChecks = { v1Check, v2Check, v3Check };

    v1Check->setChecked(true);
    v2Check->setChecked(true);
    v3Check->setChecked(true);

    v1Check->setStyleSheet("color: #ef6f7b;");
    v2Check->setStyleSheet("color: #5d9cec;");
    v3Check->setStyleSheet("color: #28c081;");

    checkLayout->addWidget(v1Check);
    checkLayout->addWidget(v2Check);
    checkLayout->addWidget(v3Check);
    checkLayout->addStretch();

    controlsSectionLayout->addLayout(checkLayout);
    mainLayout->addWidget(createSection("Chart Controls", controlsSectionLayout));

    // ===== CHART =====
    chart = new QChart();
    chart->setTitle("");
    chart->setBackgroundBrush(QBrush(QColor("#111827")));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor("#151d2b")));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setMargins(QMargins(0, 0, 0, 0));

    // CRITICAL: Disable animations to prevent glitches
    chart->setAnimationOptions(QChart::NoAnimation);

    // Create series
    series = new QLineSeries();
    seriesV1 = new QLineSeries();
    seriesV2 = new QLineSeries();
    seriesV3 = new QLineSeries();

    seriesV1->setName("V1");
    seriesV2->setName("V2");
    seriesV3->setName("V3");
    phasorSeries = { seriesV1, seriesV2, seriesV3 };

    // Set colors and pen width
    QPen pen1(QColor("#ef6f7b")); // Red
    pen1.setWidth(2);
    seriesV1->setPen(pen1);

    QPen pen2(QColor("#5d9cec")); // Blue
    pen2.setWidth(2);
    seriesV2->setPen(pen2);

    QPen pen3(QColor("#28c081")); // Green
    pen3.setWidth(2);
    seriesV3->setPen(pen3);

    QPen penSingle(QColor("#5d9cec"));
    penSingle.setWidth(2);
    series->setPen(penSingle);

    chart->addSeries(series);
    chart->addSeries(seriesV1);
    chart->addSeries(seriesV2);
    chart->addSeries(seriesV3);

    // Create axes
    axisX = new QValueAxis();
    axisY = new QValueAxis();

    axisX->setLabelsColor(QColor("#cfd7e3"));
    axisY->setLabelsColor(QColor("#cfd7e3"));
    axisX->setGridLineColor(QColor("#2c374a"));
    axisY->setGridLineColor(QColor("#2c374a"));
    axisX->setTitleText("Time (s)");
    axisX->setTitleBrush(QBrush(QColor("#cfd7e3")));
    axisY->setTitleBrush(QBrush(QColor("#cfd7e3")));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    // Attach all series to axes
    series->attachAxis(axisX);
    series->attachAxis(axisY);
    seriesV1->attachAxis(axisX);
    seriesV1->attachAxis(axisY);
    seriesV2->attachAxis(axisX);
    seriesV2->attachAxis(axisY);
    seriesV3->attachAxis(axisX);
    seriesV3->attachAxis(axisY);

    // Set initial ranges
    axisX->setRange(0, 10);
    axisY->setRange(49.5, 50.5);

    // Legend
    chart->legend()->setVisible(true);
    chart->legend()->setLabelColor(QColor("#cfd7e3"));
    chart->legend()->setAlignment(Qt::AlignTop);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(400);
    chartView->setStyleSheet("background: transparent; border: none;");
    connectSeriesHover(series, "Signal");
    connectSeriesHover(seriesV1, "V1");
    connectSeriesHover(seriesV2, "V2");
    connectSeriesHover(seriesV3, "V3");

    QVBoxLayout* chartSectionLayout = new QVBoxLayout();
    chartSectionLayout->addWidget(chartView);
    mainLayout->addWidget(createSection("Live Trend", chartSectionLayout));

    // ===== TABLE SECTION =====
    table = new QTableWidget();
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({"System Time", "PMU Time", "Latency (ms)", "PMU ID", "Frequency (Hz)", "ROCOF"});
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setMinimumHeight(300);
    table->setMaximumHeight(500);

    QVBoxLayout* tableSectionLayout = new QVBoxLayout();
    tableSectionLayout->addWidget(table);
    mainLayout->addWidget(createSection("Live Data Stream", tableSectionLayout));

    eventTable = new QTableWidget();
    eventTable->setColumnCount(6);
    eventTable->setHorizontalHeaderLabels({"Detected At", "Event", "Value", "Threshold", "Latency (ms)", "Detail"});
    eventTable->setAlternatingRowColors(true);
    eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    eventTable->verticalHeader()->setVisible(false);
    eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    eventTable->setMinimumHeight(180);
    eventTable->setMaximumHeight(320);

    QVBoxLayout* eventSectionLayout = new QVBoxLayout();
    eventSectionLayout->addWidget(eventTable);
    mainLayout->addWidget(createSection("Event Log", eventSectionLayout));

    // ===== SIGNAL CONNECTIONS =====

    // Signal selector changed
    connect(signalSelector, &QComboBox::currentTextChanged, this, &DataWindow::onSignalChanged);
    channelSelector->hide();

    // Visibility control for checkboxes and channel selector
    connect(signalSelector, &QComboBox::currentTextChanged, this, [this](const QString&) {
        updatePhasorControls();
    });

    // Pause/Resume
    connect(pauseBtn, &QPushButton::toggled, this, [this](bool checked) {
        paused = checked;
        pauseBtn->setText(paused ? "Resume" : "Pause");
        statusIndicator->setText(paused ? "PAUSED" : "LIVE");
        statusIndicator->setStyleSheet(paused ? "color: #d9a441; font-weight: 800; background: transparent;" : "color: #28c081; font-weight: 800; background: transparent;");

        if (!paused) {
            rebuildChartFromHistory();
        }
    });

    // Export CSV
    connect(exportBtn, &QPushButton::clicked, this, &DataWindow::onExportClicked);

    // Checkbox visibility updates
    connect(v1Check, &QCheckBox::toggled, this, [this](bool checked) {
        seriesV1->setVisible(checked);
        if (isPhasorSignal(signalSelector->currentText()))
            rebuildChartFromHistory();
    });

    connect(v2Check, &QCheckBox::toggled, this, [this](bool checked) {
        seriesV2->setVisible(checked);
        if (isPhasorSignal(signalSelector->currentText()))
            rebuildChartFromHistory();
    });

    connect(v3Check, &QCheckBox::toggled, this, [this](bool checked) {
        seriesV3->setVisible(checked);
        if (isPhasorSignal(signalSelector->currentText()))
            rebuildChartFromHistory();
    });

    // Table update timer
    tableTimer = new QTimer(this);
    connect(tableTimer, &QTimer::timeout, this, &DataWindow::updateTable);
    tableTimer->start(1000); // Update table every second

    // Initial signal setup
    applyConfigToUi();
    onSignalChanged(signalSelector->currentText());
    updatePhasorControls();

    resize(1200, 850);
}

void DataWindow::onNewFrame(const PMUFrame& frame)
{
    lastFrame = frame;

    if (!paused) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - lastMetricUiUpdateMs >= 250) {
            updateMetrics(frame);
            updateTimeSync(frame);
            lastMetricUiUpdateMs = nowMs;
        }
        evaluateFrameEvents(frame);
        updateChart(frame);
    }
}

void DataWindow::updateMetrics(const PMUFrame& frame)
{
    freqLabel->setText(frame.hasFrequency
                           ? QString("Frequency\n%1 Hz").arg(frame.frequency, 0, 'f', 4)
                           : "Frequency\nNot sent");
    rocofLabel->setText(frame.hasRocof
                            ? QString("ROCOF\n%1 Hz/s").arg(frame.rocof, 0, 'f', 4)
                            : "ROCOF\nNot sent");
}

void DataWindow::updateTimeSync(const PMUFrame& frame)
{
    pmuTimeLabel->setText("PMU Time\n" + isoTimeFromPmuFrame(frame));
    latencyLabel->setText(QString("Latency\n%1 ms").arg(frame.latencyMs, 0, 'f', 2));
}

void DataWindow::applyConfigToUi()
{
    QString currentSignal = signalSelector->currentText();

    signalSelector->blockSignals(true);
    signalSelector->clear();
    signalSelector->addItem("Frequency");
    signalSelector->addItem("ROCOF");
    if (hasConfigPhasorType(currentConfig, true)) {
        signalSelector->addItem("Voltage Magnitude");
        signalSelector->addItem("Voltage Angle");
    }
    if (hasConfigPhasorType(currentConfig, false)) {
        signalSelector->addItem("Current Magnitude");
        signalSelector->addItem("Current Angle");
    }

    int signalIndex = signalSelector->findText(currentSignal);
    signalSelector->setCurrentIndex(signalIndex >= 0 ? signalIndex : 0);
    signalSelector->blockSignals(false);

    const QStringList colors = { "#ef6f7b", "#5d9cec", "#28c081", "#d9a441", "#b985ff", "#4dd0e1" };
    while (phasorSeries.size() < currentConfig.phasorCount) {
        QLineSeries* extraSeries = new QLineSeries();
        chart->addSeries(extraSeries);
        extraSeries->attachAxis(axisX);
        extraSeries->attachAxis(axisY);
        phasorSeries.push_back(extraSeries);
        connectSeriesHover(extraSeries, QString("PH%1").arg(phasorSeries.size()));
    }

    for (int i = 0; i < phasorSeries.size(); ++i) {
        QString label = currentConfig.phasorLabels.size() > i
                            ? QString::fromStdString(currentConfig.phasorLabels[i])
                            : QString("PH%1").arg(i + 1);
        phasorSeries[i]->setName(label);
        QPen pen(QColor(colors[i % colors.size()]));
        pen.setWidth(2);
        phasorSeries[i]->setPen(pen);
        phasorSeries[i]->setVisible(i < currentConfig.phasorCount &&
                                    isPhasorSignal(signalSelector->currentText()) &&
                                    configPhasorMatchesSignal(currentConfig, i, signalSelector->currentText()));
    }

    while (phasorChecks.size() < currentConfig.phasorCount) {
        QCheckBox* check = new QCheckBox();
        check->setChecked(true);
        phasorChecks.push_back(check);
        phasorCheckLayout->insertWidget(static_cast<int>(phasorChecks.size()) - 1, check);
        connect(check, &QCheckBox::toggled, this, [this]() {
            if (isPhasorSignal(signalSelector->currentText()))
                rebuildChartFromHistory();
        });
    }

    for (int i = 0; i < phasorChecks.size(); ++i) {
        QString label = currentConfig.phasorLabels.size() > i
                            ? QString::fromStdString(currentConfig.phasorLabels[i])
                            : QString("PH%1").arg(i + 1);
        phasorChecks[i]->setText(label);
        phasorChecks[i]->setStyleSheet(QString("color: %1;").arg(colors[i % colors.size()]));
        phasorChecks[i]->setVisible(isPhasorSignal(signalSelector->currentText()) &&
                                    i < currentConfig.phasorCount &&
                                    configPhasorMatchesSignal(currentConfig, i, signalSelector->currentText()));
    }

    updatePhasorControls();

    if (table) {
        QStringList headers = {"System Time", "PMU Time", "Latency (ms)", "PMU ID", "Frequency (Hz)", "ROCOF"};
        for (int i = 0; i < currentConfig.phasorCount; ++i) {
            QString label = currentConfig.phasorLabels.size() > i
                                ? QString::fromStdString(currentConfig.phasorLabels[i])
                                : QString("PH%1").arg(i + 1);
            headers << (label + " Mag") << (label + " Angle");
        }
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);
    }
}

void DataWindow::updatePhasorControls()
{
    const QString signal = signalSelector->currentText();
    const bool phasorMode = isPhasorSignal(signal);
    channelSelector->hide();

    for (int i = 0; i < phasorChecks.size(); ++i)
    {
        const bool visible = phasorMode &&
                             i < currentConfig.phasorCount &&
                             configPhasorMatchesSignal(currentConfig, i, signal);
        phasorChecks[i]->setVisible(visible);
    }

    for (int i = 0; i < phasorSeries.size(); ++i)
    {
        const bool visible = phasorMode &&
                             i < currentConfig.phasorCount &&
                             configPhasorMatchesSignal(currentConfig, i, signal) &&
                             (phasorChecks.size() <= i || phasorChecks[i]->isChecked());
        phasorSeries[i]->setVisible(visible);
    }
}

void DataWindow::updateChart(const PMUFrame& frame)
{
    if (!dataManager)
        return;

    std::size_t totalFrames = dataManager->getTotalFrameCount();
    if (totalFrames == 0)
        return;

    double currentTime = sampleTimeForFrame(frame, static_cast<int>(totalFrames - 1));
    appendFrameToSeries(frame, currentTime);

    double windowSize = 10.0;
    double minX = qMax(0.0, currentTime - windowSize);
    double maxX = qMax(windowSize, currentTime);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool updateViewport = nowMs - lastChartViewportUpdateMs >= 100;
    if (updateViewport) {
        axisX->setRange(minX, maxX);
        lastChartViewportUpdateMs = nowMs;
    }

    if (updateViewport && ++chartFrameCounter % 3 == 0)
        updateYAxisRange(minX, maxX);
}

double DataWindow::sampleTimeForIndex(int index) const
{
    int rate = currentConfig.dataRate > 0 ? currentConfig.dataRate : 50;
    return static_cast<double>(index) / static_cast<double>(rate);
}

double DataWindow::sampleTimeForFrame(const PMUFrame& frame, int fallbackIndex)
{
    if (frame.systemUnixMs == 0)
        return sampleTimeForIndex(fallbackIndex);

    if (chartOriginUnixMs < 0.0)
        chartOriginUnixMs = static_cast<double>(frame.systemUnixMs);

    return (static_cast<double>(frame.systemUnixMs) - chartOriginUnixMs) / 1000.0;
}

void DataWindow::appendFrameToSeries(const PMUFrame& frame, double xValue)
{
    QString selectedSignal = signalSelector->currentText();

    if (isPhasorSignal(selectedSignal)) {
        for (int i = 0; i < frame.phasors.size() && i < phasorSeries.size(); ++i) {
            if (!phasorMatchesSignal(frame.phasors[i], selectedSignal))
                continue;

            appendSeriesPoint(phasorSeries[i], xValue, phasorValueForSignal(frame.phasors[i], selectedSignal));
            if (phasorSeries[i]->count() > MAX_POINTS)
                phasorSeries[i]->remove(0);
        }

        series->setVisible(false);
        for (int i = 0; i < phasorSeries.size(); ++i) {
            bool checked = phasorChecks.size() > i ? phasorChecks[i]->isChecked() : true;
            phasorSeries[i]->setVisible(i < frame.phasors.size() &&
                                        checked &&
                                        phasorMatchesSignal(frame.phasors[i], selectedSignal));
        }

    } else {
        double value = valueFromFrame(frame, selectedSignal, 0);

        bool valueAvailable = true;
        if (selectedSignal == "Frequency")
            valueAvailable = frame.hasFrequency;
        else if (selectedSignal == "ROCOF")
            valueAvailable = frame.hasRocof;

        if (valueAvailable) {
            appendSeriesPoint(series, xValue, value);
            if (series->count() > MAX_POINTS) {
                series->remove(0);
            }
        }

        series->setVisible(true);
        for (QLineSeries* phasorLine : phasorSeries)
            phasorLine->setVisible(false);
    }
}

void DataWindow::appendSeriesPoint(QLineSeries* targetSeries, double xValue, double yValue)
{
    if (lastSeriesX.contains(targetSeries)) {
        const double lastX = lastSeriesX.value(targetSeries);
        const double expectedInterval = 1.0 / static_cast<double>(qMax(1, currentConfig.dataRate));
        const bool timeMovedBackward = xValue <= lastX;
        const bool timeGap = (xValue - lastX) > (expectedInterval * 3.0);
        const bool lastWasFinite = lastSeriesFinite.value(targetSeries, false);

        if (lastWasFinite && (timeMovedBackward || timeGap)) {
            targetSeries->append(xValue, std::numeric_limits<double>::quiet_NaN());
        }
    }

    targetSeries->append(xValue, yValue);
    lastSeriesX[targetSeries] = xValue;
    lastSeriesFinite[targetSeries] = std::isfinite(yValue);
}

void DataWindow::updateYAxisRange(double minX, double maxX)
{
    QString selectedSignal = signalSelector->currentText();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    auto scanSeries = [&](QLineSeries* targetSeries) {
        if (!targetSeries || !targetSeries->isVisible())
            return;

        const auto points = targetSeries->points();
        for (const QPointF& point : points) {
            if (point.x() < minX || point.x() > maxX || !std::isfinite(point.y()))
                continue;

            minY = qMin(minY, point.y());
            maxY = qMax(maxY, point.y());
        }
    };

    if (isPhasorSignal(selectedSignal)) {
        for (QLineSeries* phasorLine : phasorSeries)
            scanSeries(phasorLine);
    } else {
        scanSeries(series);
    }

    if (minY == std::numeric_limits<double>::max())
        return;

    double span = qMax(0.0001, maxY - minY);
    double padding = qMax(span * 0.15, 0.5);

    if (selectedSignal == "Frequency")
        padding = qMax(span * 0.30, 0.05);
    else if (selectedSignal == "ROCOF")
        padding = qMax(span * 0.30, 0.1);
    else if (isAngleSignal(selectedSignal))
        padding = qMax(span * 0.10, 5.0);

    axisY->setRange(minY - padding, maxY + padding);
}

void DataWindow::connectSeriesHover(QLineSeries* targetSeries, const QString& label)
{
    connect(targetSeries, &QLineSeries::hovered, this, [this, label](const QPointF& point, bool state) {
        if (state) {
            QLineSeries* hoveredSeries = qobject_cast<QLineSeries*>(sender());
            QString displayLabel = hoveredSeries && !hoveredSeries->name().isEmpty() ? hoveredSeries->name() : label;
            showPointTooltip(displayLabel, point);
        } else {
            QToolTip::hideText();
        }
    });
}

void DataWindow::showPointTooltip(const QString& label, const QPointF& point)
{
    QString unit = axisY->titleText();
    QString tooltip = QString("%1\nTime: %2 s\nValue: %3")
                          .arg(label)
                          .arg(point.x(), 0, 'f', 3)
                          .arg(point.y(), 0, 'f', 4);

    if (!unit.isEmpty())
        tooltip += "\n" + unit;

    QPointF chartPoint = chart->mapToPosition(point);
    QPoint globalPoint = chartView->viewport()->mapToGlobal(chartPoint.toPoint() + QPoint(12, -18));
    QToolTip::showText(globalPoint, tooltip, chartView);
}

QString DataWindow::isoTimeFromUnixMs(uint64_t unixMs) const
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(unixMs), QTimeZone::UTC).toString(Qt::ISODateWithMs);
}

QString DataWindow::isoTimeFromPmuFrame(const PMUFrame& frame) const
{
    qint64 pmuMs = static_cast<qint64>(frame.pmuTimestampSeconds * 1000.0);
    return QDateTime::fromMSecsSinceEpoch(pmuMs, QTimeZone::UTC).toString(Qt::ISODateWithMs);
}

void DataWindow::evaluateFrameEvents(const PMUFrame& frame)
{
    if (frame.hasFrequency && frame.frequency < UNDER_FREQUENCY_THRESHOLD)
    {
        triggerEvent("Under Frequency",
                     "Frequency dropped below operating threshold",
                     frame.frequency,
                     UNDER_FREQUENCY_THRESHOLD,
                     frame);
    }

    if (frame.hasRocof && std::fabs(frame.rocof) > HIGH_ROCOF_THRESHOLD)
    {
        triggerEvent("High ROCOF",
                     "Rate of change of frequency exceeded threshold",
                     frame.rocof,
                     HIGH_ROCOF_THRESHOLD,
                     frame);
    }

    for (int i = 0; i < frame.phasors.size(); ++i)
    {
        if (frame.phasors[i].isVoltage && frame.phasors[i].magnitude < VOLTAGE_DIP_THRESHOLD)
        {
            QString label = QString::fromStdString(frame.phasors[i].label);
            triggerEvent(QString("Voltage Dip %1").arg(label),
                         QString("Voltage magnitude on %1 dropped below threshold").arg(label),
                         frame.phasors[i].magnitude,
                         VOLTAGE_DIP_THRESHOLD,
                         frame);
        }
    }
}

void DataWindow::triggerEvent(const QString& type, const QString& detail, double value, double threshold, const PMUFrame& frame)
{
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (lastEventMsByType.contains(type) && nowMs - lastEventMsByType[type] < EVENT_COOLDOWN_MS)
        return;

    lastEventMsByType[type] = nowMs;

    EventLogEntry entry;
    entry.isoTime = isoTimeFromUnixMs(frame.systemUnixMs);
    entry.type = type;
    entry.detail = detail;
    entry.value = value;
    entry.threshold = threshold;
    entry.latencyMs = frame.latencyMs;
    eventLog.push_back(entry);

    alertBanner->setText(QString("ACTIVE EVENT: %1  |  Value: %2  |  Threshold: %3")
                             .arg(type)
                             .arg(value, 0, 'f', 4)
                             .arg(threshold, 0, 'f', 4));
    alertBanner->setStyleSheet(
        "QLabel {"
        "background: #3a151d;"
        "border: 1px solid #ef6f7b;"
        "border-radius: 10px;"
        "color: #ffd8dd;"
        "font-weight: 800;"
        "padding: 12px 16px;"
        "}"
        );

    int row = eventTable->rowCount();
    eventTable->insertRow(row);
    eventTable->setItem(row, 0, new QTableWidgetItem(entry.isoTime));
    eventTable->setItem(row, 1, new QTableWidgetItem(entry.type));
    eventTable->setItem(row, 2, new QTableWidgetItem(QString::number(entry.value, 'f', 4)));
    eventTable->setItem(row, 3, new QTableWidgetItem(QString::number(entry.threshold, 'f', 4)));
    eventTable->setItem(row, 4, new QTableWidgetItem(QString::number(entry.latencyMs, 'f', 2)));
    eventTable->setItem(row, 5, new QTableWidgetItem(entry.detail));
    eventTable->scrollToBottom();
}

void DataWindow::rebuildChartFromHistory()
{
    clearAllSeries();

    if (!dataManager)
        return;

    const auto history = dataManager->getFramesSnapshot();
    std::size_t totalFrames = dataManager->getTotalFrameCount();
    int startIndex = qMax(0, static_cast<int>(history.size()) - MAX_POINTS);
    int firstSampleIndex = qMax(0, static_cast<int>(totalFrames) - static_cast<int>(history.size()));
    chartOriginUnixMs = history.empty() || history.front().systemUnixMs == 0
                            ? -1.0
                            : static_cast<double>(history.front().systemUnixMs);

    for (int i = startIndex; i < history.size(); ++i)
    {
        appendFrameToSeries(history[i], sampleTimeForFrame(history[i], firstSampleIndex + i));
    }

    if (!history.empty())
    {
        lastFrame = history.back();
        updateMetrics(lastFrame);
        updateTimeSync(lastFrame);

        double latestTime = sampleTimeForFrame(history.back(), qMax(0, static_cast<int>(totalFrames) - 1));
        double windowSize = 10.0;
        double minX = qMax(0.0, latestTime - windowSize);
        double maxX = qMax(windowSize, latestTime);
        axisX->setRange(minX, maxX);
        updateYAxisRange(minX, maxX);
    }
    else
    {
        axisX->setRange(0, 10);
    }
}

void DataWindow::updateTable()
{
    if (!dataManager) return;
    if (paused || lastFrame.systemUnixMs == 0) return;

    int row = table->rowCount();
    table->insertRow(row);

    int col = 0;
    table->setItem(row, col++, new QTableWidgetItem(isoTimeFromUnixMs(lastFrame.systemUnixMs)));
    table->setItem(row, col++, new QTableWidgetItem(isoTimeFromPmuFrame(lastFrame)));
    table->setItem(row, col++, new QTableWidgetItem(QString::number(lastFrame.latencyMs, 'f', 2)));
    table->setItem(row, col++, new QTableWidgetItem(QString::number(lastFrame.pmuID)));
    table->setItem(row, col++, new QTableWidgetItem(lastFrame.hasFrequency ? QString::number(lastFrame.frequency, 'f', 4) : "Not sent"));
    table->setItem(row, col++, new QTableWidgetItem(lastFrame.hasRocof ? QString::number(lastFrame.rocof, 'f', 4) : "Not sent"));

    for (const auto& phasor : lastFrame.phasors) {
        table->setItem(row, col++, new QTableWidgetItem(QString::number(phasor.magnitude, 'f', 2)));
        table->setItem(row, col++, new QTableWidgetItem(QString::number(phasor.angleDeg, 'f', 2)));
    }

    table->scrollToBottom();

    // Limit table size
    if (table->rowCount() > 500) {
        table->removeRow(0);
    }
}

void DataWindow::onSignalChanged(const QString& signal)
{
    if (signal == "Frequency") {
        axisY->setRange(49.5, 50.5);
        axisY->setTitleText("Frequency (Hz)");
    } else if (signal == "ROCOF") {
        axisY->setRange(-1.0, 1.0);
        axisY->setTitleText("ROCOF (Hz/s)");
    } else if (signal == "Voltage Magnitude") {
        axisY->setRange(220, 240);
        axisY->setTitleText("Voltage Magnitude");
    } else if (signal == "Current Magnitude") {
        axisY->setRange(0, 20);
        axisY->setTitleText("Current Magnitude");
    } else if (signal == "Voltage Angle" || signal == "Current Angle") {
        axisY->setRange(-180, 180);
        axisY->setTitleText("Angle (deg)");
    }

    updatePhasorControls();
    rebuildChartFromHistory();
}

void DataWindow::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Export Research Data", "", "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    auto csv = [](const QString& value) {
        QString escaped = value;
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    };

    out << "# PDC Monitor Research Export\n";
    out << "# Exported At," << csv(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)) << "\n";
    out << "# Device," << csv(deviceName) << "\n";
    out << "# PMU ID," << currentConfig.pmuID << "\n";
    out << "# Station," << csv(QString::fromStdString(currentConfig.stationName)) << "\n";
    out << "# Sampling Rate," << currentConfig.dataRate << "\n";
    out << "# Time Base," << currentConfig.timeBase << "\n";
    out << "# Phasors," << currentConfig.phasorCount << "\n\n";

    out << "[DATA]\n";
    out << "System Time ISO,PMU Time ISO,Latency ms,PMU ID,Frequency Hz,ROCOF Hz/s";
    for (int i = 0; i < currentConfig.phasorCount; ++i) {
        QString label = currentConfig.phasorLabels.size() > i
                            ? QString::fromStdString(currentConfig.phasorLabels[i])
                            : QString("PH%1").arg(i + 1);
        out << "," << label << " Magnitude," << label << " Angle";
    }
    out << "\n";

    const auto frames = dataManager ? dataManager->getFramesSnapshot() : std::deque<PMUFrame>();
    for (const auto& frame : frames)
    {
        out << csv(isoTimeFromUnixMs(frame.systemUnixMs)) << ","
            << csv(isoTimeFromPmuFrame(frame)) << ","
            << QString::number(frame.latencyMs, 'f', 3) << ","
            << frame.pmuID << ","
            << (frame.hasFrequency ? QString::number(frame.frequency, 'f', 6) : "Not sent") << ","
            << (frame.hasRocof ? QString::number(frame.rocof, 'f', 6) : "Not sent");

        for (int i = 0; i < currentConfig.phasorCount; ++i)
        {
            if (frame.phasors.size() > i)
            {
                out << "," << QString::number(frame.phasors[i].magnitude, 'f', 6)
                    << "," << QString::number(frame.phasors[i].angleDeg, 'f', 6);
            }
            else
            {
                out << ",,";
            }
        }

        out << "\n";
    }

    out << "\n[EVENTS]\n";
    out << "Detected At ISO,Event,Value,Threshold,Latency ms,Detail\n";
    for (const auto& event : eventLog)
    {
        out << csv(event.isoTime) << ","
            << csv(event.type) << ","
            << QString::number(event.value, 'f', 6) << ","
            << QString::number(event.threshold, 'f', 6) << ","
            << QString::number(event.latencyMs, 'f', 3) << ","
            << csv(event.detail) << "\n";
    }

    file.close();
}

void DataWindow::clearAllSeries()
{
    series->clear();
    for (QLineSeries* phasorLine : phasorSeries)
        phasorLine->clear();
    lastSeriesX.clear();
    lastSeriesFinite.clear();
    chartFrameCounter = 0;
}
