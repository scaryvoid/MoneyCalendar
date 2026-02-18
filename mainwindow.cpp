// mainwindow.cpp (updated)
#include "mainwindow.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

// CustomCalendar implementation
CustomCalendar::CustomCalendar(QWidget *parent) : QCalendarWidget(parent) {}

void CustomCalendar::setTransactions(const QVector<Transaction> *trans) {
    transactions = trans;
}

void CustomCalendar::paintCell(QPainter *painter, const QRect &rect, QDate date) const {
    // Draw default calendar cell (day number, background, etc.)
    QCalendarWidget::paintCell(painter, rect, date);

    if (!transactions) return;

    // Existing: net transaction amount on this exact day → green/red overlay
    double netOnDay = getNetAmountOnDate(date);
    if (netOnDay != 0.0) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(netOnDay > 0 ? QColor(0, 180, 0, 90) : QColor(220, 0, 0, 90));
        painter->drawRect(rect.adjusted(2, 2, -2, -2));
        painter->restore();
    }

    // NEW: Projected balance up to this date (including this day)
    if (mainWindow) {
        double projectedBalance = mainWindow->calculateBalance(date);

        if (projectedBalance < 0.0) {
            painter->save();

            // Draw small red warning circle with ! in top-right corner
            int size = qMin(rect.width(), rect.height()) / 4;  // e.g. 8–12 px
            if (size < 10) size = 10;  // minimum readable size

            QPoint center(rect.right() - size - 2, rect.top() + size + 2);

            // Red circle
            painter->setBrush(QColor(220, 30, 30));     // vivid red
            painter->setPen(QColor(255, 255, 255, 180)); // light white border
            painter->drawEllipse(center, size, size);

            // White exclamation mark
            painter->setPen(Qt::white);
            painter->setFont(QFont("Arial", size * 1.4, QFont::Bold));
            painter->drawText(QRect(center.x() - size, center.y() - size, size*2, size*2),
                              Qt::AlignCenter, "!");

            painter->restore();
        }
    }
}

double CustomCalendar::getNetAmountOnDate(const QDate &date) const {
    double net = 0.0;
    if (!transactions || !mainWindow) return net;

    for (const auto &trans : *transactions) {
        if (mainWindow->isTransactionOnDate(trans, date)) {   // ← call MainWindow's version
            net += trans.amount;
        }
    }
    return net;
}

bool CustomCalendar::isTransactionOnDate(const Transaction &trans, const QDate &date) const {
    if (date < trans.startDate) return false;

    if (trans.recurrence == RecurrenceType::None) {
        return trans.startDate == date;
    } else if (trans.recurrence == RecurrenceType::BiWeekly) {
        int daysDiff = trans.startDate.daysTo(date);
        return daysDiff % 14 == 0;
    } else if (trans.recurrence == RecurrenceType::Monthly) {
        int startDay = trans.startDate.day();
        int targetDay = (startDay > date.daysInMonth()) ? date.daysInMonth() : startDay;
        return date.day() == targetDay;
    }
    return false;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    calendar(new CustomCalendar),
    eventList(new QListWidget),
    addButton(new QPushButton("Add Transaction")),
    deleteButton(new QPushButton("Delete Selected")),
    currentBalanceLabel(new QLabel("Current Balance (today): $0.00")),
    selectedDateBalanceLabel(new QLabel("Balance on selected date: $0.00")) {
    calendar = new CustomCalendar(this);
    setWindowTitle("Financial Calendar Tracker");
    deleteButton->setEnabled(false);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    mainLayout->addWidget(calendar);
    mainLayout->addWidget(eventList);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(deleteButton);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addWidget(currentBalanceLabel);
    mainLayout->addWidget(selectedDateBalanceLabel);  // ← this is now the projected/selected balance

    setCentralWidget(centralWidget);

    connect(calendar, &QCalendarWidget::clicked, this, &MainWindow::onDateSelected);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddButtonClicked);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteButtonClicked);
    connect(eventList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEventSelectionChanged);

    loadTransactions();
    calendar->setTransactions(&transactions);
    calendar->setBalanceCalculator(this);

    onDateSelected(QDate::currentDate());
    updateBalances();
}


