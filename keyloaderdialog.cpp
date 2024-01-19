#include "keyloaderdialog.h"
#include "ui_keyloaderdialog.h"
#include <QtCore>
#include <QByteArray>
#include <QString>
#include <QMessageBox>
#include "mainwindow.h"

MainWindow *winP;               // Ok, this will allow us to access the parent object directly. Thorectically signals are better, but then you don't get
                                // answers back easily. So I tend to think of it as 'if you need a one way notification, use a signal/slot. If you must have
                                // interaction/feedback use qobject_case to link directly back. Not thread safe, so can only do this in 32bit apps.

static const QString control = "00";
static const QString dest_rsi = "FFFFFF";
static const QString source_rsi = "98967E";
static const QString kmm_esync = "00";

KeyloaderDialog::KeyloaderDialog(QWidget *parent) : QDialog(parent), ui(new Ui::KeyloaderDialog) {
    ui->setupUi(this);
    winP = qobject_cast<MainWindow*>(parent);          // Point back to the parent. now use winP->PublicFunction, ie winP->taitWrite(something);

    // Set up the TableView for New keys.
    newKeys = new QStandardItemModel(0,4,this);
    newKeys->setHorizontalHeaderLabels(QString("CKR|KeyID|Cipher|Key").split("|"));
    ui->tblNewKeys->setModel(newKeys);

    // Set up the TableView for inventory keys.
    keyInventory = new QStandardItemModel(0,3, this);
    keyInventory->setHorizontalHeaderLabels(QString("CKR|KeyID|Cipher").split("|"));
    ui->tblInventory->setModel(keyInventory);

    // Set up the Combo box for Key Type.
    ui->cmbKeyType->addItems(QString("DES|AES").split("|"));
}

KeyloaderDialog::~KeyloaderDialog() {
    delete ui;
}

void KeyloaderDialog::on_butClear_clicked() {
    newKeys->clear();
    newKeys = new QStandardItemModel(0,4,this);
    newKeys->setHorizontalHeaderLabels(QString("CKR|KeyID|Cipher|Key").split("|"));
    ui->tblNewKeys->setModel(newKeys);
}

void KeyloaderDialog::on_butGetInventory_clicked() {
    // Set up the TableView for inventory keys.
    keyInventory = new QStandardItemModel(0,3, this);
    keyInventory->setHorizontalHeaderLabels(QString("CKR|KeyID|Cipher").split("|"));
    ui->tblInventory->setModel(keyInventory);

    enterKeyloaderMode();
    if ( ! winP->TAIT_RADIO_CONNECTED ) { enableButtons(); return; }
    QString result = "";

    QString opcode = "KC2";
    QString cmd = control + dest_rsi + "0D000D80" + dest_rsi + source_rsi + "FD000000000A";
    QString crc = winP->taitChecksum16(cmd);

    QString len = QString::number(cmd.length() + crc.length());
    len = QString::number(len.toInt() / 2, 16).toUpper().rightJustified(4, '0');
    result = winP->taitWrite( opcode + len + cmd + crc, "\r");

    // chop off the checksum.
    result.chop(4);

    int pos = result.lastIndexOf("FD00000000") + 10;
    QString numKeys = result.mid(pos, 2);
    pos = pos + 2;

#ifdef QT_DEBUG
    qDebug() << "Start position: " << pos << "Number of keys: " << numKeys;
#endif

    for ( int i=0; i < numKeys.toInt(); i++ ) {
        QString keySet = result.mid(pos, 2);
        QString ckr = result.mid(pos + 2, 4);
        QString algid = result.mid(pos + 6, 2);
        QString keyid = result.mid(pos + 8, 4);
        pos = pos + 12;

#ifdef QT_DEBUG
        qDebug() << "ckr" << ckr << "algid" << algid << "keyid" << keyid;
#endif

        // Nicely format the data.
        keyid.prepend("0x");
        if ( algid == "81" ) { algid = "DES"; }
        if ( algid == "84" ) { algid = "AES"; }
        bool bStatus = false;
        QString ckr_dec = QString::number(ckr.toUInt(&bStatus, 16));

        QList<QStandardItem*> myRow;
        myRow.append( new QStandardItem(ckr_dec) );
        myRow.append( new QStandardItem(keyid) );
        myRow.append( new QStandardItem(algid) );
        keyInventory->appendRow(myRow);
    }

    exitKeyloaderMode();
}

void KeyloaderDialog::on_butSend_clicked() {
    if ( ui->tblNewKeys->verticalHeader()->count() == 0 ) {
        ui->lblStatus->setText("No keys added to load");
        return;
    }

    enterKeyloaderMode();
    if ( ! winP->TAIT_RADIO_CONNECTED ) { enableButtons(); return; }
    QString result = "";

    for ( int i=0; i < ui->tblNewKeys->verticalHeader()->count(); i++ ) {
        QString ckr = newKeys->index(i, 0).data().toString();
        QString keyid = newKeys->index(i, 1).data().toString();
        QString algid = newKeys->index(i, 2).data().toString();
        QString key = newKeys->index(i, 3).data().toString();

        // Convert our numbers to something useful... 0000-FFFF
        ckr = QString::number( ckr.toInt(), 16).toUpper().rightJustified(4, '0');
        keyid.remove(0,2);                      // Remove the 0x at the start,
        keyid = keyid.rightJustified(4, '0');   // and pad to 0000

        // Set the correct algoritm id code and associated data.
        QString keylength = "";
        if ( algid == "DES" ) { algid = "81"; keylength = "08"; }       // Set for DES-OFB
        if ( algid == "AES" ) { algid = "84"; keylength = "20"; }       // Set for AES

        // Build the KMM String
        QString opcode = "KC2";
        QString cmd = control + dest_rsi + "13001D80" + dest_rsi + source_rsi + kmm_esync + "0080000001" + algid + keylength + "01" + kmm_esync + ckr + keyid + key;
        QString crc = winP->taitChecksum16(cmd);

        // Calculate the length (Field 422) in bytes, represented in HEX.
        QString len = QString::number(cmd.length() + crc.length());
        len = QString::number(len.toInt() / 2, 16).toUpper().rightJustified(4, '0');

        // Write the key to the radio.
        result = winP->taitWrite(opcode + len + cmd + crc, "\r");
        if ( ! result.startsWith("kC2001600")) {
            ui->lblStatus->setText("Radio keyload response was not expected.");
            emit winP->TAIT_DISCONNECTED();
            enableButtons();
            return;
        }
    }

    exitKeyloaderMode();
}

