#pragma once

#include <QMainWindow>
#include <QThread>
#include "pmu_worker.h"
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <qlabel.h>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct ConnectionCard
{
    QLabel* titleLabel;
    QLineEdit* deviceIdEdit;
    QLineEdit* ipEdit;
    QLineEdit* portEdit;
    QPushButton* removeButton;
    QPushButton* disconnectButton;
    QLabel* statusLabel;

    QPushButton* dashboardButton; //

    QWidget* container;

    PMUWorker* worker = nullptr;
    QThread* thread = nullptr;
};
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void addConnectionCard();
    void refreshGrid();

private slots:
    void on_connectButton_clicked();
    void on_addConnectionButton_clicked();
private:
    int cardIndexForWidget(QObject* widget) const;
    int cardIndexForWorker(PMUWorker* worker) const;
    int cardIndexForThread(QThread* thread) const;
    void updateCardTitles();
    void resetCardConnectionState(int index);
    void disconnectCard(int index);
    void openCentralPdcDashboard();

    Ui::MainWindow *ui;

    std::vector<ConnectionCard> cards;
};
