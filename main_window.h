#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QBarcode>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddItemClicked();
    void onEditItemClicked();
    void onDeleteItemClicked();
    void onSellItemClicked();
    void onNewRepairRequestClicked();
    void onRegisterCustomerClicked();

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    QSqlTableModel *inventoryModel;
    QSqlTableModel *repairModel;
    QSqlTableModel *customerModel;
    QBarcode barcode;

    void setupDatabase();
    void setupModels();
    void setupConnections();
    QString generateBarcode();
    bool validateInput(const QStringList &inputs);
};
#endif // MAINWINDOW_H