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
#include <QPainter>
#include <QTextCharFormat>
#include <QSpinBox>

enum class RecurrenceType {
    None,
    Weekly,
    BiWeekly,
    Monthly,       // = every 1 month
    EveryNMonths   // 2,3,4,... months
};

struct Transaction {
    QDate startDate;
    QString description;
    double amount;
    RecurrenceType recurrence = RecurrenceType::None;
    int intervalMonths = 1;   // only used when recurrence == EveryNMonths
    int id = -1;
};

class MainWindow;

class CustomCalendar : public QCalendarWidget {
    Q_OBJECT

public:
    CustomCalendar(QWidget *parent = nullptr);
    void setTransactions(const QVector<Transaction> *trans);
    void setBalanceCalculator(MainWindow *mw);

protected:
    // Changed: remove & from the third parameter
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override;

private:
    const QVector<Transaction> *transactions = nullptr;
    MainWindow *mainWindow = nullptr;           // NEW
    double getNetAmountOnDate(const QDate &date) const;
    bool isTransactionOnDate(const Transaction &trans, const QDate &date) const;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class CustomCalendar;
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool isTransactionOnDate(const Transaction &trans, const QDate &date) const;

private slots:
    void onDateSelected(const QDate &date);
    void onAddButtonClicked();
    void onDeleteButtonClicked();
    void onEventSelectionChanged();

private:
    CustomCalendar *calendar;
    QListWidget *eventList;
    QPushButton *addButton;
    QPushButton *deleteButton;
    QLabel *currentBalanceLabel;
    QLabel *selectedDateBalanceLabel;   // ← changed name for clarity
    QVector<Transaction> transactions;
    QDate selectedDate;
    int nextTransactionId = 0;
    QString recurrenceToString(const Transaction &trans) const;

    void updateEventList(const QDate &date);
    void updateBalances();
    void saveTransactions() const;
    void loadTransactions();

    double calculateBalance(const QDate &upToDate) const;
};

class AddTransactionDialog : public QDialog {
    Q_OBJECT

public:
    AddTransactionDialog(const QDate &date, QWidget *parent = nullptr);
    QString getDescription() const;
    double getAmount() const;
    RecurrenceType getRecurrence() const;
    int getIntervalMonths() const;     // ← MUST be here

private:
    QLineEdit *descEdit;
    QDoubleSpinBox *amountSpin;
    QComboBox *recurrenceCombo;
    QSpinBox *intervalSpin;  // NEW - shown only for Every N Months
};

#endif // MAINWINDOW_H
