#ifndef TASKDIALOG_H
#define TASKDIALOG_H
#include <QDialog>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QString>
#include <QPixmap>
#include <QImage>
namespace Ui {
class taskDialog;
}

class taskDialog : public QDialog {
    Q_OBJECT
public:
    explicit taskDialog(QWidget *parent = 0);
    ~taskDialog();
    void resizeEvent(QResizeEvent * = 0);
private slots:
    void on_butADD_clicked();
    void on_txtSYSID_textEdited(const QString &arg1);
    void on_txtWACN_textEdited(const QString &arg1);
    void on_winKeys_customContextMenuRequested(const QPoint &pos);
    void deleteTASK();
private:
    QImage pTick;
    QImage smTick;
    QImage pCross;
    QImage smCross;
    Ui::taskDialog *ui;
    QStandardItemModel *myModel;
    QStringList keyFiles;
    void drawWindow();
    int currentRow;
    QString TEAEncrypt(QString,QString);
    QString TEADecrypt(QString,QString);
    bool TEACheckKey(QString);
};

#endif // TASKDIALOG_H
