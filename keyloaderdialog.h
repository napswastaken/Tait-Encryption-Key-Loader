#ifndef KEYLOADERDIALOG_H
#define KEYLOADERDIALOG_H
#include <QDialog>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QString>
#include <QImage>

namespace Ui {
    class KeyloaderDialog;
}

class KeyloaderDialog : public QDialog{
    Q_OBJECT

public:
    explicit KeyloaderDialog(QWidget *parent = 0);
    ~KeyloaderDialog();

private slots:
    void on_butAddKey_clicked();
    void on_butClear_clicked();
    void on_butEraseAll_clicked();
    void on_butSend_clicked();
    void on_butGetInventory_clicked();
    void on_cmbKeyType_currentIndexChanged(const QString &arg1);

private:
    Ui::KeyloaderDialog *ui;
    QStandardItemModel *newKeys;
    QStandardItemModel *keyInventory;
    void disableButtons();
    void enableButtons();
    void enterKeyloaderMode();
    void exitKeyloaderMode();
};

#endif // KEYLOADERDIALOG_H
