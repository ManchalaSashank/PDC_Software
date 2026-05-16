#include "pdc_dashboardwindow.h"

#include <QDateTime>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollArea>
#include <QTableWidgetItem>
#include <QTimeZone>
#include <QToolTip>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace
{
QFrame* makeSection(const QString& title, QWidget* content)
{
    QFrame* frame = new QFrame();
    frame->setObjectName("sectionBox");

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(12);

    auto* label = new QLabel(title);
    label->setObjectName("sectionTitle");
    layout->addWidget(label);
    layout->addWidget(content);

    return frame;
}

QLabel* makeMetricLabel(const QString& title)
{
    auto* label = new QLabel(title + "\n--");
    label->setObjectName("metricCard");
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(88);
    return label;
}

QTableWidgetItem* item(const QString& text)
{
    auto* tableItem = new QTableWidgetItem(text);
    tableItem->setTextAlignment(Qt::AlignCenter);
    return tableItem;
}

bool pdcMagnitudeMode(const QString& mode)
{
    return mode == "Voltage Magnitude" || mode == "Current Magnitude";
}

bool pdcAngleMode(const QString& mode)
{
    return mode == "Voltage Angle" || mode == "Current Angle";
}

QString pdcPhasorTypeLabel(const QString& label, bool isVoltage)
{
    const QString upper = label.trimmed().toUpper();
    if (upper.startsWith("V"))
        return "Voltage";
    if (upper.startsWith("I"))
        return "Current";
    return isVoltage ? "Voltage" : "Phasor";
}
}

PdcDashboardWindow::PdcDashboardWindow(const QVector<PdcPmuSource>& inputSources, QWidget* parent)
    : QWidget(parent)
{
    for (const PdcPmuSource& input : inputSources)
    {
        SourceState state;
        state.name = input.name;
        state.worker = input.worker;
        sources.push_back(state);
    }

    buildUi();
    configureChart();

    elapsedTimer.start();
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &PdcDashboardWindow::updateDashboard);
    refreshTimer->start(200);

    updateDashboard();
    resize(1350, 900);
}

void PdcDashboardWindow::buildUi()
{
    setWindowTitle("Central PDC Dashboard");
    setStyleSheet(R"(
QWidget {
    background: #0d1117;
    color: #edf4fb;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 13px;
}

QLabel#title {
    background: transparent;
    color: #f8fafc;
    font-size: 26px;
    font-weight: 800;
}

QLabel#subtitle {
    background: transparent;
    color: #91a6ba;
    font-size: 12px;
    font-weight: 600;
}

QLabel#metricCard {
    background: #151c27;
    border: 1px solid #2c4054;
    border-radius: 8px;
    color: #edf4fb;
    font-size: 17px;
    font-weight: 800;
    padding: 14px;
}

QFrame#sectionBox {
    background: #151c27;
    border: 1px solid #2c4054;
    border-radius: 8px;
}

QLabel#sectionTitle {
    background: transparent;
    color: #f8fafc;
    font-size: 15px;
    font-weight: 800;
}

QTableWidget {
    background: #101827;
    alternate-background-color: #151f2d;
    gridline-color: #2c4054;
    border: 1px solid #2c4054;
    border-radius: 8px;
    color: #edf4fb;
    selection-background-color: #244a78;
}

QTableWidget::item {
    padding: 6px;
}

QHeaderView::section {
    background: #1a2534;
    border: none;
    color: #edf4fb;
    font-weight: 800;
    padding: 8px;
}

QScrollArea {
    border: none;
}

QScrollBar:vertical {
    background: transparent;
    width: 11px;
    margin: 4px 2px 4px 2px;
}

QScrollBar::handle:vertical {
    background: #34445a;
    border-radius: 5px;
    min-height: 34px;
}

QScrollBar::handle:vertical:hover {
    background: #4b6381;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    height: 0;
    background: transparent;
}

QWidget#graphControls {
    background: #101827;
    border: 1px solid #26384c;
    border-radius: 8px;
}

