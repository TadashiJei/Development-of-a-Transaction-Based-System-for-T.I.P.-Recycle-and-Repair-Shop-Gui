#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupDatabase();
    setupModels();
    setupConnections();

    dateTimeTimer = new QTimer(this);
    connect(dateTimeTimer, &QTimer::timeout, this, &MainWindow::updateDateTime);
    dateTimeTimer->start(1000);  // Update every second

    updateDateTime();
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
               "name TEXT, price REAL, quantity INTEGER, barcode TEXT)");
    query.exec("CREATE TABLE IF NOT EXISTS repairs (id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "item TEXT, issue TEXT, status TEXT, customer_id INTEGER, barcode TEXT)");
    query.exec("CREATE TABLE IF NOT EXISTS customers (id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT, points INTEGER, email TEXT)");
}

void MainWindow::setupModels()
{
    inventoryModel = new QSqlTableModel(this, db);
    inventoryModel->setTable("inventory");
    inventoryModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    inventoryModel->select();
    ui->inventoryTableView->setModel(inventoryModel);

    repairModel = new QSqlTableModel(this, db);
    repairModel->setTable("repairs");
    repairModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    repairModel->select();
    ui->repairTableView->setModel(repairModel);

    customerModel = new QSqlTableModel(this, db);
    customerModel->setTable("customers");
    customerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
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
    connect(ui->searchCustomerButton, &QPushButton::clicked, this, &MainWindow::onSearchCustomer);
    connect(ui->barcodeInput, &QLineEdit::returnPressed, this, &MainWindow::onBarcodeScanned);
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

    QString barcode = generateBarcode();

    QSqlQuery query;
    query.prepare("INSERT INTO inventory (name, price, quantity, barcode) VALUES (:name, :price, :quantity, :barcode)");
    query.bindValue(":name", name);
    query.bindValue(":price", price);
    query.bindValue(":quantity", quantity);
    query.bindValue(":barcode", barcode);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to add item: " + query.lastError().text());
    } else {
        inventoryModel->select();
        showNotification("Item added successfully. Barcode: " + barcode);
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
        showNotification("Item updated successfully.");
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
            showNotification("Item deleted successfully.");
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

    QSqlQuery query;
    query.prepare("SELECT id, name, price, quantity FROM inventory WHERE barcode = :barcode");
    query.bindValue(":barcode", barcode);
    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, "Error", "Item not found.");
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
    showNotification("Sale completed successfully.");
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

    // Generate repair barcode
    QString repairBarcode = generateBarcode();

    // Create repair request
    query.prepare("INSERT INTO repairs (item, issue, status, customer_id, barcode) VALUES (:item, :issue, 'Pending', :customer_id, :barcode)");
    query.bindValue(":item", item);
    query.bindValue(":issue", issue);
    query.bindValue(":customer_id", customerId);
    query.bindValue(":barcode", repairBarcode);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to create repair request: " + query.lastError().text());
    } else {
        repairModel->select();
        showNotification("Repair request created successfully. Barcode: " + repairBarcode);
    }
}

void MainWindow::onRegisterCustomerClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Register Customer", "Customer Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    QString email = QInputDialog::getText(this, "Register Customer", "Email:", QLineEdit::Normal, "", &ok);
    if (!ok || email.isEmpty()) return;

    QSqlQuery query;
    query.prepare("INSERT INTO customers (name, points, email) VALUES (:name, :points, :email)");
    query.bindValue(":name", name);
    query.bindValue(":points", 100);  // Initial points for registration
    query.bindValue(":email", email);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to register customer: " + query.lastError().text());
    } else {
        customerModel->select();
        showNotification("Customer registered successfully. 100 points awarded!");
    }
}

void MainWindow::onSearchCustomer()
{
    bool ok;
    QString searchTerm = QInputDialog::getText(this, "Search Customer", "Enter name or email:", QLineEdit::Normal, "", &ok);
    if (!ok || searchTerm.isEmpty()) return;

    customerModel->setFilter(QString("name LIKE '%%1%' OR email LIKE '%%1%'").arg(searchTerm));
    customerModel->select();
}

void MainWindow::onBarcodeScanned()
{
    QString barcode = ui->barcodeInput->text();
    if (barcode.isEmpty()) return;

    QSqlQuery query;
    query.prepare("SELECT name, price, quantity FROM inventory WHERE barcode = :barcode");
    query.bindValue(":barcode", barcode);
    if (query.exec() && query.next()) {
        QString name = query.value("name").toString();
        double price = query.value("price").toDouble();
        int quantity = query.value("quantity").toInt();
        showNotification(QString("Item: %1\nPrice: $%2\nQuantity: %3").arg(name).arg(price, 0, 'f', 2).arg(quantity));
    } else {
        query.prepare("SELECT item, issue, status FROM repairs WHERE barcode = :barcode");
        query.bindValue(":barcode", barcode);
        if (query.exec() && query.next()) {
            QString item = query.value("item").toString();
            QString issue = query.value("issue").toString();
            QString status = query.value("status").toString();
            showNotification(QString("Repair Request\nItem: %1\nIssue: %2\nStatus: %3").arg(item).arg(issue).arg(status));
        } else {
            showNotification("Barcode not found.");
        }
    }
    ui->barcodeInput->clear();
}

QString MainWindow::generateBarcode()
{
    return QString::number(QRandomGenerator::global()->generate() % 1000000000, 10).rightJustified(9, '0');
}

bool MainWindow::validateInput(const QStringList &inputs)
{
    for (const QString  &input : inputs) {
        if (input.isEmpty()) {
            return false;
        }
    }
    return true;
}

void MainWindow::updateCustomerPoints(int customerId, int points)
{
    QSqlQuery query;
    query.prepare("UPDATE customers SET points = points + :points WHERE id = :id");
    query.bindValue(":points", points);
    query.bindValue(":id", customerId);
    if (!query.exec()) {
        QMessageBox::warning(this, "Error", "Failed to update customer points: " + query.lastError().text());
    }
}

void MainWindow::showNotification(const QString &message)
{
    ui->notificationLabel->setText(message);
    QTimer::singleShot(5000, ui->notificationLabel, &QLabel::clear);
}

void MainWindow::updateDateTime()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    ui->dateTimeLabel->setText(currentDateTime.toString("yyyy-MM-dd hh:mm:ss"));
}

export default function Component() {
    return (
        <div className="p-4">
            <h1 className="text-2xl font-bold mb-4">Recycle and Repair Shop</h1>
            <p>This is a placeholder for the Qt-based GUI. The actual implementation would be done in C++ using Qt framework.</p>
        </div>
    );
}