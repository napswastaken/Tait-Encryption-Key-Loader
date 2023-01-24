#include "taskdialog.h"
#include "ui_taskdialog.h"
#include <QDir>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <QMenu>
#include <QHashIterator>
#include <QMessageBox>
#include <QDebug>

static const QString TAIT_ASK_KEY = "3A42B06866DE069A755BFFFC00000000";             // Predefined key for software trunking keys

taskDialog::taskDialog(QWidget *parent) : QDialog(parent), ui(new Ui::taskDialog) {
    ui->setupUi(this);
    ui->txtSYSID->setFocus();
    ui->labelSYSID->setStyleSheet("font-family:Courier;");
    ui->txtSYSID->setStyleSheet("font-family:Courier;");
    ui->labelWACN->setStyleSheet("font-family:Courier;");
    ui->txtWACN->setStyleSheet("font-family:Courier;");
    ui->butADD->setStyleSheet("background-color:gray;color:white;font-family:Courier;");
    ui->butADD->setCursor(Qt::PointingHandCursor);
    ui->butClose->setStyleSheet("background-color:darkblue;color:white;font-family:Courier;");
    ui->butClose->setCursor(Qt::PointingHandCursor);
    this->setWindowTitle("Advanced System Key Management");
    currentRow = 0;
    ui->winKeys->horizontalHeader()->setStretchLastSection(true);
    ui->winKeys->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->winKeys->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->winKeys->setUpdatesEnabled(false);
    myModel = new QStandardItemModel(0,3,this);
    myModel->setHorizontalHeaderLabels(QString("|System ID (3 digit)|WACN (5 digit)").split("|"));
    ui->winKeys->setStyleSheet("font-family:Courier;");
    ui->winKeys->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->winKeys->setShowGrid(false);
    ui->winKeys->setModel(myModel);
    ui->winKeys->setUpdatesEnabled(true);
    ui->butADD->setEnabled(false);
    pTick = QImage(":/tick.png");
    smTick = pTick.scaled(14,14,Qt::KeepAspectRatio);
    pCross = QImage(":/cross.png");
    smCross = pCross.scaled(14,14,Qt::KeepAspectRatio);
    QDir myDir;
#ifdef WIN32
    myDir.setPath(QDir::homePath() + "/Documents/Tait Applications/System Key Files/");
#else
    myDir.setPath(QApplication::applicationDirPath() + "/trunkkeys/");
#endif
    if (!myDir.exists()) myDir.mkdir(myDir.path());
    drawWindow();
}

taskDialog::~taskDialog() {
    delete ui;
}

void taskDialog::on_winKeys_customContextMenuRequested(const QPoint &pos) {
    currentRow = ui->winKeys->rowAt(pos.y());
    if (currentRow < 0) { ui->winKeys->clearSelection(); ui->txtSYSID->setFocus(); return; }
    QMenu myMenu(this);
    QString mySystem = "Key: " + myModel->item(currentRow,1)->text() + " " + myModel->item(currentRow,2)->text();
    myMenu.addSection(mySystem);
    myMenu.addSeparator();
    myMenu.addAction(QIcon(QPixmap::fromImage(smCross)),"Delete System Key",this,SLOT(deleteTASK()));
    myMenu.popup(ui->winKeys->mapToGlobal(pos));
    myMenu.show();
    myMenu.exec();
}

void taskDialog::deleteTASK() {
    if (QMessageBox::question(this,"Confirm","Are you sure you wish to delete this Advanced System Key?",QMessageBox::Yes,QMessageBox::No)==QMessageBox::No) return;
    QString mySystem = myModel->item(currentRow,2)->text() + "_" + myModel->item(currentRow,1)->text() + ".tkey";
    QFile myFile;
#ifdef WIN32
    myFile.setFileName(QDir::homePath() + "/Documents/Tait Applications/System Key Files/" + mySystem);
#else
    myFile.setFileName(QApplication::applicationDirPath() + "/trunkkeys/" + mySystem);
#endif
    if (myFile.exists()) myFile.remove();
    myModel->removeRow(currentRow);
    drawWindow();
}

void taskDialog::on_butADD_clicked() {
    if (ui->txtSYSID->text().trimmed().length()!=3) { QMessageBox::warning(this,"ERROR","System IDs must be three digits long i.e: 236",QMessageBox::Ok); return; }
    if (ui->txtWACN->text().trimmed().length()!=5) { QMessageBox::warning(this,"ERROR","WACN's must be five digits long i.e: A43A7",QMessageBox::Ok); return; }
    ui->txtSYSID->setText(ui->txtSYSID->text().toUpper().trimmed());
    ui->txtWACN->setText(ui->txtWACN->text().toUpper().trimmed());
    QFile myFile;
    QString strWACN = ui->txtWACN->text();
    QString strSYS = ui->txtSYSID->text();
#ifdef WIN32
    myFile.setFileName(QDir::homePath() + "/Documents/Tait Applications/System Key Files/" + strWACN + "_" + strSYS + ".tkey");
#else
    myFile.setFileName(QApplication::applicationDirPath() + "/trunkkeys/" + strWACN + "_" + strSYS + ".tkey");
#endif
    if (myFile.exists()) { QMessageBox::warning(this,"ERROR","There is already a system with that System ID and WACN. Please delete the existing entry to continue.",QMessageBox::Ok); return; }
    QString varData = "D0000202" + strWACN + strSYS;
    QString result = TEAEncrypt(varData,TAIT_ASK_KEY);
    varData = result.right(8) + result.left(8);
    result = strWACN + "_" + strSYS + "_" + varData;
    if (myFile.exists()) myFile.remove();
    if (myFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream out(&myFile);
        out << result << endl;
        myFile.close();
    }
    drawWindow();
}

