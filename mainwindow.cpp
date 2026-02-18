// mainwindow.cpp (updated)
#include "mainwindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    calendar(new QCalendarWidget),
    eventList(new QListWidget),
    addButton(new QPushButton("Add Transaction")),
    deleteButton(new QPushButton("Delete Selected")),
    balanceLabel(new QLabel("Balance: $0.00")) {

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

    mainLayout->addWidget(balanceLabel);

    setCentralWidget(centralWidget);

    connect(calendar, &QCalendarWidget::clicked, this, &MainWindow::onDateSelected);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddButtonClicked);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteButtonClicked);
    connect(eventList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEventSelectionChanged);

    onDateSelected(QDate::currentDate());
}

MainWindow::~MainWindow() {}

void MainWindow::onDateSelected(const QDate &date) {
    selectedDate = date;
    updateEventList(date);
}

void MainWindow::onAddButtonClicked() {
    AddTransactionDialog dialog(selectedDate, this);
    if (dialog.exec() == QDialog::Accepted) {
        Transaction trans;
        trans.startDate = selectedDate;
        trans.description = dialog.getDescription().trimmed();
        trans.amount = dialog.getAmount();
        trans.recurrence = dialog.getRecurrence();
        trans.id = nextTransactionId++;
        transactions.append(trans);
        updateEventList(selectedDate);
        updateBalance();
    }
}

void MainWindow::onDeleteButtonClicked() {
    QList<QListWidgetItem*> selected = eventList->selectedItems();
    if (selected.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete", "Delete selected transaction(s)?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    for (QListWidgetItem *item : selected) {
        int index = eventList->row(item);
        // Find matching transaction by checking displayed text (simple but not perfect)
        // Better would be to store transaction pointer or ID in item data
        QString text = item->text();
        for (int i = 0; i < transactions.size(); ++i) {
            const Transaction &t = transactions[i];
            QString expected = t.description + " (" + QString::number(t.amount, 'f', 2) + ")";
            if (t.recurrence != RecurrenceType::None) {
                expected += " [" + recurrenceToString(t.recurrence) + "]";
            }
            if (text == expected) {
                transactions.removeAt(i);
                break;
            }
        }
    }

    updateEventList(selectedDate);
    updateBalance();
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
                itemText += " [" + recurrenceToString(trans.recurrence) + "]";
            }
            eventList->addItem(itemText);
        }
    }
}

void MainWindow::updateBalance() {
    double balance = 0.0;
    QDate today = QDate::currentDate();

    for (const auto &trans : transactions) {
        if (trans.recurrence == RecurrenceType::None) {
            if (trans.startDate <= today) {
                balance += trans.amount;
            }
        }
        else if (trans.recurrence == RecurrenceType::BiWeekly) {
            QDate current = trans.startDate;
            while (current <= today) {
                balance += trans.amount;
                current = current.addDays(14);
            }
        }
        else if (trans.recurrence == RecurrenceType::Monthly) {
            QDate current = trans.startDate;
            while (current <= today) {
                balance += trans.amount;
                // Move to next month, same day (Qt handles overflow nicely)
                current = current.addMonths(1);
            }
        }
    }

    balanceLabel->setText("Balance: $" + QString::number(balance, 'f', 2));
}

bool MainWindow::isTransactionOnDate(const Transaction &trans, const QDate &date) const {
    if (date < trans.startDate) return false;

    if (trans.recurrence == RecurrenceType::None) {
        return trans.startDate == date;
    }
    else if (trans.recurrence == RecurrenceType::BiWeekly) {
        int daysDiff = trans.startDate.daysTo(date);
        return daysDiff % 14 == 0;
    }
    else if (trans.recurrence == RecurrenceType::Monthly) {
        return (date.day() == trans.startDate.day()) &&
               (date.month() != trans.startDate.month() || date.year() != trans.startDate.year());
    }
    return false;
}

QString MainWindow::recurrenceToString(RecurrenceType type) const {
    switch (type) {
    case RecurrenceType::BiWeekly: return "Bi-weekly";
    case RecurrenceType::Monthly:  return "Monthly";
    default: return "";
    }
}

// ──────────────────────────────────────────────
// AddTransactionDialog implementation
// ──────────────────────────────────────────────

AddTransactionDialog::AddTransactionDialog(const QDate &date, QWidget *parent)
    : QDialog(parent),
    descEdit(new QLineEdit),
    amountSpin(new QDoubleSpinBox),
    recurrenceCombo(new QComboBox) {

    setWindowTitle("Add Transaction - " + date.toString("yyyy-MM-dd"));

    amountSpin->setRange(-1000000.0, 1000000.0);
    amountSpin->setDecimals(2);
    amountSpin->setPrefix("$");

    recurrenceCombo->addItem("One-time", static_cast<int>(RecurrenceType::None));
    recurrenceCombo->addItem("Every 2 weeks", static_cast<int>(RecurrenceType::BiWeekly));
    recurrenceCombo->addItem("Monthly (same day)", static_cast<int>(RecurrenceType::Monthly));

    QFormLayout *form = new QFormLayout;
    form->addRow("Description:", descEdit);
    form->addRow("Amount:", amountSpin);
    form->addRow("Recurrence:", recurrenceCombo);

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