QComboBox {
    background: #101827;
    border: 1px solid #2c4054;
    border-radius: 8px;
    color: #edf4fb;
    min-height: 24px;
    padding: 8px 10px;
}

QComboBox QAbstractItemView {
    background: #111827;
    border: 1px solid #2c4054;
    color: #edf4fb;
    selection-background-color: #2563eb;
    outline: none;
}

QComboBox:focus {
    border-color: #5d9cec;
}

QComboBox:hover {
    border-color: #60a5fa;
    background: #122035;
}

QPushButton {
    background: #2563eb;
    border: 1px solid #4d7df0;
    border-radius: 8px;
    color: #f8fafc;
    font-weight: 800;
    padding: 9px 14px;
}

QPushButton:hover {
    background: #1d4ed8;
}

QPushButton:pressed {
    background: #1e40af;
}
)");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    root->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(18);

    auto* title = new QLabel("Central PDC Dashboard");
    title->setObjectName("title");
    auto* subtitle = new QLabel("System-level monitoring, time-aligned PMU comparison, and unified grid event view");
    subtitle->setObjectName("subtitle");
    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    auto* metricsRow = new QHBoxLayout();
    metricsRow->setSpacing(16);
    connectedValue = makeMetricLabel("Connected PMUs");
    streamingValue = makeMetricLabel("Streaming PMUs");
    averageFrequencyValue = makeMetricLabel("Average Frequency");
    systemStatusValue = makeMetricLabel("Grid Status");
    metricsRow->addWidget(connectedValue);
    metricsRow->addWidget(streamingValue);
    metricsRow->addWidget(averageFrequencyValue);
    metricsRow->addWidget(systemStatusValue);
    mainLayout->addLayout(metricsRow);

    auto* graphControls = new QWidget();
    graphControls->setObjectName("graphControls");
    auto* graphControlsLayout = new QHBoxLayout(graphControls);
    graphControlsLayout->setContentsMargins(12, 10, 12, 10);
    graphControlsLayout->setSpacing(12);

    auto* graphModeLabel = new QLabel("Graph:");
    graphModeLabel->setObjectName("subtitle");
    graphModeSelector = new QComboBox();
    graphModeSelector->addItems({"Frequency", "ROCOF", "Voltage Magnitude", "Voltage Angle", "Current Magnitude", "Current Angle"});
    graphModeSelector->setMinimumWidth(190);

    graphChannelLabel = new QLabel("Phasor:");
    graphChannelLabel->setObjectName("subtitle");
    graphChannelSelector = new QComboBox();
    graphChannelSelector->setMinimumWidth(120);

    graphControlsLayout->addWidget(graphModeLabel);
    graphControlsLayout->addWidget(graphModeSelector);
    graphControlsLayout->addWidget(graphChannelLabel);
    graphControlsLayout->addWidget(graphChannelSelector);
    graphControlsLayout->addStretch();

    frequencyChart = new QChart();
    frequencyChartView = new QChartView(frequencyChart);
    frequencyChartView->setRenderHint(QPainter::Antialiasing);
    frequencyChartView->setMinimumHeight(360);

    auto* graphContent = new QWidget();
    auto* graphLayout = new QVBoxLayout(graphContent);
    graphLayout->setContentsMargins(0, 0, 0, 0);
    graphLayout->setSpacing(12);
    graphLayout->addWidget(graphControls);
    graphLayout->addWidget(frequencyChartView);
    mainLayout->addWidget(makeSection("Central Trend Comparison", graphContent));

    connect(graphModeSelector, &QComboBox::currentTextChanged, this, [this]() {
        for (SourceState& source : sources)
        {
            if (source.frequencySeries)
                source.frequencySeries->clear();
        }
        lastSeriesX.clear();
        lastSeriesFinite.clear();
        chartRangeCounter = 0;
        chartOriginMs = -1.0;
        graphChannelSignature.clear();
        refreshGraphControls();
        axisX->setRange(0, 10);
    });
    connect(graphChannelSelector, &QComboBox::currentTextChanged, this, [this]() {
        for (SourceState& source : sources)
        {
            if (source.frequencySeries)
                source.frequencySeries->clear();
        }
        lastSeriesX.clear();
        lastSeriesFinite.clear();
        chartRangeCounter = 0;
        chartOriginMs = -1.0;
        axisX->setRange(0, 10);
    });

    statusTable = new QTableWidget();
    statusTable->setColumnCount(8);
    statusTable->setHorizontalHeaderLabels({"PMU", "Frequency", "ROCOF", "Status", "Frames", "Frame Rate", "Packet Quality", "Latency"});
    statusTable->setAlternatingRowColors(true);
    statusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    statusTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    statusTable->verticalHeader()->setVisible(false);
    statusTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    statusTable->setMinimumHeight(220);
    mainLayout->addWidget(makeSection("Grid Status and Connection Health", statusTable));

    phasorTable = new QTableWidget();
    phasorTable->setColumnCount(5);
    phasorTable->setHorizontalHeaderLabels({"PMU", "Channel", "Type", "Magnitude", "Angle"});
    phasorTable->setAlternatingRowColors(true);
    phasorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    phasorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    phasorTable->verticalHeader()->setVisible(false);
    phasorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    phasorTable->setMinimumHeight(260);
    mainLayout->addWidget(makeSection("Synchrophasor Channels", phasorTable));

    comparisonTable = new QTableWidget();
    comparisonTable->setColumnCount(6);
    comparisonTable->setHorizontalHeaderLabels({"Pair", "Channel", "Magnitude Delta", "Angle Delta", "Frequency Delta", "Assessment"});
    comparisonTable->setAlternatingRowColors(true);
    comparisonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    comparisonTable->verticalHeader()->setVisible(false);
    comparisonTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    comparisonTable->setMinimumHeight(190);
    mainLayout->addWidget(makeSection("PMU Comparison", comparisonTable));

    eventTable = new QTableWidget();
    eventTable->setColumnCount(4);
    eventTable->setHorizontalHeaderLabels({"Time", "Scope", "Event", "Detail"});
    eventTable->setAlternatingRowColors(true);
    eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    eventTable->verticalHeader()->setVisible(false);
    eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    eventTable->setMinimumHeight(220);
    mainLayout->addWidget(makeSection("Unified Event Timeline", eventTable));

    scrollArea->setWidget(content);
}

