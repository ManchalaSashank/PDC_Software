#pragma once

#include <QElapsedTimer>
#include <QComboBox>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <QTableWidget>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "pmu_worker.h"

struct PdcPmuSource
{
    QString name;
    PMUWorker* worker = nullptr;
};

class PdcDashboardWindow : public QWidget
{
public:
    explicit PdcDashboardWindow(const QVector<PdcPmuSource>& sources, QWidget* parent = nullptr);

private:
    void updateDashboard();

    struct SourceState
    {
        struct PhasorSnapshot
        {
            QString label;
            bool isVoltage = true;
            double magnitude = 0.0;
            double angleDeg = 0.0;
        };

        QString name;
        QPointer<PMUWorker> worker;
        QLineSeries* frequencySeries = nullptr;
        std::size_t lastFrameCount = 0;
        qint64 lastCheckMs = 0;
        double frameRate = 0.0;
        double lastFrequency = 0.0;
        bool hasFrequency = false;
        double lastRocof = 0.0;
        bool hasRocof = false;
        QVector<PhasorSnapshot> phasors;
    };

    void buildUi();
    void configureChart();
    void refreshGraphControls();
    void appendGraphPoint(SourceState& source, double xValue, double yValue);
    void appendSeriesPoint(QLineSeries* targetSeries, double xValue, double yValue);
    void connectSeriesHover(QLineSeries* targetSeries);
    void showPointTooltip(const QString& label, const QPointF& point);
    void refreshSummary(int connectedCount, int streamingCount, double averageFrequency, const QString& systemStatus);
    void refreshStatusTable();
    void refreshPhasorTable();
    void refreshComparisonTable();
    void evaluateGlobalEvents();
    void logEvent(const QString& scope, const QString& event, const QString& detail);
    PMUFrame latestFrame(const SourceState& source) const;
    double timeForFrame(const PMUFrame& frame);
    double selectedGraphValue(const SourceState& source, bool* ok = nullptr) const;
    const SourceState::PhasorSnapshot* findPhasor(const SourceState& source, const QString& label) const;
    bool graphModeNeedsChannel() const;
    bool graphModeWantsVoltage() const;
    QString isoNow() const;

    QVector<SourceState> sources;
    QTimer* refreshTimer = nullptr;
    QElapsedTimer elapsedTimer;

    QLabel* connectedValue = nullptr;
    QLabel* streamingValue = nullptr;
    QLabel* averageFrequencyValue = nullptr;
    QLabel* systemStatusValue = nullptr;

    QComboBox* graphModeSelector = nullptr;
    QLabel* graphChannelLabel = nullptr;
    QComboBox* graphChannelSelector = nullptr;
    QChart* frequencyChart = nullptr;
    QChartView* frequencyChartView = nullptr;
    QValueAxis* axisX = nullptr;
    QValueAxis* axisY = nullptr;

    QTableWidget* statusTable = nullptr;
    QTableWidget* phasorTable = nullptr;
    QTableWidget* comparisonTable = nullptr;
    QTableWidget* eventTable = nullptr;

    double chartOriginMs = -1.0;
    QMap<QString, qint64> lastEventMsByKey;
    QString graphChannelSignature;
    QMap<QLineSeries*, double> lastSeriesX;
    QMap<QLineSeries*, bool> lastSeriesFinite;
    qint64 lastChartViewportUpdateMs = 0;
    int chartRangeCounter = 0;

    static constexpr double UNDER_FREQUENCY_THRESHOLD = 49.5;
    static constexpr double VOLTAGE_DIP_THRESHOLD = 210.0;
    static constexpr double FREQUENCY_WARNING_LOW = 49.8;
    static constexpr double FREQUENCY_WARNING_HIGH = 50.2;
    static constexpr qint64 EVENT_COOLDOWN_MS = 2000;
};
