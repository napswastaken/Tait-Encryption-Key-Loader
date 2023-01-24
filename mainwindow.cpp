#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QByteArray>
#include <QSize>
#include <QDataStream>
#include <QMenu>
#include <QIODevice>
#include <QApplication>
#include <QPixmap>
#include <QMessageBox>
#include <QColor>
#include <QBrush>
#include <QRect>
#include <QFont>
#include <QPushButton>
#include <QTextStream>
#include <QVariant>
#include <QDir>
#include <QFile>
#include <math.h>
#include <QChar>
#include <taskdialog.h>
#include <keyloaderdialog.h>

using namespace std;
// Now for some static stuff worked out by mnix.
// These values are taken from the 8200 Firmware. MAGIC1 is obviously used in the generation of SFE keys
static const QString TAIT_MAGIC1 = "3467E59B1F95FEB9";                              // Predefined magic value defined in firmware
static const QString TAIT_MAGIC2 = "52F3F3DCEA43E28C";                              // Predefined magic value defined in firmware
static const QString TAIT_8000_KEY1 = "9DE3BF98700686E10100000013200000";           // Memory location 40004B00
static const QString TAIT_8000_KEY2 = "D00240009410200090122800D0224000";           // Memory location 40004B10
static const QString TAIT_8000_KEY3 = "90102090D0328000F010210096103C00";           // Memory location 40004B20
static const QString TAIT_8000_UNK1 = "0000010A0000900A000000000009000A";           // Memory location 4004FCE0
static const QString TAIT_8000_UNK2 = "0000000100040009000900020003000500060007";   // Memory location 400037BC

// List of features different models have. Restrict to only known functions to make sure we don't cause problems.
static const QStringList TM8200_FEATURES = QString("00 01 02 05 06 07 08 09 0A 35").split(" ");
static const QStringList TM9100_FEATURES = QString("05 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 20 21 22 23 25 26 27 29 2A 2B 2C 35 3B 3F 44").split(" ");
static const QStringList TM9300_FEATURES = QString("1A 1F 2A 2D 32 33 35 39 3B 40 41 43 48 49 4B 4C 4D 4E").split(" ");
static const QStringList TP9100_FEATURES = QString("05 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 21 22 23 25 29 2A 2B 35 3B 3F 44").split(" ");

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
#ifndef QT_DEBUG
    ui->menuDebug->setVisible(false);
#endif
    QApplication::setApplicationVersion("1.18");
    QApplication::setApplicationName("Tait Key Management Utility");
    this->setWindowTitle(QApplication::applicationName() + " ver " + QApplication::applicationVersion());
    myPorts = QSerialPortInfo::availablePorts();
    TAIT_RADIO_CONNECTED = false;
    canExit=true;
    connect(this,SIGNAL(TAIT_CONNECTED()),this,SLOT(tait_is_now_connected()));
    connect(this,SIGNAL(TAIT_DISCONNECTED()),this,SLOT(tait_is_now_disconnected()));
    connect(this,SIGNAL(TAIT_RADIO_FINISHED_INSPECTION()),this,SLOT(tait_inspected()));
    connect(this,SIGNAL(TAIT_RADIO_NOREPLY()),this,SLOT(tait_no_reply()));
    LoadConfiguration(); // Setup the variables.
    myStatus = new QLabel();
    myStatus->setStyleSheet("font-family:Courier;");
    myStatus->setText("");
    ui->statusBar->setLayoutDirection(Qt::LeftToRight);
    ui->statusBar->addWidget(myStatus);
    ui->winMain->horizontalHeader()->setStretchLastSection(true);
    ui->winMain->setUpdatesEnabled(false);
    myModel = new QStandardItemModel(taitFeatures.size(),3,this);
    myModel->setHorizontalHeaderLabels(QString("|Feature|Key").split("|"));
    QStandardItem *myItem;
    for (int a=0;a<myModel->rowCount();a++) {
        for (int b=0;b<myModel->columnCount();b++) {
            myItem = new QStandardItem;
            if (b < 2) myItem->setTextAlignment(Qt::AlignVCenter);
            if (b == 2) myItem->setTextAlignment(Qt::AlignVCenter|Qt::AlignHCenter);
            myItem->setFlags(myItem->flags() ^ Qt::ItemIsEditable);
            myModel->setItem(a,b,myItem);
        }
        ui->winMain->hideRow(a);
    }
    ui->winMain->setModel(myModel);
    ui->cmbPorts->clear();
    ui->butInspect->setEnabled(false);
    ui->butInspect->setCursor(Qt::PointingHandCursor);
    ui->butLoadKeys->setEnabled(false);
    ui->butLoadKeys->setCursor(Qt::PointingHandCursor);
    ui->butDisableAllKeys->setEnabled(false);
    ui->butDisableAllKeys->setCursor(Qt::PointingHandCursor);
    ui->txtModel->setText("");
    ui->txtChassis->setText("");
#ifdef WIN32
    foreach (QSerialPortInfo myPort,myPorts) { ui->cmbPorts->addItem(myPort.portName()); }
#else
    foreach(QSerialPortInfo myPort,myPorts) { ui->cmbPorts->addItem(myPort.systemLocation()); }
#endif
    if (ui->cmbPorts->count() > 0) ui->butInspect->setEnabled(true);
    ui->winMain->setUpdatesEnabled(true);
    currentAttackNum = 0;
    resetDisplay();
    resetButtons();
    ui->txtModel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->txtChassis->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->txtBand->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->txtFlash->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->txtFirmware_version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pTick = QImage(":/tick.png");
    smTick = pTick.scaled(14,14,Qt::KeepAspectRatio);
    pCross = QImage(":/cross.png");
    smCross = pCross.scaled(14,14,Qt::KeepAspectRatio);
    ui->butDisableAllKeys->setEnabled(false);
    ui->butLoadKeys->setVisible(false);
    ui->butDisableAllKeys->setVisible(false);
    ui->actionFull_SFE_List->setVisible(false);     // This is a dangerous test option. It can brick radios. Danger Will Robinson, Danger!
    sleep(2000);
#ifdef QT_DEBUG
    testfunction();
    ui->actionFull_SFE_List->setVisible(true);
    ui->actionFull_SFE_List->setChecked(false);
    ui->menuDebug->setChecked(true);
#endif
}

MainWindow::~MainWindow() {
    disconnect(this,SIGNAL(TAIT_CONNECTED()),this,SLOT(tait_is_now_connected()));
    disconnect(this,SIGNAL(TAIT_DISCONNECTED()),this,SLOT(tait_is_now_disconnected()));
    disconnect(this,SIGNAL(TAIT_RADIO_FINISHED_INSPECTION()),this,SLOT(tait_inspected()));
    disconnect(this,SIGNAL(TAIT_RADIO_NOREPLY()),this,SLOT(tait_no_reply()));
    delete ui;
}

void MainWindow::on_butInspect_clicked() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString myReturn, tmpStr, varCmd;
    ui->cmbPorts->setEnabled(false);
    ui->butInspect->setEnabled(false);
    ui->butLoadKeys->setEnabled(false);
    ui->butDisableAllKeys->setEnabled(false);
    ui->butInspect->setStyleSheet("background-color:gray");
    ui->butLoadKeys->setStyleSheet("background-color:gray");
    ui->butDisableAllKeys->setStyleSheet("background-color:gray");
    resetDisplay();
    QApplication::processEvents();      // Refresh the display
    ui->cmbPorts->setFocus();
    canExit=false;
    myStatus->setText("Attempting to connect to the Tait Radio on " + ui->cmbPorts->currentText());
    myReturn = taitWrite("^","v");      // Attempt to contact the radio
    if (myReturn=="ERROR") return;
    myReturn = taitWrite("%","-");      // Lets start with TEST MODE
    if (myReturn=="ERROR") return;
    if (mySerial.isOpen()) emit TAIT_CONNECTED();
    taitInterrogate();
    myStatus->setText("Switching Tait Radio on " + ui->cmbPorts->currentText() + " to programming mode to continue interrogation");
    myReturn = taitWrite("^","v");
    if (myReturn=="ERROR") return;
    myReturn = taitWrite("#","v");       // Move to PROGRAMMING MODE
    if (myReturn=="ERROR") return;
    QStringList myCmds = QString("ld p01 r00 r22").split(" ");
    QStringList varList;
    foreach(varCmd,myCmds) {
        myReturn = taitWrite(varCmd,">");
        if (myReturn=="ERROR") break;
        varList = myReturn.split("\r");
        foreach(tmpStr,varList) {
            tmpStr.chop(2);
            if (varCmd=="r22") { radioInfo["R22"] = tmpStr.mid(4).trimmed(); }
            if ((varCmd=="r00") && (tmpStr.left(4)=="0008")) { ui->txtModel->setText(HextoAscii(tmpStr.mid(6)).trimmed().toUpper()); radioInfo["MODEL"] = ui->txtModel->text(); }
        }
    }
    if (!TAIT_RADIO_CONNECTED) { QMessageBox::warning(this,"ERROR","The radio stopped responding.",QMessageBox::Ok); resetDisplay(); resetButtons(); return; }
    if (radioInfo["MODEL"].startsWith("TM8200") && (radioInfo["BOOTVER"]!="1")) { ui->txtModel->setText(ui->txtModel->text() + " (boot code ver " + radioInfo.value("BOOTVER") + ")"); } // Workaround only applies to TM8000's
    // Now we have checked for the extra information lets move on to retrieving the SFE keys currently in the radio.
    QStandardItem *myItem;
    int a = 0;
    QStringList keyStatus;
    myStatus->setText("Now querying SFE keys for Tait Radio connected to " + ui->cmbPorts->currentText());
    bool canSFEcheck = true;