void PdcDashboardWindow::configureChart()
{
    frequencyChart->setBackgroundBrush(QBrush(QColor("#101827")));
    frequencyChart->setPlotAreaBackgroundBrush(QBrush(QColor("#151f2d")));
    frequencyChart->setPlotAreaBackgroundVisible(true);
    frequencyChart->setMargins(QMargins(0, 0, 0, 0));
    frequencyChart->setAnimationOptions(QChart::NoAnimation);

    const QStringList colors = {"#5d9cec", "#28c081", "#ef6f7b", "#d9a441", "#b985ff", "#4dd0e1"};
    for (int i = 0; i < sources.size(); ++i)
    {
        sources[i].frequencySeries = new QLineSeries();
        sources[i].frequencySeries->setName(sources[i].name);
        QPen pen(QColor(colors[i % colors.size()]));
        pen.setWidth(2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        sources[i].frequencySeries->setPen(pen);
        frequencyChart->addSeries(sources[i].frequencySeries);
        connectSeriesHover(sources[i].frequencySeries);
    }

    axisX = new QValueAxis();
    axisY = new QValueAxis();
    axisX->setTitleText("Time (s)");
    axisY->setTitleText("Frequency (Hz)");
    axisX->setRange(0, 10);
    axisY->setRange(49.5, 50.5);
    axisX->setLabelsColor(QColor("#cfd7e3"));
    axisY->setLabelsColor(QColor("#cfd7e3"));
    axisX->setGridLineColor(QColor("#2c4054"));
    axisY->setGridLineColor(QColor("#2c4054"));
    axisX->setTitleBrush(QBrush(QColor("#cfd7e3")));
    axisY->setTitleBrush(QBrush(QColor("#cfd7e3")));

    frequencyChart->addAxis(axisX, Qt::AlignBottom);
    frequencyChart->addAxis(axisY, Qt::AlignLeft);

    for (SourceState& source : sources)
    {
        source.frequencySeries->attachAxis(axisX);
        source.frequencySeries->attachAxis(axisY);
    }

    frequencyChart->legend()->setVisible(true);
    frequencyChart->legend()->setLabelColor(QColor("#cfd7e3"));
    frequencyChart->legend()->setAlignment(Qt::AlignTop);
}

void PdcDashboardWindow::refreshGraphControls()
{
    if (!graphModeSelector || !graphChannelSelector)
        return;

    const QString mode = graphModeSelector->currentText();
    axisY->setTitleText(mode);
    if (graphChannelLabel)
        graphChannelLabel->setVisible(graphModeNeedsChannel());
    graphChannelSelector->setVisible(graphModeNeedsChannel());

    if (!graphModeNeedsChannel())
        return;

    QStringList labels;
    for (const SourceState& source : sources)
    {
        for (const SourceState::PhasorSnapshot& phasor : source.phasors)
        {
            if (!labels.contains(phasor.label, Qt::CaseInsensitive))
                labels << phasor.label;
        }
    }
    labels.sort(Qt::CaseInsensitive);

    const QString signature = labels.join("|");
    if (signature == graphChannelSignature)
        return;

    graphChannelSignature = signature;
    const QString previous = graphChannelSelector->currentText();
    graphChannelSelector->blockSignals(true);
    graphChannelSelector->clear();
    if (labels.isEmpty())
    {
        graphChannelSelector->addItem("No matching channels");
    }
    else
    {
        graphChannelSelector->addItems(labels);
        const int previousIndex = graphChannelSelector->findText(previous, Qt::MatchFixedString);
        if (previousIndex >= 0)
            graphChannelSelector->setCurrentIndex(previousIndex);
    }
    graphChannelSelector->blockSignals(false);
}

void PdcDashboardWindow::updateDashboard()
{
    int connectedCount = 0;
    int streamingCount = 0;
    double frequencySum = 0.0;
    int frequencyCount = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (SourceState& source : sources)
    {
        if (!source.worker)
            continue;

        ++connectedCount;
        const std::size_t frameCount = source.worker->getDataManager().getTotalFrameCount();
        const qint64 elapsedMs = source.lastCheckMs == 0 ? 0 : nowMs - source.lastCheckMs;
        const std::size_t deltaFrames = frameCount >= source.lastFrameCount ? frameCount - source.lastFrameCount : 0;

        if (elapsedMs > 0)
            source.frameRate = (static_cast<double>(deltaFrames) * 1000.0) / static_cast<double>(elapsedMs);

        const bool hasNewFrame = deltaFrames > 0;
        if (hasNewFrame)
            ++streamingCount;

        source.lastFrameCount = frameCount;
        source.lastCheckMs = nowMs;

        const PMUFrame frame = latestFrame(source);
        if (frame.systemUnixMs == 0)
            continue;

        source.hasFrequency = frame.hasFrequency;
        source.lastFrequency = frame.frequency;
        source.hasRocof = frame.hasRocof;
        source.lastRocof = frame.rocof;
        source.phasors.clear();
        for (const PhasorData& phasor : frame.phasors)
        {
            SourceState::PhasorSnapshot snapshot;
            snapshot.label = QString::fromStdString(phasor.label);
            snapshot.isVoltage = phasor.isVoltage;
            snapshot.magnitude = phasor.magnitude;
            snapshot.angleDeg = phasor.angleDeg;
            source.phasors.push_back(snapshot);
        }

        if (frame.hasFrequency)
        {
            frequencySum += frame.frequency;
            ++frequencyCount;
        }

        bool graphValueOk = false;
        const double graphValue = selectedGraphValue(source, &graphValueOk);
        if (hasNewFrame && graphValueOk)
        {
            appendGraphPoint(source, timeForFrame(frame), graphValue);
        }
    }

    const double averageFrequency = frequencyCount > 0 ? frequencySum / frequencyCount : 0.0;
    QString systemStatus = "No Data";
    if (frequencyCount > 0)
    {
        systemStatus = (averageFrequency < FREQUENCY_WARNING_LOW || averageFrequency > FREQUENCY_WARNING_HIGH)
                           ? "Warning"
                           : "Stable";
    }

    refreshGraphControls();
    refreshSummary(connectedCount, streamingCount, averageFrequency, systemStatus);
    refreshStatusTable();
    refreshPhasorTable();
    refreshComparisonTable();
    evaluateGlobalEvents();
}

void PdcDashboardWindow::appendGraphPoint(SourceState& source, double xValue, double yValue)
{
    appendSeriesPoint(source.frequencySeries, xValue, yValue);
    if (source.frequencySeries->count() > 2000)
        source.frequencySeries->remove(0);

    const double minX = qMax(0.0, xValue - 10.0);
    const double maxX = qMax(10.0, xValue);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool updateViewport = nowMs - lastChartViewportUpdateMs >= 100;
    if (updateViewport)
    {
        axisX->setRange(minX, maxX);
        lastChartViewportUpdateMs = nowMs;
    }

    if (!updateViewport || (++chartRangeCounter % 3) != 0)
        return;

    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (const SourceState& entry : sources)
    {
        if (!entry.frequencySeries)
            continue;

        const auto points = entry.frequencySeries->points();
        for (const QPointF& point : points)
        {
            if (point.x() < minX || point.x() > maxX || !std::isfinite(point.y()))
                continue;
            minY = qMin(minY, point.y());
            maxY = qMax(maxY, point.y());
        }
    }

    if (minY != std::numeric_limits<double>::max())
    {
        const double span = qMax(0.001, maxY - minY);
        const double padding = qMax(0.04, span * 0.25);
        axisY->setRange(minY - padding, maxY + padding);
    }
}

void PdcDashboardWindow::appendSeriesPoint(QLineSeries* targetSeries, double xValue, double yValue)
{
    if (!targetSeries)
        return;

    if (lastSeriesX.contains(targetSeries))
    {
        const double lastX = lastSeriesX.value(targetSeries);
        const bool timeMovedBackward = xValue <= lastX;
        const bool timeGap = (xValue - lastX) > 1.5;
        const bool lastWasFinite = lastSeriesFinite.value(targetSeries, false);

        if (lastWasFinite && (timeMovedBackward || timeGap))
            targetSeries->append(xValue, std::numeric_limits<double>::quiet_NaN());
    }

    targetSeries->append(xValue, yValue);
    lastSeriesX[targetSeries] = xValue;
    lastSeriesFinite[targetSeries] = std::isfinite(yValue);
}

void PdcDashboardWindow::connectSeriesHover(QLineSeries* targetSeries)
{
    connect(targetSeries, &QLineSeries::hovered, this, [this](const QPointF& point, bool state) {
        if (state)
        {
            auto* hoveredSeries = qobject_cast<QLineSeries*>(sender());
            showPointTooltip(hoveredSeries ? hoveredSeries->name() : "PMU", point);
        }
        else
        {
            QToolTip::hideText();
        }
    });
}

void PdcDashboardWindow::showPointTooltip(const QString& label, const QPointF& point)
{
    const QString tooltip = QString("%1\nTime: %2 s\nValue: %3")
                                .arg(label)
                                .arg(point.x(), 0, 'f', 3)
                                .arg(point.y(), 0, 'f', 4);
    const QPointF chartPoint = frequencyChart->mapToPosition(point);
    const QPoint globalPoint = frequencyChartView->viewport()->mapToGlobal(chartPoint.toPoint() + QPoint(12, -18));
    QToolTip::showText(globalPoint, tooltip, frequencyChartView);
}

void PdcDashboardWindow::refreshSummary(int connectedCount, int streamingCount, double averageFrequency, const QString& systemStatus)
{
    connectedValue->setText(QString("Connected PMUs\n%1 / %2").arg(connectedCount).arg(sources.size()));
    streamingValue->setText(QString("Streaming PMUs\n%1").arg(streamingCount));
    averageFrequencyValue->setText(averageFrequency > 0.0
                                       ? QString("Average Frequency\n%1 Hz").arg(averageFrequency, 0, 'f', 4)
                                       : "Average Frequency\nNot sent");
    systemStatusValue->setText("Grid Status\n" + systemStatus);

    if (systemStatus == "Warning")
        systemStatusValue->setStyleSheet("QLabel#metricCard { background: #302612; border: 1px solid #d9a441; color: #ffe5a6; }");
    else if (systemStatus == "Stable")
        systemStatusValue->setStyleSheet("QLabel#metricCard { background: #102821; border: 1px solid #28c081; color: #8ee6bd; }");
    else
        systemStatusValue->setStyleSheet("");
}

void PdcDashboardWindow::refreshStatusTable()
{
    statusTable->setRowCount(sources.size());
    for (int row = 0; row < sources.size(); ++row)
    {
        const SourceState& source = sources[row];
        const bool connected = !source.worker.isNull();
        const PMUFrame frame = latestFrame(source);
        const bool hasData = frame.systemUnixMs != 0;

        QString status = "Disconnected";
        if (connected && hasData && source.hasFrequency)
            status = (source.lastFrequency < FREQUENCY_WARNING_LOW || source.lastFrequency > FREQUENCY_WARNING_HIGH) ? "Warning" : "OK";
        else if (connected && hasData)
            status = "No frequency";
        else if (connected)
            status = "Connected";

        QString quality = "Disconnected";
        if (connected)
            quality = source.frameRate > 0.1 ? "Streaming" : "Idle";

        statusTable->setItem(row, 0, item(source.name));
        statusTable->setItem(row, 1, item(source.hasFrequency ? QString::number(source.lastFrequency, 'f', 4) : "Not sent"));
        statusTable->setItem(row, 2, item(source.hasRocof ? QString::number(source.lastRocof, 'f', 4) : "Not sent"));
        statusTable->setItem(row, 3, item(status));
        statusTable->setItem(row, 4, item(hasData && connected ? QString::number(source.worker->getDataManager().getTotalFrameCount()) : "--"));
        statusTable->setItem(row, 5, item(connected ? QString("%1 fps").arg(source.frameRate, 0, 'f', 1) : "--"));
        statusTable->setItem(row, 6, item(quality));
        statusTable->setItem(row, 7, item(hasData ? QString("%1 ms").arg(frame.latencyMs, 0, 'f', 2) : "--"));
    }
}

void PdcDashboardWindow::refreshPhasorTable()
{
    phasorTable->setRowCount(0);

    for (const SourceState& source : sources)
    {
        if (!source.worker)
        {
            const int row = phasorTable->rowCount();
            phasorTable->insertRow(row);
            phasorTable->setItem(row, 0, item(source.name));
            phasorTable->setItem(row, 1, item("Disconnected"));
            phasorTable->setItem(row, 2, item("--"));
            phasorTable->setItem(row, 3, item("--"));
            phasorTable->setItem(row, 4, item("--"));
            continue;
        }

        if (source.phasors.isEmpty())
        {
            const int row = phasorTable->rowCount();
            phasorTable->insertRow(row);
            phasorTable->setItem(row, 0, item(source.name));
            phasorTable->setItem(row, 1, item("No phasors sent"));
            phasorTable->setItem(row, 2, item("--"));
            phasorTable->setItem(row, 3, item("--"));
            phasorTable->setItem(row, 4, item("--"));
            continue;
        }

        for (const SourceState::PhasorSnapshot& phasor : source.phasors)
        {
            const int row = phasorTable->rowCount();
            phasorTable->insertRow(row);
            phasorTable->setItem(row, 0, item(source.name));
            phasorTable->setItem(row, 1, item(phasor.label));
            phasorTable->setItem(row, 2, item(pdcPhasorTypeLabel(phasor.label, phasor.isVoltage)));
            phasorTable->setItem(row, 3, item(QString::number(phasor.magnitude, 'f', 3)));
            phasorTable->setItem(row, 4, item(QString::number(phasor.angleDeg, 'f', 3)));
        }
    }
}

void PdcDashboardWindow::refreshComparisonTable()
{
    QVector<int> active;
    for (int i = 0; i < sources.size(); ++i)
    {
        if (sources[i].worker && latestFrame(sources[i]).systemUnixMs != 0)
            active.push_back(i);
    }

    comparisonTable->setRowCount(0);

    for (int a = 0; a < active.size(); ++a)
    {
        for (int b = a + 1; b < active.size(); ++b)
        {
            const SourceState& left = sources[active[a]];
            const SourceState& right = sources[active[b]];

            const QString freqDelta = (left.hasFrequency && right.hasFrequency)
                                          ? QString::number(std::fabs(left.lastFrequency - right.lastFrequency), 'f', 5)
                                          : "Not sent";
            bool wrotePairRow = false;

            for (const SourceState::PhasorSnapshot& leftPhasor : left.phasors)
            {
                const SourceState::PhasorSnapshot* rightPhasor = findPhasor(right, leftPhasor.label);
                if (!rightPhasor)
                    continue;

                const double magnitudeDelta = std::fabs(leftPhasor.magnitude - rightPhasor->magnitude);
                const double angleDelta = std::fabs(leftPhasor.angleDeg - rightPhasor->angleDeg);
                QString assessment = "Comparable";
                if ((left.hasFrequency && right.hasFrequency && std::fabs(left.lastFrequency - right.lastFrequency) > 0.05) ||
                    magnitudeDelta > (leftPhasor.isVoltage ? 5.0 : 1.0) ||
                    angleDelta > 5.0)
                {
                    assessment = "Check";
                }

                const int row = comparisonTable->rowCount();
                comparisonTable->insertRow(row);
                comparisonTable->setItem(row, 0, item(left.name + " / " + right.name));
                comparisonTable->setItem(row, 1, item(leftPhasor.label));
                comparisonTable->setItem(row, 2, item(QString::number(magnitudeDelta, 'f', 3)));
                comparisonTable->setItem(row, 3, item(QString::number(angleDelta, 'f', 3)));
                comparisonTable->setItem(row, 4, item(freqDelta));
                comparisonTable->setItem(row, 5, item(assessment));
                wrotePairRow = true;
            }

            if (!wrotePairRow && (left.hasFrequency || right.hasFrequency))
            {
                const int row = comparisonTable->rowCount();
                comparisonTable->insertRow(row);
                comparisonTable->setItem(row, 0, item(left.name + " / " + right.name));
                comparisonTable->setItem(row, 1, item("No common phasors"));
                comparisonTable->setItem(row, 2, item("Not sent"));
                comparisonTable->setItem(row, 3, item("Not sent"));
                comparisonTable->setItem(row, 4, item(freqDelta));
                comparisonTable->setItem(row, 5, item(freqDelta == "Not sent" ? "Not comparable" : "Frequency only"));
            }
        }
    }
}

void PdcDashboardWindow::evaluateGlobalEvents()
{
    int activeWithFrequency = 0;
    int frequencyDipCount = 0;

    for (const SourceState& source : sources)
    {
        if (!source.worker)
        {
            continue;
        }

        if (source.hasFrequency)
        {
            ++activeWithFrequency;
            if (source.lastFrequency < UNDER_FREQUENCY_THRESHOLD)
            {
                ++frequencyDipCount;
                logEvent(source.name, "Under Frequency", QString("Frequency %1 Hz").arg(source.lastFrequency, 0, 'f', 4));
            }
        }

        for (const SourceState::PhasorSnapshot& phasor : source.phasors)
        {
            if (phasor.isVoltage && phasor.magnitude < VOLTAGE_DIP_THRESHOLD)
            {
                logEvent(source.name,
                         QString("Voltage Dip %1").arg(phasor.label),
                         QString("%1 magnitude %2").arg(phasor.label).arg(phasor.magnitude, 0, 'f', 3));
            }
        }
    }

    if (activeWithFrequency >= 2 && frequencyDipCount == activeWithFrequency)
    {
        logEvent("System", "Possible Grid Event", "All reporting PMUs show frequency dip");
    }
}

void PdcDashboardWindow::logEvent(const QString& scope, const QString& event, const QString& detail)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString key = scope + "|" + event + "|" + detail;
    if (lastEventMsByKey.contains(key) && nowMs - lastEventMsByKey[key] < EVENT_COOLDOWN_MS)
        return;

    lastEventMsByKey[key] = nowMs;
    const int row = eventTable->rowCount();
    eventTable->insertRow(row);
    eventTable->setItem(row, 0, item(isoNow()));
    eventTable->setItem(row, 1, item(scope));
    eventTable->setItem(row, 2, item(event));
    eventTable->setItem(row, 3, item(detail));
    eventTable->scrollToBottom();
}

