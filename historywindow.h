#ifndef HISTORYWINDOW_H
#define HISTORYWINDOW_H
#include "data_manager.h"
#include <QTimer>
#include <QWidget>

namespace Ui {
class HistoryWindow;
}

class HistoryWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWindow(QWidget *parent = nullptr);
    ~HistoryWindow();

public:
    void loadData(const DataManager& dataManager);

private:
    const DataManager* dataManagerRef;
    QTimer* timer;

private slots:
    void refreshTable();

private:
    Ui::HistoryWindow *ui;
};

#endif // HISTORYWINDOW_H