#ifdef QT_DEBUG
    if (ui->actionFull_SFE_List->isChecked()) canSFEcheck = false; // A remarkably dangerous option - ALL SFE's can be checked for and changed. It could do bad things.
#endif
    foreach(QString varCmd,taitQueryOrder) {
        // Skip commands that we don't think are compatible with this model radio.
        if (canSFEcheck) {
            if ( radioInfo["MODEL"].startsWith("TM8200") && ! TM8200_FEATURES.contains(varCmd) ) { continue; }  // Covers the TM8200 series
            if ( radioInfo["MODEL"].startsWith("TM9000") && ! TM9100_FEATURES.contains(varCmd) ) { continue; }  // Covers the TM9100 with HHCH
            if ( radioInfo["MODEL"].startsWith("TM9100") && ! TM9100_FEATURES.contains(varCmd) ) { continue; }  // Covers the TM9155
            if ( radioInfo["MODEL"].startsWith("TM9300") && ! TM9300_FEATURES.contains(varCmd) ) { continue; }  // Covers the TM9300
            if ( radioInfo["MODEL"].startsWith("TP9100") && ! TP9100_FEATURES.contains(varCmd) ) { continue; }  // Covers the TP9100 HH
        }
        tmpStr = taitChecksum("02" + varCmd);
        tmpStr.prepend("f");
        myReturn = taitWrite(tmpStr,">");
        if (myReturn=="ERROR") break;
        if (myReturn.length()==32) keyStatus = taitSFETest(myReturn);
        if (myReturn=="ERROR") continue;
        if (myReturn=="01FF") {
            myReturn = "FEATURE NOT SUPPORTED";
            continue;
        } else if (myReturn=="{C02}") {
            myReturn = "INVALID CHECKSUM"; // Really hope not.
        } else if ((myReturn!="01FF") && (myReturn!="{C02}")) {
            if (ui->txtFlash->text().trimmed()=="") { // We don't already know the ESN, lets grab it now.
                bool ok;
                uint tnum = QString("0x" + myReturn.mid(20,7)).toUInt(&ok,16);
                tnum >>=1 ;
                ui->txtFlash->setText(QString::number(tnum,10).rightJustified(8,'0'));
                radioInfo["ESN"] = ui->txtFlash->text().trimmed();
            }
            myReturn = taitKeyFromHex(myReturn);
        }
        QFont myFont = QFont();
        (keyStatus.at(0)=="ENABLED") ? myFont.setBold(true) : myFont.setBold(false);
        myItem = myModel->item(a,0);
        if (keyStatus.at(0)=="ENABLED") myItem->setData(QVariant(QPixmap::fromImage(smTick)),Qt::DecorationRole);
        if (keyStatus.at(0)=="DISABLED") myItem->setData(QVariant(QPixmap::fromImage(smCross)),Qt::DecorationRole);
        myItem = myModel->item(a,1);
        myItem->setFont(myFont);
        myItem->setText(taitFeatures.value(varCmd));
        myItem->setTextAlignment(Qt::AlignVCenter);
        myItem = myModel->item(a,2);
        myItem->setFont(myFont);
        myItem->setText(myReturn.trimmed());
        myItem->setTextAlignment(Qt::AlignVCenter|Qt::AlignHCenter);
        ui->winMain->showRow(a);
        a++;
        if (radioInfo.value("BODY").startsWith("TMAB2") && (varCmd=="0A")) break;   // Limit of TM8200 features. No point delaying things by trying pointless options.
    }
    if (!TAIT_RADIO_CONNECTED) { QMessageBox::warning(this,"ERROR","The radio stopped responding.",QMessageBox::Ok); resetDisplay(); resetButtons(); return; }
    QFile myFile(QApplication::applicationDirPath() + "/radiodb/" + radioInfo.value("CHASSIS") + ".txt");
    if (myFile.exists()) myFile.remove();
    if (myFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream out(&myFile);
        QHashIterator<QString,QString> i(radioInfo);
        while (i.hasNext()) {
            i.next();
            tmpStr = i.key() + "=" + i.value();
            out << tmpStr << endl;
        }
        myFile.close();
    }
    myStatus->setText("All SFE Keys retrieved. Resetting the radio on " + ui->cmbPorts->currentText());
    myReturn = taitWrite("^","v"); // Reset the radio.
    sleep(4000);
    if (!radioInfo.contains("TEA")) radioInfo.insert("TEA","UNKNOWN");
    if (!radioInfo.contains("R22")) radioInfo.insert("R22","UNKNOWN");
    emit TAIT_RADIO_FINISHED_INSPECTION();
    emit TAIT_DISCONNECTED();
    myStatus->setText("Inspection of Tait Radio on " + ui->cmbPorts->currentText() + " complete.");
    resetButtons();
    ui->cmbPorts->setFocus();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (mySerial.isOpen() || (!canExit)) {
        event->ignore();
        QMessageBox::warning(this,"Error","Please wait for the radio to reset before closing the application");
        return;
    }
    if (QMessageBox::question(this,"Confirm","Are you sure?",QMessageBox::Yes,QMessageBox::No) == QMessageBox::No) { event->ignore(); return; }
    if (mySerial.isOpen()) {
        taitWrite("^",">");
        mySerial.close();
    }
}