PMUFrame PdcDashboardWindow::latestFrame(const SourceState& source) const
{
    if (!source.worker)
        return PMUFrame();

    return source.worker->getDataManager().getLatest();
}

double PdcDashboardWindow::timeForFrame(const PMUFrame& frame)
{
    if (frame.systemUnixMs == 0)
        return elapsedTimer.elapsed() / 1000.0;

    if (chartOriginMs < 0.0)
        chartOriginMs = static_cast<double>(frame.systemUnixMs);

    return (static_cast<double>(frame.systemUnixMs) - chartOriginMs) / 1000.0;
}

double PdcDashboardWindow::selectedGraphValue(const SourceState& source, bool* ok) const
{
    const QString mode = graphModeSelector ? graphModeSelector->currentText() : "Frequency";
    if (ok)
        *ok = false;

    if (mode == "Frequency")
    {
        if (ok)
            *ok = source.hasFrequency;
        return source.lastFrequency;
    }

    if (mode == "ROCOF")
    {
        if (ok)
            *ok = source.hasRocof;
        return source.lastRocof;
    }

    const QString selectedChannel = graphChannelSelector ? graphChannelSelector->currentText() : QString();
    const SourceState::PhasorSnapshot* phasor = findPhasor(source, selectedChannel);
    if (!phasor)
        return 0.0;

    if (ok)
        *ok = true;

    return pdcAngleMode(mode) ? phasor->angleDeg : phasor->magnitude;
}

const PdcDashboardWindow::SourceState::PhasorSnapshot* PdcDashboardWindow::findPhasor(const SourceState& source, const QString& label) const
{
    for (const SourceState::PhasorSnapshot& phasor : source.phasors)
    {
        if (phasor.label.compare(label, Qt::CaseInsensitive) == 0)
            return &phasor;
    }

    return nullptr;
}

bool PdcDashboardWindow::graphModeNeedsChannel() const
{
    const QString mode = graphModeSelector ? graphModeSelector->currentText() : QString();
    return pdcMagnitudeMode(mode) || pdcAngleMode(mode);
}

bool PdcDashboardWindow::graphModeWantsVoltage() const
{
    const QString mode = graphModeSelector ? graphModeSelector->currentText() : QString();
    return mode.startsWith("Voltage");
}

QString PdcDashboardWindow::isoNow() const
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