MainWindow::~MainWindow() {
    saveTransactions();
}

void MainWindow::onDateSelected(const QDate &date) {
    selectedDate = date;
    updateEventList(date);
    updateBalances();          // ← important: refresh both labels
    calendar->update();
}

void MainWindow::onAddButtonClicked() {
    AddTransactionDialog dialog(selectedDate, this);
    if (dialog.exec() == QDialog::Accepted) {
        Transaction trans;
        trans.startDate = selectedDate;
        trans.description = dialog.getDescription().trimmed();
        trans.amount = dialog.getAmount();
        trans.recurrence = dialog.getRecurrence();

        if (trans.recurrence == RecurrenceType::EveryNMonths) {
            trans.intervalMonths = dialog.getIntervalMonths();
        } else if (trans.recurrence == RecurrenceType::Monthly) {
            trans.intervalMonths = 1;
        } // else ignored

        trans.id = nextTransactionId++;
        transactions.append(trans);
        // ... save, update, etc.

    // if (dialog.exec() == QDialog::Accepted) {
    //     Transaction trans;
    //     trans.startDate = selectedDate;
    //     trans.description = dialog.getDescription().trimmed();
    //     trans.amount = dialog.getAmount();
    //     trans.recurrence = dialog.getRecurrence();
    //     trans.id = nextTransactionId++;
    //     transactions.append(trans);
        saveTransactions();
        updateEventList(selectedDate);
        updateBalances();
        calendar->update();
    }
}

