#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QCloseEvent>
#include <QHash>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QHashIterator>
#include <QResizeEvent>
#include <QLabel>
#include <QPixmap>
#include <QImage>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void closeEvent(QCloseEvent *);
    void resizeEvent(QResizeEvent * = 0);
    int varRowNum;
    long currentAttackNum;
    bool canExit;
    bool TAIT_RADIO_CONNECTED;
    QList<QSerialPortInfo> myPorts;
    QLabel *myStatus;
    QSerialPort mySerial;
    QImage pTick;
    QImage smTick;
    QImage pCross;
    QImage smCross;
    QString HextoAscii(QString);
    QString AsciitoHex(QString);
    QString taitAlphabetB[32];
    QStringList taitQueryOrder;
    QStringList taitSupportedModels;
    QStandardItemModel *myModel;
    QHash<QString,QString> taitAlphabet;
    QHash<QString,QString> taitBands;
    QHash<QString,QString> hexTable;
    QHash<QString,QString> bitTable;
    QHash<QString,QString> taitFeatures;
    QHash<QString,QString> attackDetails;
    QHash<QString,QString> radioInfo;
    void sleep(int);
    void LoadConfiguration();
    void resetButtons();
    void resetDisplay();
    void taitInterrogate();
    void taitKeysFromSFEKey(QString);
    QString taitKeyFromHex(QString);
    QString taitHexFromKey(QString);
    QString taitEncodeKey(QString);
    QString taitWrite(QString,QString = ">");
    QString taitChecksum(QString);
    QString taitChecksum16(QString);
    QString taitDecode(QString);
    QString taithswap(QString);
    QString TEAEncrypt(QString,QString);
    QString TEADecrypt(QString,QString);
    QStringList taitGenerateSFE(QString, int = 1);
    QStringList taitSFETest(QString);   // Returns[4 QStrings] [0] =  ENABLED/DISABLED, [1] = Current Sequence, [2] = Next in sequence, [3] Feature Number
private slots:
    void on_butInspect_clicked();
    void on_cmbPorts_currentIndexChanged(const QString &arg1);
    void on_actionApplication_triggered();
    void on_butLoadKeys_clicked();
    void tait_is_now_connected();
    void tait_is_now_disconnected();
    void tait_no_reply();
    void tait_inspected();
    void on_butDisableAllKeys_clicked();
    void on_winMain_customContextMenuRequested(const QPoint &pos);
    void toggleOneSFE();
    void doKeys(QString = "ENABLEALL");
    void on_actionFull_SFE_List_triggered();
    void on_actionAdvanced_Trunking_Keys_triggered();
    void on_actionKeyloader_triggered();
public:
#ifdef QT_DEBUG
    void testfunction();
#endif
private:
    Ui::MainWindow *ui;
signals:
    void TAIT_RADIO_NOREPLY();
    void TAIT_RADIO_FINISHED_INSPECTION();
    void TAIT_CONNECTED();
    void TAIT_DISCONNECTED();
};

#endif // MAINWINDOW_H
