#include "main_window.h"
#include "ui_main_window.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupDatabase();
    setupModels();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("shop.db");
    
    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Could not open database: " + db.lastError().text());
        return;
    }

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS inventory (id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT, price REAL, quantity INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS repairs (id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "item TEXT, issue TEXT, status TEXT, customer_id INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS customers (id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT, points INTEGER)");
}

void MainWindow::setupModels()
{
    inventoryModel = new QSqlTableModel(this, db);
    inventoryModel->setTable("inventory");
    inventoryModel->select();
    ui->inventoryTableView->setModel(inventoryModel);

    repairModel = new QSqlTableModel(this, db);
    repairModel->setTable("repairs");
    repairModel->select();
    ui->repairTableView->setModel(repairModel);

    customerModel = new QSqlTableModel(this, db);
    customerModel->setTable("customers");
    customerModel->select();
    ui->customerTableView->setModel(customerModel);
}

void MainWindow::setupConnections()
{
    connect(ui->addItemButton, &QPushButton::clicked, this, &MainWindow::onAddItemClicked);
    connect(ui->editItemButton, &QPushButton::clicked, this, &MainWindow::onEditItemClicked);
    connect(ui->deleteItemButton, &QPushButton::clicked, this, &MainWindow::onDeleteItemClicked);
    connect(ui->sellButton, &QPushButton::clicked, this, &MainWindow::onSellItemClicked);
    connect(ui->newRepairButton, &QPushButton::clicked, this, &MainWindow::onNewRepairRequestClicked);
    connect(ui->registerCustomerButton, &QPushButton::clicked, this, &MainWindow::onRegisterCustomerClicked);
}

void MainWindow::onAddItemClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Add Item", "Item Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    double price = QInputDialog::getDouble(this, "Add Item", "Price:", 0, 0, 1000000, 2, &ok);
    if (!ok) return;

    int quantity = QInputDialog::getInt(this, "Add Item", "Quantity:", 1, 0, 1000000, 1, &ok);
    if (!ok) return;

    QSqlQuery query;
    query.prepare("INSERT INTO inventory (name, price, quantity) VALUES (:name, :price, :quantity)");
    query.bindValue(":name", name);
    query.bindValue(":price", price);
    query.bindValue(":quantity", quantity);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to add item: " + query.lastError().text());
    } else {
        inventoryModel->select();
    }
}

void MainWindow::onEditItemClicked()
{
    QModelIndex index = ui->inventoryTableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Error", "Please select an item to edit.");
        return;
    }

    int row = index.row();
    QSqlRecord record = inventoryModel->record(row);

    bool ok;
    QString name = QInputDialog::getText(this, "Edit Item", "Item Name:", QLineEdit::Normal, record.value("name").toString(), &ok);
    if (!ok) return;

    double price = QInputDialog::getDouble(this, "Edit Item", "Price:", record.value("price").toDouble(), 0, 1000000, 2, &ok);
    if (!ok) return;

    int quantity = QInputDialog::getInt(this, "Edit Item", "Quantity:", record.value("quantity").toInt(), 0, 1000000, 1, &ok);
    if (!ok) return;

    record.setValue("name", name);
    record.setValue("price", price);
    record.setValue("quantity", quantity);

    if (!inventoryModel->setRecord(row, record)) {
        QMessageBox::critical(this, "Error", "Failed to update item: " + inventoryModel->lastError().text());
    } else {
        inventoryModel->submitAll();
    }
}

void MainWindow::onDeleteItemClicked()
{
    QModelIndex index = ui->inventoryTableView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Error", "Please select an item to delete.");
        return;
    }

    if (QMessageBox::question(this, "Confirm Deletion", "Are you sure you want to delete this item?") == QMessageBox::Yes) {
        if (!inventoryModel->removeRow(index.row())) {
            QMessageBox::critical(this, "Error", "Failed to delete item: " + inventoryModel->lastError().text());
        } else {
            inventoryModel->submitAll();
        }
    }
}

void MainWindow::onSellItemClicked()
{
    QString barcode = ui->barcodeInput->text();
    if (barcode.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a barcode.");
        return;
    }

    // In a real application, you would look up the item based on the barcode
    // For this example, we'll just use the first item in the inventory

    QSqlQuery query;
    query.prepare("SELECT id, name, price, quantity FROM inventory LIMIT 1");
    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, "Error", "No items in inventory.");
        return;
    }

    int id = query.value("id").toInt();
    QString name = query.value("name").toString();
    double price = query.value("price").toDouble();
    int quantity = query.value("quantity").toInt();

    if (quantity <= 0) {
        QMessageBox::warning(this, "Error", "Item out of stock.");
        return;
    }

    // Update inventory
    query.prepare("UPDATE inventory SET quantity = quantity - 1 WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to update inventory: " + query.lastError().text());
        return;
    }

    // Generate sale barcode
    QString saleBarcode = generateBarcode();

    QMessageBox::information(this, "Sale Completed", 
                             "Item: " + name + "\n" +
                             "Price: $" + QString::number(price, 'f', 2) + "\n" +
                             "Sale Barcode: " + saleBarcode);

    inventoryModel->select();
    ui->barcodeInput->clear();
}

void MainWindow::onNewRepairRequestClicked()
{
    bool ok;
    QString item = QInputDialog::getText(this, "New Repair Request", "Item:", QLineEdit::Normal, "", &ok);
    if (!ok || item.isEmpty()) return;

    QString issue = QInputDialog::getText(this, "New Repair Request", "Issue:", QLineEdit::Normal, "", &ok);
    if (!ok || issue.isEmpty()) return;

    QString customerName = QInputDialog::getText(this, "New Repair Request", "Customer Name:", QLineEdit::Normal, "", &ok);
    if (!ok || customerName.isEmpty()) return;

    // Find or create customer
    QSqlQuery query;
    query.prepare("SELECT id FROM customers WHERE name = :name");
    query.bindValue(":name", customerName);
    if (!query.exec() || !query.next()) {
        query.prepare("INSERT INTO customers (name, points) VALUES (:name, 0)");
        query.bindValue(":name", customerName);
        if (!query.exec()) {
            QMessageBox::critical(this, "Error", "Failed to create customer: " + query.lastError().text());
            return;
        }
        query.prepare("SELECT last_insert_rowid()");
        query.exec();
        query.next();
    }
    int customerId = query.value(0).toInt();

    // Create repair request
    query.prepare("INSERT INTO repairs (item, issue, status, customer_id) VALUES (:item, :issue, 'Pending', :customer_id)");
    query.bindValue(":item", item);
    query.bindValue(":issue", issue);
    query.bindValue(":customer_id", customerId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", 