void MainWindow::resizeEvent(QResizeEvent *) {
    QSize mySize;
    mySize = ui->winMain->size();
    ui->winMain->setColumnWidth(0,20);
    ui->winMain->setColumnWidth(1, mySize.width() * 0.5);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void MainWindow::on_cmbPorts_currentIndexChanged(const QString &arg1) {
    if (arg1==0) { }
    if (mySerial.isOpen()) mySerial.close();
    resetDisplay();
    resetButtons();
    ui->butInspect->setEnabled(true);
    QStandardItem *myItem;
    for (int a=0;a<myModel->rowCount();a++) {
        myItem = myModel->item(a,1);
        myItem->setText("");
        ui->winMain->hideRow(a);
    }

}

void MainWindow::sleep(int myInt) {
    QDateTime startTime = QDateTime::currentDateTime();
    while (startTime.msecsTo(QDateTime::currentDateTime()) < myInt) {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

QString MainWindow::HextoAscii(QString String) {
    QByteArray ByteArray1=String.toUtf8();
    const char* chArr1=ByteArray1.constData();
    QByteArray ByteArray2=QByteArray::fromHex(chArr1);
    const char* chArr2=ByteArray2.constData();
    return QString::fromUtf8(chArr2);
}
QString MainWindow::AsciitoHex(QString String) {
    QByteArray ByteArray1=String.toUtf8();
    QByteArray ByteArray2=ByteArray1.toHex();
    const char* chArr1=ByteArray2.constData();
    return QString::fromUtf8(chArr1);
}

void MainWindow::LoadConfiguration() {
    ui->menuDebug->setChecked(false);
    // Some tables to make conversion between bytes and 5 bit Tait Ascii easy. Adapted from CRCinAU's perl script.
    taitAlphabet.insert("00000","T"); // 00
    taitAlphabet.insert("00001","3"); // 01
    taitAlphabet.insert("00010","F"); // 02
    taitAlphabet.insert("00011","J"); // 03
    taitAlphabet.insert("00100","D"); // 04
    taitAlphabet.insert("00101","6"); // 05
    taitAlphabet.insert("00110","W"); // 06
    taitAlphabet.insert("00111","8"); // 07
    taitAlphabet.insert("01000","A"); // 08
    taitAlphabet.insert("01001","C"); // 09
    taitAlphabet.insert("01010","7"); // 10
    taitAlphabet.insert("01011","H"); // 11
    taitAlphabet.insert("01100","N"); // 12
    taitAlphabet.insert("01101","E"); // 13
    taitAlphabet.insert("01110","G"); // 14
    taitAlphabet.insert("01111","4"); // 15
    taitAlphabet.insert("10000","2"); // 16
    taitAlphabet.insert("10001","V"); // 17
    taitAlphabet.insert("10010","S"); // 18
    taitAlphabet.insert("10011","Z"); // 19
    taitAlphabet.insert("10100","5"); // 20
    taitAlphabet.insert("10101","P"); // 21
    taitAlphabet.insert("10110","Y"); // 22
    taitAlphabet.insert("10111","U"); // 23
    taitAlphabet.insert("11000","9"); // 24
    taitAlphabet.insert("11001","L"); // 25
    taitAlphabet.insert("11010","R"); // 26
    taitAlphabet.insert("11011","B"); // 27
    taitAlphabet.insert("11100","K"); // 28
    taitAlphabet.insert("11101","M"); // 29
    taitAlphabet.insert("11110","X"); // 30
    taitAlphabet.insert("11111","Q"); // 31
    taitAlphabetB[0] = "T"; //00
    taitAlphabetB[1] = "3"; // 01
    taitAlphabetB[2] = "F"; // 02
    taitAlphabetB[3] = "J"; // 03
    taitAlphabetB[4] = "D"; // 04
    taitAlphabetB[5] = "6"; // 05
    taitAlphabetB[6] = "W"; // 06
    taitAlphabetB[7] = "8"; // 07
    taitAlphabetB[8] = "A"; // 08
    taitAlphabetB[9] = "C"; // 09
    taitAlphabetB[10] = "7"; // 10
    taitAlphabetB[11] = "H"; // 11
    taitAlphabetB[12] = "N"; // 12
    taitAlphabetB[13] = "E"; // 13
    taitAlphabetB[14] = "G"; // 14
    taitAlphabetB[15] = "4"; // 15
    taitAlphabetB[16] = "2"; // 16
    taitAlphabetB[17] = "V"; // 17
    taitAlphabetB[18] = "S"; // 18
    taitAlphabetB[19] = "Z"; // 19
    taitAlphabetB[20] = "5"; // 20
    taitAlphabetB[21] = "P"; // 21
    taitAlphabetB[22] = "Y"; // 22
    taitAlphabetB[23] = "U"; // 23
    taitAlphabetB[24] = "9"; // 24
    taitAlphabetB[25] = "L"; // 25
    taitAlphabetB[26] = "R"; // 26
    taitAlphabetB[27] = "B"; // 27
    taitAlphabetB[28] = "K"; // 28
    taitAlphabetB[29] = "M"; // 29
    taitAlphabetB[30] = "X"; // 30
    taitAlphabetB[31] = "Q"; // 31
    hexTable.insert("0","0000");
    hexTable.insert("1","0001");
    hexTable.insert("2","0010");
    hexTable.insert("3","0011");
    hexTable.insert("4","0100");
    hexTable.insert("5","0101");
    hexTable.insert("6","0110");
    hexTable.insert("7","0111");
    hexTable.insert("8","1000");
    hexTable.insert("9","1001");
    hexTable.insert("A","1010");
    hexTable.insert("B","1011");
    hexTable.insert("C","1100");
    hexTable.insert("D","1101");
    hexTable.insert("E","1110");
    hexTable.insert("F","1111");
    bitTable.insert("0000","0");
    bitTable.insert("0001","1");
    bitTable.insert("0010","2");
    bitTable.insert("0011","3");
    bitTable.insert("0100","4");
    bitTable.insert("0101","5");
    bitTable.insert("0110","6");
    bitTable.insert("0111","7");
    bitTable.insert("1000","8");
    bitTable.insert("1001","9");
    bitTable.insert("1010","A");
    bitTable.insert("1011","B");
    bitTable.insert("1100","C");
    bitTable.insert("1101","D");
    bitTable.insert("1110","E");
    bitTable.insert("1111","F");
    // List of known Tait Features. If anyone knows of more (probably DMR ones) please publish them.
    taitFeatures.insert("00", "TMAS010 - Tait High Speed Data (8xxx)");
    taitFeatures.insert("01", "TMAS011/31 - MPT1327 Trunking (8xxx/93xx)");
    taitFeatures.insert("02", "TMAS012 - MDC1200 Encode (8xxx)");
    taitFeatures.insert("04", "TBAS050 - P25 Common Air Interface (TB9100)");
    taitFeatures.insert("05", "TxAS015 - GPS Display");
    taitFeatures.insert("06", "TMAS016 - Multi-Body Support (8xxx)");
    taitFeatures.insert("07", "TMAS017 - Multi-Head Support (8xxx)");
    taitFeatures.insert("08", "TMAS018 - TDMA Support (8xxx)");
    taitFeatures.insert("09", "TMAS019 - Conventional Call Queuing (8xxx) / TBAS055 - Transmit Enable (TB9100)");
    taitFeatures.insert("0A", "TMAS020 - Lone Worker Support");
    taitFeatures.insert("14", "TMAS050 - P25 Common Air Interface");
    taitFeatures.insert("15", "TMAS051 - P25 Administrator Services");
    taitFeatures.insert("16", "TMAS052 - P25 Graphical C/Head Operation");
    taitFeatures.insert("17", "TMAS053 - Single DES Encryption & Key Loading");
    taitFeatures.insert("18", "TMAS054 - P25 Base OTAR Re-Keying");
    taitFeatures.insert("19", "TMAS055 - P25 Trunking Services");
    taitFeatures.insert("1A", "TMAS056 - P25 User IP Data");
    taitFeatures.insert("1B", "TMAS057 - P25 DES Encryption & Key Loading");
    taitFeatures.insert("1C", "TMAS058 - P25 AES Encryption (req TMAS057)");
    taitFeatures.insert("1D", "TMAS059 - MDC1200");
    taitFeatures.insert("1E", "TMAS060 - Tait Radio API");
    taitFeatures.insert("1F", "TMAS061 - P25 Protocol API");
    taitFeatures.insert("20", "TMAS062 - P25 Digital Crossband");
    taitFeatures.insert("21", "TMAS063 - P25 DLI / Trunked OTAR");
    taitFeatures.insert("22", "TMAS064 - P25 Trunked PSTN");
    taitFeatures.insert("23", "TMAS065 - 2-Tone Decode");
    taitFeatures.insert("24", "TMAS066 - Reserved (P25 Diagnostic Menu)");
    taitFeatures.insert("25", "TMAS067 - GPS Transmission");
    taitFeatures.insert("26", "TMAS068 - TM9000 Multi-Body Support");
    taitFeatures.insert("27", "TMAS069 - TM9000 Multi-Head Support");
    taitFeatures.insert("28", "TMAS070 - APCO TCI");
    taitFeatures.insert("29", "TMAS071 - GPS Logging");
    taitFeatures.insert("2A", "TMAS072 - Alphanumeric ID");
    taitFeatures.insert("2B", "TMAS073 - Emergency Acknowledgement");
    taitFeatures.insert("2C", "TMAS074 - Terminal Repeater Detection Collision Avoidance");
    taitFeatures.insert("2D", "TPAS075 - OTAP (Over-The-Air Programming)");
    taitFeatures.insert("30", "TPAS078 - Tait Radio API");
    taitFeatures.insert("32", "TPAS080 - DMR Trunking Digital");
    taitFeatures.insert("33", "TPAS081 - GPS Hardware Location Services");
    taitFeatures.insert("34", "TPAS082 - Bluetooth");
    taitFeatures.insert("35", "TMAS083 - 20/25kHz Unrestricted Wideband");
    taitFeatures.insert("36", "TPAS084 - WiFi");
    taitFeatures.insert("38", "TPAS086 - Enhanced Channel Capacity");
    taitFeatures.insert("39", "TPAS087 - Voice Annunciations");
    taitFeatures.insert("3A", "TMAS088 - P25 Digital Crossband Remote Control");
    taitFeatures.insert("3B", "TMAS089 - Enhanced Location Reporting");
    taitFeatures.insert("3F", "TMAS093 - Keyloading");
    taitFeatures.insert("40", "TPAS094 - Unrestricted Dist DES Encryption (9335,9337)");
    taitFeatures.insert("41", "TPAS095 - DES Encryption (9355,9357)");
    taitFeatures.insert("43", "TPAS097 - DMR Conventional");
    taitFeatures.insert("44", "TPAS098 - Trunked GPS Transmission");
    taitFeatures.insert("45", "TPAS099 - BIOLINK");
    taitFeatures.insert("48", "TPAS102 - ARC4 Encryption");
    taitFeatures.insert("49", "TPAS103 - Radio Logging");
    taitFeatures.insert("4A", "TPAS104 - Extended Hunt List Capacity");
    taitFeatures.insert("4B", "TPAS105 - Geofencing Services");
    taitFeatures.insert("4C", "TMAS106 - File Transfer Services");
    taitFeatures.insert("4D", "TPAS107 - Embedded GPS Decoding");
    taitFeatures.insert("4E", "TMAS108 - Unify API");
    taitFeatures.insert("64", "TMAS130 - Tait Internal 4");
    taitFeatures.insert("65", "TMAS131 - Tait Internal D");
    taitQueryOrder.clear();         // Lets autofill in numeric order. QHash randomly changes the order of items and that irritates me.
    QString tmpStr;
    for (int a=0;a<255;a++) {
        tmpStr = QString::number(a,16).rightJustified(2,'0').toUpper();
        if (taitFeatures.contains(tmpStr)) taitQueryOrder.append(tmpStr);
    }
    //taitQueryOrder = QString("1B|1C|41").split("|");
    // List of known band splits for standard issue radios
    taitBands.insert("A4","VHF 66 - 88 Mhz");
    taitBands.insert("B1","VHF 136 - 174 Mhz");
    taitBands.insert("C0","VHF 174 - 225 Mhz");
    taitBands.insert("D1","VHF 216 - 266 Mhz");
    taitBands.insert("G2","UHF 350 - 400 Mhz");
    taitBands.insert("H5","UHF 400 - 470 Mhz");
    taitBands.insert("H6","UHF 450 - 530 Mhz");
    taitBands.insert("H7","UHF 450 - 520 Mhz");
    taitBands.insert("K5","UHF 762 - 870 Mhz");
    // Supported models for SFE loading
    taitSupportedModels = QString("TMAB22|TMAB23|TMAB24").split("|");
}

void MainWindow::resetButtons() {
    #ifdef WIN32
    ui->butInspect->setStyleSheet("background-color:darkgreen;color:white;border-style:ridge;border-width:1px;padding:2px;font-family:Courier;");
    ui->butLoadKeys->setStyleSheet("background-color:darkred;color:white;border-style:ridge;border-width:1px;padding:2px;font-family:Courier;");
    ui->butDisableAllKeys->setStyleSheet("background-color:darkblue;color:white;border-style:ridge;border-width:1px;padding:2px;font-family:Courier;");
    #else
    ui->butInspect->setStyleSheet("background-color:darkgreen; color:white;");
    ui->butLoadKeys->setStyleSheet("background-color:darkred; color:white;");
    ui->butDisableAllKeys->setStyleSheet("background-color:darkblue; color:white;");
    #endif
    QApplication::restoreOverrideCursor();
}

void MainWindow::resetDisplay() {
    radioInfo.clear();
    ui->txtModel->setStyleSheet("background-color:white;");
    ui->txtModel->setText("");
    ui->txtChassis->setStyleSheet("background-color:white;");
    ui->txtChassis->setText("");
    ui->txtBand->setStyleSheet("background-color:white;");
    ui->txtBand->setText("");
    ui->txtFlash->setStyleSheet("background-color:white;");
    ui->txtFlash->setText("");
    ui->txtFirmware_version->setStyleSheet("background-color:white;");
    ui->txtFirmware_version->setText("");
    ui->txtSFE_capable->setText("");
    ui->txtSFE_capable->setStyleSheet("background-color:white;");
    ui->cmbPorts->setStyleSheet("background-color:white;color:black;");
    QStandardItem *myItem;
    for (int a=0;a<myModel->rowCount();a++) {
        myItem = myModel->item(a,1);
        myItem->setText("");
        myItem->setBackground(QBrush(QColor(Qt::white)));
        myItem->setForeground(QBrush(QColor(Qt::black)));
        ui->winMain->hideRow(a);
    }
    ui->cmbPorts->setFocus();
}

void MainWindow::on_actionApplication_triggered(){
    QMessageBox about;
    QPixmap pix(":/9100a.jpg");
    about.setText(QApplication::applicationName() + " " + QApplication::applicationVersion());
    about.setInformativeText("Developed January 2018\nModel Support :\n- TM82xx\n- TM91xx\n");
    about.setStandardButtons(QMessageBox::Ok);
    about.setDefaultButton(QMessageBox::Ok);
    about.setIconPixmap(pix.scaled(400,400,Qt::KeepAspectRatio));
    about.show();
    about.exec();
}

#ifdef QT_DEBUG
void MainWindow::testfunction() {
    // SEED1 = "06B32F2BFFFF55B0";                              // Byte swapped FLASH ROM serial. In this case: B3062B2FFFFFB055
    // SEED2 = "83E1C36D11F27032";                              // Absolutely no idea where this comes from. That is the problem!
    // (SEED2) 9503091561948409906 - (SEED1) 482781451083797936 = 9020310110864611970
    // A26D45E2A44026E3 - 05C2596C74814834 = 11289095408308903599 ... no obvious number correlation. Almost certainly an encrytion using TEA, key unknown.
}
#endif

void MainWindow::on_butLoadKeys_clicked() {
    doKeys("ENABLEALL");
}

void MainWindow::on_butDisableAllKeys_clicked() {
    doKeys("DISABLEALL");
}

void MainWindow::on_actionFull_SFE_List_triggered() {
#ifndef QT_DEBUG
    return;             // This is a dangerous test option. It can brick radios. Danger Will Robinson, Danger! So much so that the option will not appear in Release Mode at all.
#else
    if (ui->actionFull_SFE_List->isChecked()) { // Ok... don't say you weren't warned!
        if (QMessageBox::warning(this,"Danger","Enabling this option is extremely dangerous and may brick your radio. Are you sure?",QMessageBox::Yes,QMessageBox::No) == QMessageBox::No) {
            ui->actionFull_SFE_List->setChecked(false);
            return;
        }
    }
#endif
}

void MainWindow::toggleOneSFE() {
    doKeys("TOGGLE");
}

void MainWindow::on_winMain_customContextMenuRequested(const QPoint &pos) {
    if (!radioInfo.contains("SFE_TEA_KEY")) return;
    if (radioInfo.value("SFE_TEA_KEY").trimmed().length()==0) return;
    if (!canExit || mySerial.isOpen()) { QMessageBox::warning(this,"Wait","Please wait for the current serial port operations to finish",QMessageBox::Ok); return; }
    varRowNum = ui->winMain->rowAt(pos.y());
    if (varRowNum < 0) { ui->winMain->clearSelection(); ui->cmbPorts->setFocus(); return; }
    QMenu myMenu(this);
    myMenu.addSection(myModel->item(varRowNum,1)->text());
    myMenu.addSeparator();
    myMenu.addAction("Toggle SFE Feature Option",this,SLOT(toggleOneSFE()));
    myMenu.popup(ui->winMain->mapToGlobal(pos));
    myMenu.show();
    myMenu.exec();
}

void MainWindow::on_actionAdvanced_Trunking_Keys_triggered() { // Lets open the TASK dialog.
    taskDialog myDialog(this);
    myDialog.setModal(true);
    myDialog.show();
    myDialog.exec();
}

void MainWindow::on_actionKeyloader_triggered() { // Lets open the Keyloader dialog.
    KeyloaderDialog myDialog(this);
    myDialog.setModal(true);
    myDialog.show();
    myDialog.exec();
}

void MainWindow::doKeys(QString myOption) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    ui->cmbPorts->setEnabled(false);
    ui->butInspect->setEnabled(false);
    ui->butLoadKeys->setEnabled(false);
    ui->butDisableAllKeys->setEnabled(false);
    QApplication::processEvents();      // Refresh the display
    ui->cmbPorts->setFocus();
    canExit=false;
    int a = 0;
    int keysAttempted = 0;
    int keysSuccessful = 0;
    int keysFailed = 0;
    QString myReturn = "";
    QString tmpStr = "";
    QStringList newKey;
    bool continueProcessing = true;
    QStandardItem *myItem;
    myItem = myModel->item(0,1);
    QString myFeatureNum = "00";
    QStringList keyStatus;
    QString chassis;
    bool EnableFeature = true;
    bool ok;
    QList<QString> failedFeatures;
    if (radioInfo.value("SFE_TEA_KEY").length()!=32) { QMessageBox::warning(this,"No SFE TEA Key","No SFE TEA Key was found. Ensure you have run this application after loading mnix firmware. See the doucmentation for help.",QMessageBox::Ok); continueProcessing=false; }
    while (continueProcessing) {
        QFont myFont = myModel->item(0,0)->font();
        myFont.setFamily("Courier");
        myFont.setBold(true);
        myStatus->setText("Attempting to connect to the Tait Radio on " + ui->cmbPorts->currentText());
        myReturn = taitWrite("^","v");
        if (myReturn == "ERROR") break;
        myStatus->setText("Switching Tait Radio on " + ui->cmbPorts->currentText() + " to programming mode");
        myReturn = taitWrite("#",">");                                  // Move to PROGRAMMING MODE
        if (myReturn == "ERROR") break;
        sleep(5000);                                                    // Or else radio falls out of sequence
        QStringList myCmds = QString("ld p01 r00").split(" ");          // Check to see if someone plugged in a new radio body after doing an inspection.... stops bad things.
        QStringList varList;
        foreach(QString varCmd,myCmds) {
            myReturn = taitWrite(varCmd,">");
            if (myReturn=="ERROR") { continueProcessing=false; break; }
            varList = myReturn.split("\r");
            foreach(tmpStr,varList) {
                tmpStr.chop(2);
                if (tmpStr.startsWith("00050") && (varCmd=="r00")) {
                    chassis = taitDecode(tmpStr.mid(6));
                    if (chassis != ui->txtChassis->text()) {
                        continueProcessing = false;
                        QMessageBox::warning(this,"ERROR","I think you changed the radio body. Please run the inspect tool before attempting to change keys on this radio",QMessageBox::Ok);
                    }
                }
            }
        }
        if (!continueProcessing) break;
        if (myOption=="TOGGLE") a = varRowNum;
        keyStatus = taitSFETest(taitHexFromKey(myItem->text()));                                // This gives is ?ENABLED|CurrentSeq|NextSequence
        myStatus->setText("Now processing SFE key(s) for Tait Radio connected to " + ui->cmbPorts->currentText());
        QString existingHex = "";
        for (a=0;a<taitFeatures.size();a++) {
            EnableFeature = true;                                                               // Default setting is to enable keys. Seems most likely
            if (myOption=="TOGGLE") a = varRowNum;                                              // We are only going to edit one key, then exit.
            if (ui->winMain->isRowHidden(a)) break;                                             // The row is hidden - so the previous line was the last valid key.
            myItem = myModel->item(a,2);                                                        // Get the current key being looked at from the display
            existingHex = taitHexFromKey(myItem->text());                                       // Used for version checking.
            keyStatus = taitSFETest(existingHex);                                               // This gives is ?ENABLED|CurrentSeq|NextSequence
            myFeatureNum = keyStatus.at(3);                                                     // The feature number for this key.
            // Do we actually need to do anything at all.
            if ((myOption=="ENABLEALL") && (keyStatus.at(0)=="ENABLED")) continue;              // Key already enabled (which we want).. no action required. Next
            if ((myOption=="DISABLEALL") && (keyStatus.at(1)=="DISABLED")) continue;            // Key already disabled (which we want).. no action required. Next.
            if (myOption=="DISABLEALL") EnableFeature = false;                                  // Who knows... maybe.... possibly if you intend to sell the radio
            if ((myOption=="TOGGLE") && (keyStatus.at(0)=="ENABLED")) EnableFeature = false;    // Probable if you are trying to disbale wideband restrictions.
            if (EnableFeature) {
                newKey = taitGenerateSFE(myFeatureNum,QString(keyStatus.at(2)).toInt(&ok,10));
                tmpStr = "f" + newKey.at(1);
            } else {
                tmpStr = "f" + taitChecksum("01" + myFeatureNum);                               // Well this is fun. Example f0100(+ checksum) will disable feature 00
            }                                                                                   // If you do, the radio returns the new Tait Key as output.
            myReturn = taitWrite(tmpStr,">");                                                   // So we have a new SFE. Lets try to send it to the radio.
            if (myReturn=="ERROR") { continueProcessing=false; break; }
            myFont = myModel->item(a,2)->font();
            if (!EnableFeature) {
                if (myReturn.length()==32) {                                                    // Yay - we managed to disable the key successfully.
                    keysSuccessful++;                                                           // The radio returns the HEX of the new disabled key that it has constructed.
                    tmpStr = taitKeyFromHex(myReturn);
                    myFont.setBold(false);
                    myModel->item(a,0)->setData(QVariant(QPixmap::fromImage(smCross)),Qt::DecorationRole);
                    myModel->item(a,2)->setText(tmpStr);
                    myModel->item(a,1)->setFont(myFont);
                    myModel->item(a,2)->setFont(myFont);
                } else {
                    keysFailed++;
                }
            } else if (myReturn=="0000") {                                                      // Success loading a SFE Enabling key!
                keysSuccessful++;
                myFont.setBold(true);
                myModel->item(a,2)->setText(newKey.at(0));
                myModel->item(a,0)->setData(QVariant(QPixmap::fromImage(smTick)),Qt::DecorationRole);
                myModel->item(a,1)->setFont(myFont);
                myModel->item(a,2)->setFont(myFont);
            } else {
                keysFailed++;
                failedFeatures.append(myFeatureNum);
            }
            if (myOption=="TOGGLE") break;  // This is a single key edit, break out of the programming cycle.
        }
        if (myOption=="TOGGLE") myStatus->setText("SFE Key toggle complete. Resetting radio.");
        if (myOption=="ENABLEALL") myStatus->setText("SFE Key enabling complete. Resetting radio.");
        if (myOption=="DISABLEALL") myStatus->setText("SFE Key disabling complete. Resetting radio.");
        break;  // End of function. Exit.
    }
    // End of function - clean up.
    if (mySerial.isOpen()) {
        myReturn = taitWrite("^","v"); // Reset the radio.
        sleep(5000);
        mySerial.close();
        myStatus->setText("Tait radio on " + ui->cmbPorts->currentText() + " disconnected.");
    }
    ui->butLoadKeys->setEnabled(true);
    ui->butInspect->setEnabled(true);
    ui->butDisableAllKeys->setEnabled(true);
    ui->cmbPorts->setEnabled(true);
    ui->cmbPorts->setFocus();
    canExit=true;
    resetButtons();
    if (keysFailed > 0) {
        if ((keysFailed < 4) && (failedFeatures.contains("1B") || failedFeatures.contains("1C") || failedFeatures.contains("41"))) {
            QMessageBox::information(this,"Complete","SFE Key change sequence complete. Some encryption SFE keys failed but this is probably due to the radio body being a TM9135 or C variant TMAB32 which do not support 3DES or AES encryption",QMessageBox::Ok);
        } else {
            QMessageBox::information(this,"Complete","SFE Key change sequence complete.\nKeys Attempted: " + QString::number(keysAttempted) + "\nKeys Successful: " + QString::number(keysSuccessful) + "\nKeys Failed:" + QString::number(keysFailed),QMessageBox::Ok);
        }
    }
}

// ===================================================================================================================================
// Cryptography Functions
// ===================================================================================================================================
QString MainWindow::TEAEncrypt(QString vData, QString vKey) {
    if (vData.length() < 16) return "ERROR";                        // This code reworked from wikipedia provided information
    if (vKey.length() < 32) return "ERROR";
    bool ok;
    uint32_t v[2] = { 0,0 };
    uint32_t k[4] = { 0,0,0,0 };
    v[0] = vData.mid(0,8).toULong(&ok,16);                          // Since we are converting QStrings of hex we do not need to do the
    v[1] = vData.mid(8,8).toULong(&ok,16);                          // bit shifting you see on the wiki page to get the two datablocks
    k[0] = vKey.mid(0,8).toULong(&ok,16);
    k[1] = vKey.mid(8,8).toULong(&ok,16);
    k[2] = vKey.mid(16,8).toULong(&ok,16);
    k[3] = vKey.mid(24,8).toULong(&ok,16);
    uint32_t v0=v[0], v1=v[1], sum=0;                               // Set up
    uint32_t delta=0x9e3779b9;                                      // Key schedule constant
    uint32_t k0 = k[0];
    uint32_t k1 = k[1];
    uint32_t k2 = k[2];
    uint32_t k3 = k[3];
    for (int i=0; i < 32; i++) {                                    // Cycle start
        sum += delta;
        v0 += ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        v1 += ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
    }                                                               // End cycle
    v[0]=v0; v[1]=v1;                                               // Absolutely vital to pad the front with 0's or else things break badly.
    return QString(QString::number(v0,16).rightJustified(8,'0') + QString::number(v1,16).rightJustified(8,'0')).toUpper();
}


QString MainWindow::TEADecrypt(QString vData, QString vKey) {
    if (vData.length() < 16) return "ERROR";                        // The comments here are the same as for TEAEncrypt. This function is
    if (vKey.length() < 32) return "ERROR";                         // also from wikipedia and simply reverses the encryption routine.
    bool ok;
    uint32_t v[2];
    uint32_t k[4];
    v[0] = vData.mid(0,8).toULong(&ok,16);
    v[1] = vData.mid(8,8).toULong(&ok,16);
    k[0] = vKey.mid(0,8).toULong(&ok,16);
    k[1] = vKey.mid(8,8).toULong(&ok,16);
    k[2] = vKey.mid(16,8).toULong(&ok,16);
    k[3] = vKey.mid(24,8).toULong(&ok,16);
    uint32_t v0=v[0], v1=v[1], sum=0xC6EF3720;
    uint32_t delta=0x9e3779b9;
    uint32_t k0 = k[0];
    uint32_t k1 = k[1];
    uint32_t k2 = k[2];
    uint32_t k3 = k[3];
    for (int i=0; i < 32; i++) {
        v1 -= ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
        v0 -= ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        sum -= delta;
    }
    v[0]=v0; v[1]=v1;                                               // Don't forget the padding!
    return QString(QString::number(v0,16).rightJustified(8,'0') + QString::number(v1,16).rightJustified(8,'0')).toUpper();
}
// ===================================================================================================================================
// Tait Functions
// ===================================================================================================================================
QStringList MainWindow::taitGenerateSFE(QString newFeature, int newSequence) { // Sequence defaults to 1 if not provided.
    // =======================================================================================================--===================================
    // How SFE Key generation for the TM8200 works (for the curious). I am told the TM9000 generation sequence is different.
    // The binary decompilation of the firmware certainly suggests that.
    // --------------------------------------------------------------------------------------------------------------------------------------------
    // SEED1 = "06B32F2BFFFF55B0";                              // Byte swapped FLASH ROM serial. In this case: B3062B2FFFFFB055
    // SEED2 = "83E1C36D11F27032";                              // Absolutely no idea where this comes from. That is the problem!
    // QString varData = taithswap(SEED2);                      // Generate the first Data string from SEED2--
    // QString varKey = taithswap(SEED1) + TAIT_MAGIC1;         // Add a TAIT MAGIC value to the end of SEED1 swapped around to make a 128 bit key
    // QString decryptedTEAkey = TEADecrypt(varData,varKey);    // An intermediate key in the chain. This makes reverse engineering impractical.
    // QString defaultTEAkey = taithswap(decryptedTEAkey);      // Minor byte shuffling.
    //
    // The SFE_TEA_KEY is the final key that is used to encode the hex string just prior to producing the formatted TAIT SFE KEY
    //
    // QString SFE_TEA_KEY = decryptedTEAkey + "59" + defaultTEAkey.mid(10,4) + "A22C" + defaultTEAkey.mid(2,2) + defaultTEAkey.mid(8,2) + "F7";
    //
    // From here the code takes over to generate the data which will be TEAEncrypted using the SFE_TEA_KEY
    // The custom firmware grabs the SFE_TEA_KEY from the radio, making this code un-necessary and it is here so it is not forgotten about.
    // If anyone can work out where SEED2 comes from, that is the holy grail of SFE keygen as hacked firmware would no longer be needed (maybe)
    // ============================================================================================================================================
    QString radioESN = radioInfo.value("ESN");                      // This code has now been changed to support TM9x00
    int a=0;
    bool ok;
    QString SFE_TEA_Key = radioInfo.value("SFE_TEA_KEY");           // The magic number out of mnix firmware report 203
    QString tmpStr = "";                                            // OK... blank slate time. Lets clear tmpStr and
    uint16_t tmp[16];                                               // We use a lot of QStrings in this code but here it really is easier to deal with bytes as bytes
    tmp[0] = (radioESN.toUInt(&ok,10) >>  8) & 0xFF;                // Start to fill in the array - it should be mostly obvious.
    tmp[1] = radioESN.toUInt(&ok,10) & 0xFF;                        // We will convert the byte strings into real bytes for processing.
    tmp[2] = newSequence & 0xFF;                                    // Rotate the number back to 0 once we get to 255. This is a guess, but seems reasonable.
    tmp[3] = newFeature.toUInt(&ok,16);                             // Add the feature code
    tmp[5] = ((tmp[1]>>4) & 0x0F) | ((tmp[0]<<4) & 0xF0);           // The default value.
    if (radioInfo.value("BODY").startsWith("TMAB2")) {              // This is a TM8200 radio. Lets see if we need a workaround for version 2 and above boot code.
        if (radioInfo.value("BOOTVER") != "1") { tmp[5] = ((tmp[1]>>4) & 0x0F) | ((tmp[1]<<4) & 0xF0); }
    }
    tmp[6] = (tmp[3] << 4) & 0xF0;                                  // This is a change from the perl script. It appears that tmp[6] = Always &= 0x0F0 for all models.
    tmp[7] = (radioESN.toUInt(&ok,10) >> 16) & 0xFF;                // Back to the unified code.
    tmp[4] = (((tmp[7]>>4) & 0x0F) | ((tmp[2]<<4) & 0xF0));
    tmp[8] = (radioESN.toUInt(&ok,10) >> 24) & 0xFF;                // tmp[8-13] will be non-encrypted in this phase. The ESN + Feature Code + Sequence Number
    tmp[9] = (radioESN.toUInt(&ok,10) >> 16) & 0xFF;
    tmp[10] = (radioESN.toUInt(&ok,10) >> 8) & 0xFF;
    tmp[11] = radioESN.toUInt(&ok,10) & 0xFF;
    tmp[12] = tmp[3];                                               // Add the feature code.
    tmp[13] = tmp[2];                                               // Add the sequence number.
    tmpStr.clear();
    for (a=0;a<14;a++) { tmpStr += QString::number((tmp[a] & 0xFF),16).rightJustified(2,'0').toUpper(); }
    QString ts = tmpStr.toUpper();
    tmpStr = TEAEncrypt(tmpStr,SFE_TEA_Key);                        // This step prevents reverse decoding the string. Damn it.
    tmpStr += ts.right(12);                                         // Re add the non encrypted data to end.
    a = 0;
    while (tmpStr.length()) {                                       // Transfer the string into uints in an array so we can fiddle with them.
        tmp[a] = tmpStr.left(2).toInt(&ok,16);
        tmpStr = tmpStr.mid(2);
        a++;
    }
    uint16_t sfe[16];                                               // Create the final array we will use to spit out the SFE Hex
    for (a=8;a<14;a++) { sfe[a] = tmp[a]; }                         // Copy in the feature code and ESN
    int o1, o3, rsn;                                                // This is taken directly from mnix's perl code and I haven't really even thought about it.
    o1 = 0xE000 & ((((tmp[0x0A] << 8) | tmp[0x0B]) << 13) | (tmp[0x0D]<<4));
    rsn = (tmp[0x08] << 24) | (tmp[0x09] << 16) | (tmp[0x0A]<<8) | tmp[0x0B];
    o3 = ((rsn >> 19) & 0xFF) | (tmp[0x0C] << 8);
    o1 |= 0x1000 | (tmp[0x0D] << 4);
    sfe[0x08] = (o3 >> 8) & 0xFF;
    sfe[0x09] = (o3 & 0xFF);
    sfe[0x0A] = (rsn >> 11) & 0xFF;
    sfe[0x0B] = (rsn >> 3) & 0xFF;
    sfe[0x0C] = (o1 >> 8) & 0xFF;
    sfe[0x0D] = o1 & 0xFF;
    for (a=0;a<4;a++) { sfe[a] = tmp[a+4]; sfe[a+4] = tmp[a]; }     // Some reversing.
    tmpStr = "00";
    for(a=0;a<14;a++) { tmpStr += QString::number(sfe[a] & 0xFF,16).rightJustified(2,'0').toUpper(); } // Never forget the .toUpper()... ever.
    tmpStr = tmpStr.rightJustified(28,'0');                         // Yay we have out HEX string finally.
    tmpStr = taitChecksum(tmpStr).toUpper();                        // Add a checksum
    QString varData = taitKeyFromHex(tmpStr);                       // Create the formatted Key
    return QString(varData + "|" + tmpStr).split("|");              // Return the string array [0] = Formatted Key, [1] = HEX we just created.
}

QString MainWindow::taitEncodeKey(QString myStr) {
    QString myReturn = "";
    QString tmpStr = "";
    int z = 0;
    for (int a=0;a<myStr.length();a++) tmpStr += hexTable.value(myStr.at(a));
    while (tmpStr.length()) {
        myReturn += taitAlphabet.value(tmpStr.left(5));
        tmpStr = tmpStr.mid(5);
        if (z==3) myReturn += ".";
        if (z==7) myReturn += ".";
        if (z==11) myReturn += ".";
        if (z==15) myReturn += ".";
        if (z==19) myReturn += ".";
        z++;
    }
    return myReturn;
}

QString MainWindow::taitKeyFromHex(QString myStr) {
    if (myStr.startsWith("f")) myStr = myStr.mid(1);
    myStr = myStr.mid(2);
    myStr.chop(3);
    QString tmpStr = "";
    QString myReturn = "";
    for (int a=0;a<myStr.length();a++) {
        tmpStr += hexTable.value(myStr.at(a));
    }
    tmpStr += "0000";
    int z = 0;
    while (z < 30) {
        myReturn += taitAlphabet.value(tmpStr.left(5));
        tmpStr = tmpStr.mid(5);
        if (z==3) myReturn += ".";
        if (z==7) myReturn += ".";
        if (z==11) myReturn += ".";
        if (z==15) myReturn += ".";
        if (z==19) myReturn += ".";
        z++;
        if (z==22) break;
    }
    return myReturn.trimmed();
}

QString MainWindow::taitWrite(QString myStr,QString myReply) {
    // =====================================================================================================================
    // Generically write a HEX ASCII string to the serial port. Send back any reply to the calling function.
    // The default expected reply is a '>' text character. In Test mode that will be a '-' character.
    // =====================================================================================================================
    QByteArray varSend;
    QDateTime myTime = QDateTime::currentDateTime();
    if (!mySerial.isOpen()) {
        mySerial.setPortName(ui->cmbPorts->currentText());
        mySerial.setBaudRate(QSerialPort::Baud19200);
        mySerial.setDataBits(QSerialPort::Data8);
        mySerial.setStopBits(QSerialPort::OneStop);
        mySerial.setFlowControl(QSerialPort::SoftwareControl);  // Fix for FTDI usb converters.
        mySerial.setParity(QSerialPort::NoParity);
        if (!mySerial.open(QIODevice::ReadWrite)) {
            emit TAIT_RADIO_NOREPLY();
            return "ERROR";
        }
    }
    QString tmpString = "";
    int a = 0;
    while (a < 5) {
        if (myStr!="^") myStr.append("\r");                         // A special character. This is grabbed by the radio without a \r
        varSend.clear();
        varSend.append(myStr);                                      // Prepare the byte array that will be sent
#ifdef QT_DEBUG
        if (ui->menuDebug->isChecked()) qDebug() << mySerial.portName() << "TX:" << myStr;
#endif
        mySerial.flush();                                           // Clean out any junk in the buffer.
        mySerial.write(varSend);                                    // Send off the data to the radio
        myTime = QDateTime::currentDateTime();                      // Reset the timer.
        sleep(50);
        while (!tmpString.endsWith(myReply)) {
            if ( mySerial.bytesAvailable() ) {
                tmpString += QString(mySerial.readAll()); myTime = QDateTime::currentDateTime();
            } else {
                sleep(50);
            }
            if (myTime.msecsTo(QDateTime::currentDateTime()) > 10000) { a = 100; break; } // Emergency trap.
        }
#ifdef QT_DEBUG
        if (ui->menuDebug->isChecked()) qDebug() << mySerial.portName() << "RX:" << tmpString;
#endif
        if (tmpString.trimmed()!="") a = 10;                        // Break out - the radio said something.
        if (myStr!="^") a = 10;                                     // Break out - the radio said something.
        a++;                                                        // No reply - increment the counter and try again (10 second delay)
    }
    if (tmpString.trimmed()=="") { emit TAIT_RADIO_NOREPLY(); return "ERROR"; }
    TAIT_RADIO_CONNECTED = true;
    if (tmpString.endsWith(myReply)) { tmpString.chop(1); tmpString = tmpString.trimmed(); }
    return tmpString.trimmed();
}

QString MainWindow::taitChecksum(QString myStr) {
    // =====================================================================================================================
    // Code adapted from 8 bit 2's complement converter at http://easyonlineconverter.com/converters/checksum_converter.html
    // =====================================================================================================================
    myStr = myStr.toUpper();
    QString strHex = "0123456789ABCDEF";        // This is quite a bit different to the perl code which does not work when
    uint8_t z;                                     // directly translated across. I suspect this can be cleaned - but is working so not going to rush to change it.
    int mytotal = 0;                            // Must be initialized to 0 or things break.
    int fctr = 16;
    for (int a=0;a<myStr.length();a++) {
        z = strHex.indexOf(myStr.at(a));        // Get a char from QString and store the hex value of the ASCII character.
        mytotal += z * fctr;                    // Modulo 2 - add the new byte to the existing total.
        (fctr == 16) ? fctr = 1 : fctr = 16;
    }
    mytotal &= 0xFF;                            // Only take the least significant 7 bits.
    mytotal = ~mytotal;                         // Invert the bit values
    mytotal += 0x01;                            // Add one to the total. Inversion + 1 = Two Complement.
    mytotal &= 0xFF;                            // Only use the last byte for the checksum.
    return myStr + QString::number(mytotal,16).toUpper().rightJustified(2,'0');
}

QString MainWindow::taitChecksum16(QString myStr) {
    // =======================================================================================================================
    // Code adapted from crcalc.com. Tait uses CRC16/MCRF4XX for keyloader checksums.
    // Typically a command will be a 7 character command type followed by a variable amount of data. Only the data is
    // checksumed. ie: K20015<DATA><FCS>
    // =======================================================================================================================
    myStr = myStr.toUpper();
    QString origStr = myStr;
    if (myStr.startsWith("k",Qt::CaseInsensitive)) myStr = myStr.mid(7);
    uint16_t array[] = {
        0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD,
        0x6536, 0x74BF, 0x8C48, 0x9DC1, 0xAF5A, 0xBED3,
        0xCA6C, 0xDBE5, 0xE97E, 0xF8F7, 0x1081, 0x0108,
        0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
        0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64,
        0xF9FF, 0xE876, 0x2102, 0x308B, 0x0210, 0x1399,
        0x6726, 0x76AF, 0x4434, 0x55BD, 0xAD4A, 0xBCC3,
        0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
        0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E,
        0x54B5, 0x453C, 0xBDCB, 0xAC42, 0x9ED9, 0x8F50,
        0xFBEF, 0xEA66, 0xD8FD, 0xC974, 0x4204, 0x538D,
        0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
        0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1,
        0xAB7A, 0xBAF3, 0x5285, 0x430C, 0x7197, 0x601E,
        0x14A1, 0x0528, 0x37B3, 0x263A, 0xDECD, 0xCF44,
        0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
        0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB,
        0x0630, 0x17B9, 0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5,
        0xA96A, 0xB8E3, 0x8A78, 0x9BF1, 0x7387, 0x620E,
        0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
        0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862,
        0x9AF9, 0x8B70, 0x8408, 0x9581, 0xA71A, 0xB693,
        0xC22C, 0xD3A5, 0xE13E, 0xF0B7, 0x0840, 0x19C9,
        0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
        0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324,
        0xF1BF, 0xE036, 0x18C1, 0x0948, 0x3BD3, 0x2A5A,
        0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E, 0xA50A, 0xB483,
        0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
        0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF,
        0x4C74, 0x5DFD, 0xB58B, 0xA402, 0x9699, 0x8710,
        0xF3AF, 0xE226, 0xD0BD, 0xC134, 0x39C3, 0x284A,
        0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
        0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1,
        0xA33A, 0xB2B3, 0x4A44, 0x5BCD, 0x6956, 0x78DF,
        0x0C60, 0x1DE9, 0x2F72, 0x3EFB, 0xD68D, 0xC704,
        0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
        0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68,
        0x3FF3, 0x2E7A, 0xE70E, 0xF687, 0xC41C, 0xD595,
        0xA12A, 0xB0A3, 0x8238, 0x93B1, 0x6B46, 0x7ACF,
        0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
        0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022,
        0x92B9, 0x8330, 0x7BC7, 0x6A4E, 0x58D5, 0x495C,
        0x3DE3, 0x2C6A, 0x1EF1, 0x0F78,
    } ;
    uint16_t crc = 0xFFFF;
    bool ok;
    while (myStr.length() > 1) {
        uint16_t d = QString(myStr.left(2)).toUInt(&ok,16);
        myStr = myStr.mid(2);
        crc = array[(d ^ crc) & 0xFF] ^ (crc >> 8 & 0xFF);
    }
    crc = crc & 0xFFFF;
    QString myReturn = QString::number(crc,16).rightJustified(4,'0').toUpper();
    return  myReturn.right(2) + myReturn.left(2);
}

void MainWindow::tait_is_now_connected() {
    TAIT_RADIO_CONNECTED = true;
    myStatus->setText("Tait Radio connected on " + mySerial.portName());
}

void MainWindow::tait_is_now_disconnected() {
    myStatus->setText("Tait Radio disconnected from " + mySerial.portName());
    TAIT_RADIO_CONNECTED = false;
    if (mySerial.isOpen()) mySerial.close();
    canExit=true;
}

void MainWindow::tait_inspected() {
    ui->cmbPorts->setEnabled(true);
    ui->butInspect->setEnabled(true);
    ui->butLoadKeys->setEnabled(true);
    ui->butDisableAllKeys->setEnabled(true);
    canExit=true;
}

void MainWindow::tait_no_reply() {
    myStatus->setText("Tait Radio @ " + mySerial.portName() + " did not reply and the serial port was shutdown");
    if (mySerial.isOpen()) mySerial.close();
    TAIT_RADIO_CONNECTED = false;
    resetDisplay();
    resetButtons();
    ui->cmbPorts->setEnabled(true);
    ui->butInspect->setEnabled(true);
    ui->butLoadKeys->setEnabled(true);
    ui->butDisableAllKeys->setEnabled(true);
    canExit=true;
}

void MainWindow::taitInterrogate() {
    myStatus->setText("Interrogating Tait Radio on " + ui->cmbPorts->currentText() + " in test mode for basic information");
    sleep(5000);
    QString myReturn,tmpStrB;
    QStringList varList;
    QStringList myCmds = QString("94 96 97 98 133 93 134 93 203").split(" ");
    QDir myDir(QApplication::applicationDirPath() + "/radiodb/");
    QFile myFile(QApplication::applicationDirPath() + "/radiodb/0000000.txt");
    foreach(QString varCmd,myCmds) {
        myReturn = taitWrite(varCmd,"-");
        if (myReturn=="ERROR") break;
        if (myReturn.startsWith("{")) continue;                                                                 // Feature not supported - skip processing.
        if ((varCmd=="133") && myReturn.startsWith("TMAB2")) { myCmds.removeAll("93"); }                        // No return on TM8200's so no point.
        if (varCmd=="93") { radioInfo["ESN"] = myReturn.rightJustified(8,'0'); ui->txtFlash->setText(radioInfo.value("ESN")); }
        if (varCmd=="94") {                                                                                     // Radio Serial Number
            ui->txtChassis->setText(myReturn); radioInfo["CHASSIS"] = myReturn;
            if (!myDir.exists()) myDir.mkdir(QApplication::applicationDirPath() + "/radiodb/");
            myFile.setFileName(QApplication::applicationDirPath() + "/radiodb/" + myReturn + ".txt");           // Lets see if we have seen this radio before
            if (myFile.exists()) {                                                                              // Handy if you got the TEA but forgot to load
                if (myFile.open(QIODevice::ReadOnly | QIODevice::Text)) {                                       // the keys last time, the key information being
                    while (!myFile.atEnd()) {                                                                   // the SFE_TEA_KEY which allows us to generate
                        tmpStrB = QString(myFile.readLine()).trimmed();                                         // new SFE codes.
                        if (tmpStrB.startsWith("#")) continue;
                        varList = tmpStrB.split("=");
                        if (varList.size()==2) { radioInfo[varList.at(0)] = varList.at(1); }
                    }
                    myFile.close();
                }
            }
        }
        if (varCmd=="96") {                                                                                         // Body Firmware
            radioInfo["BODYFW"] = myReturn.toUpper().trimmed();
            ui->txtFirmware_version->setText(radioInfo["BODYFW"]);
        }
        if (varCmd=="97") { radioInfo["BOOTFW"] = myReturn.toUpper().trimmed(); }                                   // Boot Firmware
        if (varCmd=="98") { radioInfo["FPGAFW"] = myReturn.toUpper().trimmed(); }                                   // FPGA Firmware
        if (varCmd=="133") {                                                                                        // Model Information
            if (myReturn.at(4)=="2") { ui->txtModel->setText("TM8200 Series"); }
            if (myReturn.at(4)=="3") { ui->txtModel->setText("TM9000 Series"); }
            if (myReturn.at(5)=="1") { ui->txtModel->setText(ui->txtModel->text() + " Conventional Mobile"); }
            if (myReturn.at(5)=="2") { ui->txtModel->setText(ui->txtModel->text() + " Trunking Mobile"); }
            if (myReturn.at(5)=="3") { ui->txtModel->setText(ui->txtModel->text() + " USA Signalling Mobile"); }
            if (myReturn.at(5)=="4") { ui->txtModel->setText(ui->txtModel->text() + " Conventional/Trunking Mobile"); }
            ui->txtBand->setText("UNKNOWN BAND");                                                                   // Frequency Band
            if (taitBands.contains(myReturn.mid(7,2))) ui->txtBand->setText(taitBands.value(myReturn.mid(7,2)));
            radioInfo["BAND"] = ui->txtBand->text();
            radioInfo["MODEL"] = ui->txtModel->text();
            radioInfo["BODY"] = myReturn.left(6);
            if (!taitSupportedModels.contains(radioInfo.value("BODY"))) {
                ui->butLoadKeys->setVisible(false);
                ui->butDisableAllKeys->setVisible(false);
            } else {
                ui->butLoadKeys->setVisible(true);
                ui->butDisableAllKeys->setVisible(true);
            }
        }
        if (varCmd=="134") { radioInfo["ROM"] = myReturn; }
        if (varCmd=="93") { ui->txtFlash->setText(myReturn.rightJustified(8,'0')); radioInfo["ESN"] = ui->txtFlash->text(); }
        if ((varCmd=="203" ) || (varCmd=="204")) {
            varList = myReturn.split("\r");
            foreach(QString tmpStr,varList) {
                if (tmpStr.startsWith("SFE TEA key")) {
                    QString ts = tmpStr.replace(" ","");
                    while (ts.endsWith("-")) ts.chop(1);
                    ts = ts.right(32).trimmed().toUpper();
                    radioInfo["SFE_TEA_KEY"] = ts.right(32);
                    // Add a (Modified) to the firmware label
                    ui->txtFirmware_version->setText(ui->txtFirmware_version->text() + " (Modified)");
                    ui->txtSFE_capable->setText("Yes (Loaded from Radio)");
                    ui->txtSFE_capable->setStyleSheet("background-color: green; color: white;");
                    ui->butLoadKeys->setVisible(true);
                    ui->butDisableAllKeys->setVisible(true);
                }
            }
        }
    }
    if (!TAIT_RADIO_CONNECTED) { QMessageBox::warning(this,"ERROR","The radio stopped responding.",QMessageBox::Ok); resetDisplay(); resetButtons(); return; }
    if (radioInfo.contains("ESN")) ui->txtFlash->setText(radioInfo.value("ESN"));
    // Check if we have a TEA key...
    if ( radioInfo.contains("SFE_TEA_KEY") ) {
        if ( ui->txtSFE_capable->text() == "" ) {
            ui->txtSFE_capable->setText("Yes (Loaded from file)");
            ui->txtSFE_capable->setStyleSheet("background-color: green; color: white;");
        }
    } else {
        ui->txtSFE_capable->setText("No");
        ui->txtSFE_capable->setStyleSheet("background-color: red");
    }
    if (radioInfo.contains("BOOTFW")) {
        varList = radioInfo.value("BOOTFW").split(".");
        tmpStrB = varList.at(0);
        radioInfo["BOOTVER"] = (tmpStrB.endsWith("1")) ? "1" : "2";
    }
}

QString MainWindow::taitHexFromKey(QString myKey) {
    // Lets translate a key into its HEX string equivalent. This changes 5 bit ASCII into 4 bit HEX.
    QString tmpStr = myKey.replace(".","").trimmed();
    QString byteStr = "";
    QHashIterator<QString,QString> i(taitAlphabet);
    for(int a=0;a<tmpStr.length();a++) {
        i.toFront();
        while (i.hasNext()) {
            i.next();
            if (i.value()==tmpStr.at(a)) { byteStr.append(i.key()); break; }
        }
    }
    // Okay we now have a binary string. Lets convert that to hex.
    tmpStr.clear();
    for (int a=0;a<byteStr.length();a+=4) {
        tmpStr.append(bitTable.value(byteStr.mid(a,4)));
    }
    tmpStr.append("0");
    tmpStr = tmpStr.rightJustified(30,'0');
    tmpStr = taitChecksum(tmpStr);
    return tmpStr.trimmed();
}

QStringList MainWindow::taitSFETest(QString myKey) { // Hex key - 32 bytes long.
    if (myKey.length()!=32) return QString("ERROR|0|0|00").split("|");
    QStringList myReturn = QString("DISABLED|0|0|00").split("|");
    bool ok = true;
    QString taitKey = taitKeyFromHex(myKey).right(2);
    uint16_t tmpNum = 0;
    for (int a=0;a<32;a++) {
        if (taitAlphabetB[a]==taitKey.at(0)) tmpNum = a;
    }
    for (int a=0;a<32;a++) {
        if (taitAlphabetB[a]==taitKey.at(1)) tmpNum = tmpNum & a;
    }
    int varSeq = myKey.mid(27,2).toInt(&ok,16);
    ok = ((varSeq % 2) == 0); // true = even 0, 2, 4, 6, 8 etc is a disabled feature key.
    myReturn[0] = (ok) ? "DISABLED" : "ENABLED";    // Current status of the key
    myReturn[1] = QString::number(varSeq);          // Current sequence.
    varSeq++;
    if (varSeq > 255) varSeq = 0;
    myReturn[2] = QString::number(varSeq);          // Next in the sequence.
    myReturn[3] = myKey.mid(18,2).rightJustified(2,'0'); // The feature number.
    return myReturn;
}

QString MainWindow::taitDecode(QString myStr) {
    myStr = myStr.toUpper();
    QString taitString = "";
    uint cs;
    bool ok;
    for(int a=0;a < myStr.length();a+=2) {
        cs = myStr.mid(a,2).toInt(&ok,16);
        taitString += QString(QChar(cs));
    }
    return taitString.trimmed();
}

QString MainWindow::taithswap(QString myStr) {
    if (myStr.length()!=16) return "0000000000000000";
    QString myReturn = myStr.mid(4,4) + myStr.left(4) + myStr.right(4) + myStr.mid(8,4);
    return myReturn;
}