void KeyloaderDialog::on_butEraseAll_clicked() {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Confirm Erase");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("This will erase *ALL* stored keys\n\nAre you sure?");
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if ( msgBox.exec() == QMessageBox::No) { return; }

    enterKeyloaderMode();
    if ( ! winP->TAIT_RADIO_CONNECTED ) { enableButtons(); return; }
    QString opcode = "KC2";
    QString cmd = control + dest_rsi + "21000780" + dest_rsi + source_rsi;
    QString crc = winP->taitChecksum16(cmd);
    QString len = QString::number(cmd.length() + crc.length());
    len = QString::number(len.toInt() / 2, 16).toUpper().rightJustified(4, '0');

    QString result = winP->taitWrite( opcode + len + cmd + crc, "\r");
    if ( ! result.startsWith("kC2") ) { ui->lblStatus->setText("Radio did not accept erase command."); enableButtons(); return; }

    exitKeyloaderMode();
}

void KeyloaderDialog::on_cmbKeyType_currentIndexChanged(const QString &arg1) {
    int length = 0;
    if ( arg1 == "DES") {
        length = 16;
    }
    if ( arg1 == "AES") {
        length = 64;
    }
    ui->txtKey->setValidator(new QRegExpValidator (QRegExp("[0-9A-Fa-f]{1,64}"), this));
    ui->txtKey->setText("");
    ui->txtKey->setMaxLength(length);
}

void KeyloaderDialog::on_butAddKey_clicked() {
    if (
            ( ui->txtKey->text().length() != 16 && ui->cmbKeyType->currentText() == "DES" ) ||
            ( ui->txtKey->text().length() != 64 && ui->cmbKeyType->currentText() == "AES" )
    ) {
        QMessageBox::critical(this, "Error", "Key length is not valid", QMessageBox::Ok);
        return;
    }

    // Validate DES keys follow the correct parity.
    if ( ui->cmbKeyType->currentText() == "DES" ) {
        for ( int i=0; i < 16; i+=2 ) {
            bool ok;
            QString byte = ui->txtKey->text().mid(i, 2);
            QString binary = QString("%1").arg(byte.toULong(&ok, 16), 8, 2, QChar('0'));
            int number_of_ones = binary.count(QString("1"));
#ifdef QT_DEBUG
            qDebug() << "i" << i << "byte" << byte << "binary" << binary << "Ones" << number_of_ones;
#endif
            if ( number_of_ones % 2 == 0) {
                QMessageBox::critical(this,
                                      "Invalid key format",
                                      "Each octet (two hexadecimal numbers) must have odd parity (an odd number of 1s in the binary number).\n\n"
                                      "This means a key consisting of entirely one number (for example 1111111111111111 or 2222222222222222) is not"
                                      " valid and must not be used (hexadecimal 11 converted to binary is 00010001 which contains an even number"
                                      " of ’1’ bits).", QMessageBox::Ok);
                return;
            }
        }
    }

    QList<QStandardItem*> myRow;
    myRow.append( new QStandardItem(ui->spnCKR->text()) );
    myRow.append( new QStandardItem(ui->spnKeyID->text()) );
    myRow.append( new QStandardItem(ui->cmbKeyType->currentText()) );
    myRow.append( new QStandardItem(ui->txtKey->text().toUpper()) );
    newKeys->appendRow(myRow);
}

void KeyloaderDialog::disableButtons() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    ui->butAddKey->setEnabled(false);
    ui->butClear->setEnabled(false);
    ui->butEraseAll->setEnabled(false);
    ui->butGetInventory->setEnabled(false);
    ui->butSend->setEnabled(false);
}

void KeyloaderDialog::enableButtons() {
    winP->sleep(5000);
    ui->butAddKey->setEnabled(true);
    ui->butClear->setEnabled(true);
    ui->butEraseAll->setEnabled(true);
    ui->butGetInventory->setEnabled(true);
    ui->butSend->setEnabled(true);
    QApplication::restoreOverrideCursor();
}


void KeyloaderDialog::enterKeyloaderMode() {
    disableButtons();
    ui->lblStatus->setText("Attempting to connect to radio...");
    QString result = winP->taitWrite("^", "v");
    result = winP->taitWrite("$", ":");
    result = winP->taitWrite("S\rKC0", "\r");
    if ( result != "kD0" ) { ui->lblStatus->setText("Radio does not seem to support APCO keyloading."); return; }
    emit winP->TAIT_CONNECTED();
}

void KeyloaderDialog::exitKeyloaderMode() {
    QString result = winP->taitWrite("KC1", "\r");
    result = winP->taitWrite("K92", "k90x");
    ui->lblStatus->setText("Resetting radio...");
    result = winP->taitWrite("^", "v");
    emit winP->TAIT_DISCONNECTED();
    enableButtons();
    ui->lblStatus->setText("Operation Complete!");
}
