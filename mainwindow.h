// mainwindow.h (updated)
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCalendarWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QDate>
#include <QString>
#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

enum class RecurrenceType {
    None,
    BiWeekly,
    Monthly
};

struct Transaction {
    QDate startDate;
    QString description;
    double amount;
    RecurrenceType recurrence = RecurrenceType::None;
    int id = -1;  // unique identifier for deletion
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onDateSelected(const QDate &date);
    void onAddButtonClicked();
    void onDeleteButtonClicked();
    void onEventSelectionChanged();

private:
    QCalendarWidget *calendar;
    QListWidget *eventList;
    QPushButton *addButton;
    QPushButton *deleteButton;
    QLabel *balanceLabel;
    QVector<Transaction> transactions;
    QDate selectedDate;
    int nextTransactionId = 0;

    void updateEventList(const QDate &date);
    void updateBalance();
    bool isTransactionOnDate(const Transaction &trans, const QDate &date) const;
    QString recurrenceToString(RecurrenceType type) const;
};

class AddTransactionDialog : public QDialog {
    Q_OBJECT

public:
    AddTransactionDialog(const QDate &date, QWidget *parent = nullptr);
    QString getDescription() const;
    double getAmount() const;
    RecurrenceType getRecurrence() const;

private:
    QLineEdit *descEdit;
    QDoubleSpinBox *amountSpin;
    QComboBox *recurrenceCombo;
};

#endif // MAINWINDOW_H