void MainWindow::onDeleteButtonClicked() {
    QList<QListWidgetItem*> selected = eventList->selectedItems();
    if (selected.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete", "Delete selected transaction(s)?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QSet<int> idsToDelete;
    for (QListWidgetItem *item : selected) {
        QString text = item->text();
        for (const auto &t : transactions) {
            QString expected = t.description + " (" + QString::number(t.amount, 'f', 2) + ")";
            if (t.recurrence != RecurrenceType::None) {
                expected += " [" + recurrenceToString(t) + "]";       // ← change to recurrenceToString(t)
            }
            if (text == expected) {
                idsToDelete.insert(t.id);
                break;
            }
        }
    }

    for (int i = transactions.size() - 1; i >= 0; --i) {
        if (idsToDelete.contains(transactions[i].id)) {
            transactions.removeAt(i);
        }
    }

    saveTransactions();
    updateEventList(selectedDate);
    updateBalances();
    calendar->update();
}

void MainWindow::onEventSelectionChanged() {
    deleteButton->setEnabled(!eventList->selectedItems().isEmpty());
}

void MainWindow::updateEventList(const QDate &date) {
    eventList->clear();
    for (const auto &trans : transactions) {
        if (isTransactionOnDate(trans, date)) {
            QString itemText = trans.description + " (" + QString::number(trans.amount, 'f', 2) + ")";
            if (trans.recurrence != RecurrenceType::None) {
                itemText += " [" + recurrenceToString(trans) + "]";   // ← pass trans, not trans.recurrence
            }
            eventList->addItem(itemText);
        }
    }
}


void MainWindow::updateBalances() {
    QDate today = QDate::currentDate();
    double current = calculateBalance(today);
    currentBalanceLabel->setText("Current Balance (today): $" + QString::number(current, 'f', 2));

    double selectedBalance = calculateBalance(selectedDate);
    QString dateStr = selectedDate.toString("yyyy-MM-dd");
    if (selectedDate == today) {
        selectedDateBalanceLabel->setText("Balance on selected date (today): $" + QString::number(selectedBalance, 'f', 2));
    } else if (selectedDate < today) {
        selectedDateBalanceLabel->setText("Historical Balance on " + dateStr + ": $" + QString::number(selectedBalance, 'f', 2));
    } else {
        selectedDateBalanceLabel->setText("Projected Balance on " + dateStr + ": $" + QString::number(selectedBalance, 'f', 2));
    }
}

double MainWindow::calculateBalance(const QDate &upToDate) const {
    double balance = 0.0;

    for (const auto &trans : transactions) {
        if (trans.recurrence == RecurrenceType::None) {
            if (trans.startDate <= upToDate) {
                balance += trans.amount;
            }
        }
        else if (trans.recurrence == RecurrenceType::Weekly) {
            QDate current = trans.startDate;
            while (current <= upToDate) {
                balance += trans.amount;
                current = current.addDays(7);
            }
        }
        else if (trans.recurrence == RecurrenceType::BiWeekly) {
            QDate current = trans.startDate;
            while (current <= upToDate) {
                balance += trans.amount;
                current = current.addDays(14);
            }
        }
        else {  // Monthly or EveryNMonths
            int interval = (trans.recurrence == RecurrenceType::Monthly) ? 1 : trans.intervalMonths;
            QDate current = trans.startDate;
            while (current <= upToDate) {
                balance += trans.amount;
                current = current.addMonths(interval);
            }
        }
    }
    return balance;
}

bool MainWindow::isTransactionOnDate(const Transaction &trans, const QDate &date) const {
    if (date < trans.startDate) return false;

    switch (trans.recurrence) {
    case RecurrenceType::None:
        return trans.startDate == date;

    case RecurrenceType::Weekly:
    {
        int daysDiff = trans.startDate.daysTo(date);
        return (daysDiff >= 0) && (daysDiff % 7 == 0);
    }

    case RecurrenceType::BiWeekly:
    {
        int daysDiff = trans.startDate.daysTo(date);
        return (daysDiff >= 0) && (daysDiff % 14 == 0);
    }

    case RecurrenceType::Monthly:
    case RecurrenceType::EveryNMonths:
    {
        int monthsDiff = (date.year() - trans.startDate.year()) * 12
                         + (date.month() - trans.startDate.month());
        if (monthsDiff < 0) return false;

        int interval = (trans.recurrence == RecurrenceType::Monthly)
                           ? 1 : trans.intervalMonths;

        if (monthsDiff % interval != 0) return false;

        int targetDay = trans.startDate.day();
        int actualDay = (targetDay > date.daysInMonth()) ? date.daysInMonth() : targetDay;
        return date.day() == actualDay;
    }
    }

    return false;
}

QString MainWindow::recurrenceToString(const Transaction &trans) const {
    switch (trans.recurrence) {
    case RecurrenceType::None:        return "";
    case RecurrenceType::Weekly:      return "Weekly";
    case RecurrenceType::BiWeekly:    return "Bi-weekly";
    case RecurrenceType::Monthly:     return "Monthly";
    case RecurrenceType::EveryNMonths:
        return QString("Every %1 months").arg(trans.intervalMonths);
    }
    return "";
}

void MainWindow::saveTransactions() const {
    QJsonArray jsonArray;
    for (const auto &trans : transactions) {
        QJsonObject obj;
        obj["startDate"]     = trans.startDate.toString(Qt::ISODate);
        obj["description"]   = trans.description;
        obj["amount"]        = trans.amount;
        obj["recurrence"]    = static_cast<int>(trans.recurrence);
        obj["intervalMonths"] = trans.intervalMonths;     // important for the new every-N-months feature
        obj["id"]            = trans.id;
        jsonArray.append(obj);
    }

    QJsonObject root;
    root["transactions"] = jsonArray;
    root["nextId"]       = nextTransactionId;

    QJsonDocument doc(root);

    QString filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       + "/transactions.json";

    QDir dir;
    dir.mkpath(QFileInfo(filePath).absolutePath());   // make sure folder exists

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    } else {
        qWarning() << "Could not save transactions:" << file.errorString();
    }
}