void taskDialog::on_txtSYSID_textEdited(const QString &arg1) {
    if (arg1=="") {} // Remove a compiler warning.
    ui->butADD->setStyleSheet("background-color:grey;color:white;font-family:Courier;");
    if ((ui->txtSYSID->text().trimmed().length()!=3) || (ui->txtWACN->text().trimmed().length()!=5)) { ui->butADD->setEnabled(false); return; }
    ui->butADD->setStyleSheet("background-color:darkgreen;color:white;font-family:Courier;");
    ui->butADD->setEnabled(true);
}

void taskDialog::on_txtWACN_textEdited(const QString &arg1) {
    if (arg1=="") {} // Remove a compiler warning.
    ui->butADD->setStyleSheet("background-color:grey;color:white;font-family:Courier;");
    if ((ui->txtSYSID->text().trimmed().length()!=3) || (ui->txtWACN->text().trimmed().length()!=5)) { ui->butADD->setEnabled(false); return; }
    ui->butADD->setStyleSheet("background-color:darkgreen;color:white;font-family:Courier;");
    ui->butADD->setEnabled(true);
}


void taskDialog::drawWindow() {
    ui->winKeys->hide();
    QDir myDir;
#ifdef WIN32
    myDir.setPath(QDir::homePath() + "/Documents/Tait Applications/System Key Files");
#else
    myDir.setPath(QApplication::applicationDirPath() + "/trunkkeys");
#endif
    myDir.setNameFilters(QStringList("*.tkey"));
    myDir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    keyFiles = myDir.entryList();                           // We have a list of trunking key files in the data directory
    QList<QStandardItem*> varItems;
    QStandardItem *myItem;
    while (myModel->rowCount() < keyFiles.size()) {         // Make sure we have enough rows to display the data
        varItems.clear();
        for(int a=0;a<myModel->columnCount();a++) {
            myItem = new QStandardItem;
            myItem->setTextAlignment(Qt::AlignVCenter);
            myItem->setFlags(myItem->flags() ^ Qt::ItemIsEditable);
            varItems.append(myItem);
        }
        myModel->appendRow(varItems);
    }
    for (int a=0;a<keyFiles.size();a++) {
        myItem = myModel->item(a,0);
        if (TEACheckKey(keyFiles.at(a))) {
            myItem->setData(QVariant(QPixmap::fromImage(smTick)),Qt::DecorationRole);
        } else {
            myItem->setData(QVariant(QPixmap::fromImage(smCross)),Qt::DecorationRole);
        }
        myItem = myModel->item(a,1);
        myItem->setText(QString(keyFiles.at(a)).mid(6,3));
        myItem = myModel->item(a,2);
        myItem->setText(QString(keyFiles.at(a)).left(5));
    }
    ui->winKeys->show();
}

void taskDialog::resizeEvent(QResizeEvent *) {
    QSize mySize = ui->winKeys->size();
    ui->winKeys->setColumnWidth(0,20);
    ui->winKeys->setColumnWidth(1,mySize.width() * 0.5);
}

// ===================================================================================================================================
// Cryptography Functions. TEA Encrypt and TEA Decrypt are common throughout Tait Application - only the scret keys change.
// ===================================================================================================================================
QString taskDialog::TEAEncrypt(QString vData, QString vKey) {
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


QString taskDialog::TEADecrypt(QString vData, QString vKey) {
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

bool taskDialog::TEACheckKey(QString fileName) {
    // We receive a filename like 12345_236.tkey
    // It should contact 12345_236_[8 Byte TEA Encrypted String]
    bool myReturn = false;                                                                  // Set the default case
    QFile myFile;
#ifdef WIN32
    myFile.setFileName(QDir::homePath() + "/Documents/Tait Applications/System Key Files/" + fileName);
#else
    myFile.setFileName(QApplication::applicationDirPath() + "/trunkkeys/" + fileName);
#endif
    QString varLine;
    if (myFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!myFile.atEnd()) {
            varLine = QString(myFile.readLine()).trimmed();                                 // There should only ever be one line.
        }
        myFile.close();
        QString varData = varLine.right(16);                                                // Take the data and swap the halves around.
        QString decoded = TEADecrypt(varData.right(8) + varData.left(8),TAIT_ASK_KEY);      // Decode this against the super secret key
        if ((decoded.mid(8,5)==fileName.left(5)) && (decoded.right(3) == fileName.mid(6,3))) myReturn = true;  // If it matches show a green tick!
    }
    return myReturn;
}


