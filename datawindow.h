#pragma once

#include <QWidget>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTimer>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QScrollArea>
#include <QMap>
#include <vector>
#include "data_manager.h"
#include "pmu_worker.h"

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>

class DataWindow : public QWidget
{
    Q_OBJECT

public:
    DataWindow(DataManager* manager, PMUWorker* worker, const QString& deviceName, QWidget* parent = nullptr);

private slots:
    void onNewFrame(const PMUFrame& frame);
    void updateTable();
    void onSignalChanged(const QString& signal);
    void onExportClicked();

private:
    struct EventLogEntry
    {
        QString isoTime;
        QString type;
        QString detail;
        double value = 0.0;
        double threshold = 0.0;
        double latencyMs = 0.0;
    };

    void updateMetrics(const PMUFrame& frame);
    void updateTimeSync(const PMUFrame& frame);
    void applyConfigToUi();
    void updateChart(const PMUFrame& frame);
    void rebuildChartFromHistory();
    void appendFrameToSeries(const PMUFrame& frame, double xValue);
    void appendSeriesPoint(QLineSeries* targetSeries, double xValue, double yValue);
    void updateYAxisRange(double minX, double maxX);
    void connectSeriesHover(QLineSeries* targetSeries, const QString& label);
    void showPointTooltip(const QString& label, const QPointF& point);
    void updatePhasorControls();
    void evaluateFrameEvents(const PMUFrame& frame);
    void triggerEvent(const QString& type, const QString& detail, double value, double threshold, const PMUFrame& frame);
    QString isoTimeFromUnixMs(uint64_t unixMs) const;
    QString isoTimeFromPmuFrame(const PMUFrame& frame) const;
    double sampleTimeForIndex(int index) const;
    double sampleTimeForFrame(const PMUFrame& frame, int fallbackIndex);
    void clearAllSeries();

    // Data
    DataManager* dataManager;
    PMUWorker* worker;
    PMUFrame lastFrame;

    // UI Components
    QLabel* freqLabel;
    QLabel* rocofLabel;
    QLabel* pmuTimeLabel;
    QLabel* latencyLabel;
    QLabel* alertBanner;
    QLabel* titleLabel;
    QLabel* statusIndicator;

    QComboBox* signalSelector;
    QComboBox* channelSelector;

    QCheckBox* v1Check;
    QCheckBox* v2Check;
    QCheckBox* v3Check;
    QHBoxLayout* phasorCheckLayout;
    std::vector<QCheckBox*> phasorChecks;

    QPushButton* pauseBtn;
    QPushButton* exportBtn;

    QTableWidget* table;
    QTableWidget* eventTable;

    // Chart
    QChart* chart;
    QChartView* chartView;
    QLineSeries* series;
    QLineSeries* seriesV1;
    QLineSeries* seriesV2;
    QLineSeries* seriesV3;
    std::vector<QLineSeries*> phasorSeries;
    QValueAxis* axisX;
    QValueAxis* axisY;

    // Timers
    QTimer* tableTimer;

    // State
    bool paused = false;
    QString deviceName;
    double chartOriginUnixMs = -1.0;
    int chartFrameCounter = 0;
    PMUConfig currentConfig;
    std::vector<EventLogEntry> eventLog;
    QMap<QString, qint64> lastEventMsByType;
    QMap<QLineSeries*, double> lastSeriesX;
    QMap<QLineSeries*, bool> lastSeriesFinite;
    qint64 lastMetricUiUpdateMs = 0;
    qint64 lastChartViewportUpdateMs = 0;

    static constexpr int MAX_POINTS = 2000;
    static constexpr double UNDER_FREQUENCY_THRESHOLD = 49.5;
    static constexpr double HIGH_ROCOF_THRESHOLD = 0.5;
    static constexpr double VOLTAGE_DIP_THRESHOLD = 210.0;
    static constexpr qint64 EVENT_COOLDOWN_MS = 1000;
};