void MainWindow::loadTransactions() {
    QString filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    + "/transactions.json";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;  // first run → no file yet, that's fine
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON in transactions file";
        return;
    }

    QJsonObject root = doc.object();

    nextTransactionId = root["nextId"].toInt(0);

    QJsonArray jsonArray = root["transactions"].toArray();
    for (const auto &val : jsonArray) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        Transaction trans;
        trans.startDate     = QDate::fromString(obj["startDate"].toString(), Qt::ISODate);
        trans.description   = obj["description"].toString();
        trans.amount        = obj["amount"].toDouble();
        trans.recurrence    = static_cast<RecurrenceType>(obj["recurrence"].toInt());
        trans.intervalMonths = obj["intervalMonths"].toInt(1);   // default 1 if missing
        trans.id            = obj["id"].toInt(-1);

        // Safety: assign new ID if corrupted/missing
        if (trans.id < 0) {
            trans.id = nextTransactionId++;
        } else {
            nextTransactionId = qMax(nextTransactionId, trans.id + 1);
        }

        transactions.append(trans);
    }
}

// AddTransactionDialog implementation
AddTransactionDialog::AddTransactionDialog(const QDate &date, QWidget *parent)
    : QDialog(parent),
    descEdit(new QLineEdit),
    amountSpin(new QDoubleSpinBox),
    recurrenceCombo(new QComboBox),
    intervalSpin(new QSpinBox){

    setWindowTitle("Add Transaction - " + date.toString("yyyy-MM-dd"));

    amountSpin->setRange(-1000000.0, 1000000.0);
    amountSpin->setDecimals(2);
    amountSpin->setPrefix("$");

    recurrenceCombo->addItem("One-time", static_cast<int>(RecurrenceType::None));
    recurrenceCombo->addItem("Weekly", static_cast<int>(RecurrenceType::Weekly));
    recurrenceCombo->addItem("Every 2 weeks", static_cast<int>(RecurrenceType::BiWeekly));
    recurrenceCombo->addItem("Monthly (same/last day)", static_cast<int>(RecurrenceType::Monthly));
    recurrenceCombo->addItem("Every 2 months", static_cast<int>(RecurrenceType::EveryNMonths));
    recurrenceCombo->addItem("Every 3 months", static_cast<int>(RecurrenceType::EveryNMonths));
    recurrenceCombo->addItem("Every 4 months", static_cast<int>(RecurrenceType::EveryNMonths));

    intervalSpin->setRange(2, 12);
    intervalSpin->setValue(2);
    intervalSpin->setSuffix(" months");
    intervalSpin->setEnabled(false);  // initially hidden/disabled

    // Show/hide interval spinbox based on selection
    connect(recurrenceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        RecurrenceType type = static_cast<RecurrenceType>(recurrenceCombo->itemData(index).toInt());
        bool showInterval = (type == RecurrenceType::EveryNMonths);
        intervalSpin->setEnabled(showInterval);
        intervalSpin->setVisible(showInterval);
    });

    QFormLayout *form = new QFormLayout;
    form->addRow("Description:", descEdit);
    form->addRow("Amount:", amountSpin);
    form->addRow("Recurrence:", recurrenceCombo);
    form->addRow("Interval:", intervalSpin);  // will be shown conditionally

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);
}

QString AddTransactionDialog::getDescription() const {
    return descEdit->text();
}

double AddTransactionDialog::getAmount() const {
    return amountSpin->value();
}

RecurrenceType AddTransactionDialog::getRecurrence() const {
    return static_cast<RecurrenceType>(recurrenceCombo->currentData().toInt());
}

// New getter
int AddTransactionDialog::getIntervalMonths() const {
    return intervalSpin->value();
}

void CustomCalendar::setBalanceCalculator(MainWindow *mw) {
    mainWindow = mw;
}
