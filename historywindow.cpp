#include "historywindow.h"
#include "ui_historywindow.h"

HistoryWindow::HistoryWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HistoryWindow)
{
    ui->setupUi(this);
    timer = new QTimer(this);

    connect(timer, &QTimer::timeout,
            this, &HistoryWindow::refreshTable);

    timer->start(1000); // refresh every 1 second
}

HistoryWindow::~HistoryWindow()
{
    delete ui;
}


void HistoryWindow::loadData(const DataManager& dataManager)
{
    dataManagerRef = &dataManager;
    const auto history = dataManager.getFramesSnapshot();

    ui->tableWidget->setRowCount(history.size());

    ui->tableWidget->setColumnCount(6);
    ui->tableWidget->setHorizontalHeaderLabels({
        "Time", "Freq", "ROCOF",
        "Mag1", "Angle1", "Analog1"
    });

    int row = 0;


    for (const auto& frame : history)
    {
        ui->tableWidget->setItem(row, 0,
                                 new QTableWidgetItem(QString::number(frame.soc)));

        ui->tableWidget->setItem(row, 1,
                                 new QTableWidgetItem(QString::number(frame.frequency)));

        ui->tableWidget->setItem(row, 2,
                                 new QTableWidgetItem(QString::number(frame.rocof)));

        if (!frame.phasors.empty())
        {
            ui->tableWidget->setItem(row, 3,
                                     new QTableWidgetItem(QString::number(frame.phasors[0].magnitude)));

            ui->tableWidget->setItem(row, 4,
                                     new QTableWidgetItem(QString::number(frame.phasors[0].angleDeg)));
        }

        if (!frame.analogs.empty())
        {
            ui->tableWidget->setItem(row, 5,
                                     new QTableWidgetItem(QString::number(frame.analogs[0])));
        }

        row++;
        // Stretch columns nicely
        ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    }
}


void HistoryWindow::refreshTable()
{
    if (!dataManagerRef) return;

    const auto history = dataManagerRef->getFramesSnapshot();

    ui->tableWidget->setRowCount(history.size());

    ui->tableWidget->setColumnCount(6);

    ui->tableWidget->setHorizontalHeaderLabels({
        "Time", "Freq", "ROCOF",
        "Mag1", "Angle1", "Analog1"
    });

    int row = 0;

    for (const auto& frame : history)
    {
        ui->tableWidget->setItem(row, 0,
                                 new QTableWidgetItem(QString::number(frame.soc)));

        ui->tableWidget->setItem(row, 1,
                                 new QTableWidgetItem(QString::number(frame.frequency)));

        ui->tableWidget->setItem(row, 2,
                                 new QTableWidgetItem(QString::number(frame.rocof)));

        if (!frame.phasors.empty())
        {
            ui->tableWidget->setItem(row, 3,
                                     new QTableWidgetItem(QString::number(frame.phasors[0].magnitude)));

            ui->tableWidget->setItem(row, 4,
                                     new QTableWidgetItem(QString::number(frame.phasors[0].angleDeg)));
        }

        if (!frame.analogs.empty())
        {
            ui->tableWidget->setItem(row, 5,
                                     new QTableWidgetItem(QString::number(frame.analogs[0])));
        }

        row++;
    }
}
