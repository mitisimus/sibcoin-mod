// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015 The SibCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "genandprintdialog.h"
#include "ui_genandprintdialog.h"

#include "guiconstants.h"
#include "walletmodel.h"

#include "allocators.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QPrintDialog>
#include <QFileDialog>
#include <QPainter>
#include <QPrinter>

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

GenAndPrintDialog::GenAndPrintDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenAndPrintDialog),
    mode(mode),
    model(0),
    fCapsLock(false)
{
    ui->setupUi(this);

    ui->passEdit1->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit2->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit3->setMaxLength(MAX_PASSPHRASE_SIZE);

    // Setup Caps Lock detection.
    ui->passEdit1->installEventFilter(this);
    ui->passEdit2->installEventFilter(this);
    ui->passEdit3->installEventFilter(this);

    switch(mode)
    {
        case Export: // Ask passphrase x2 and account
            setWindowTitle(tr("Export key pair"));
            ui->passLabel1->setText(tr("Account name"));
            ui->passLabel2->setText(tr("Password"));
            ui->passLabel3->setText(tr("Repeat password"));
            ui->warningLabel->setText(tr("Enter account and passphrase to the encrypt private key"));
            break;
        case Import: // Ask old passphrase + new passphrase x2
            setWindowTitle(tr("Import private key"));
            ui->passLabel1->setText(tr("Private key"));
            ui->passLabel2->setText(tr("Key password"));
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            ui->warningLabel->setText(tr("Enter private key and passphrase"));
            break;
    }

    textChanged();
    connect(ui->passEdit1, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit2, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit3, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
}

GenAndPrintDialog::~GenAndPrintDialog()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->passEdit1->setText(QString(" ").repeated(ui->passEdit1->text().size()));
    ui->passEdit2->setText(QString(" ").repeated(ui->passEdit2->text().size()));
    ui->passEdit3->setText(QString(" ").repeated(ui->passEdit3->text().size()));
    delete ui;
}

void GenAndPrintDialog::setModel(WalletModel *model)
{
    this->model = model;
    //ui->anonymizationCheckBox->setChecked(model->isAnonymizeOnlyUnlocked());
}

void GenAndPrintDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;
    if(!model)
        return;
    oldpass.reserve(MAX_PASSPHRASE_SIZE);
    newpass1.reserve(MAX_PASSPHRASE_SIZE);
    newpass2.reserve(MAX_PASSPHRASE_SIZE);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->passEdit1->text().toStdString().c_str());
    newpass1.assign(ui->passEdit2->text().toStdString().c_str());
    newpass2.assign(ui->passEdit3->text().toStdString().c_str());
}

void GenAndPrintDialog::textChanged()
{
    // Validate input, set Ok button to enabled when acceptable
    bool acceptable = false;
    switch(mode)
    {
    case Export:
        acceptable = !ui->passEdit2->text().isEmpty() 
                && !ui->passEdit3->text().isEmpty() 
                && ui->passEdit3->text() == ui->passEdit2->text();
        break;
    case Import:
        acceptable = true;
        break;
    }
    ui->printButton->setEnabled(acceptable);
}

void GenAndPrintDialog::on_printButton_clicked()
{
    //QMessageBox::information(this, tr("Generating..."), tr("Generating..."));
    //QString fileName = QFileDialog::getOpenFileName(this,"Open File",QString(),"PNG File(*.png)");
    
    CKey secret = model->generateNewKey();
    CPrivKey privkey = secret.GetPrivKey();
    CPubKey pubkey = secret.GetPubKey();
    CKeyID keyid = pubkey.GetID();
    
    std::string secret_str = CBitcoinSecret(secret).ToString();
    std::string pubkey_str = CBitcoinAddress(keyid).ToString();
    
    QString qsecret = QString::fromStdString(secret_str);
    QString qaddress = QString::fromStdString(pubkey_str);
     
    QMessageBox::information(this, tr("New key"), "Debug:\n" + qsecret + "\n" + qaddress); 
                    
    QPrinter printer;
    QPrintDialog *dlg = new QPrintDialog(&printer, this);
    if(dlg->exec() == QDialog::Accepted) {
        //QImage img(fileName);
        QString strAccount = ui->passEdit1->text();
        QPainter painter(&printer);
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);
        printAsQR(painter, qaddress, 0);
        painter.drawText(0, 250, qaddress);
        printAsQR(painter, qsecret, 250);
        painter.drawText(250, 250, qsecret);
        painter.drawText(0, 300, QString("Account: ") + strAccount);
        painter.end();
        
        // import pub key into wallet
        model->setAddressBook(keyid, strAccount.toStdString(), "send");
    }
    delete dlg;
    
    this->close();
}

void GenAndPrintDialog::printAsQR(QPainter &painter, QString &vchKey, int shift)
{
    QRcode *qr = QRcode_encodeString(vchKey.toStdString().c_str(), 1, QR_ECLEVEL_L, QR_MODE_8, 0);
    if(0!=qr) {
        QPaintDevice *pd = painter.device(); 
        QColor fg("black");
        QColor bg("white");
        painter.setBrush(bg);
        painter.setPen(Qt::SolidLine);
        //painter.drawRect(0, 0, width(), height());
        painter.setBrush(fg);
        const int s=qr->width > 0 ? qr->width : 1;
        const double w = pd->width();
        const double h = pd->height();
        const double aspect = w / h;
        const double scale = ((aspect > 1.0) ? h : w) / s * 0.3;
        for(int y = 0; y < s; y++){
            const int yy = y*s;
            for(int x = 0; x < s; x++){
                const int xx = yy + x;
                const unsigned char b = qr->data[xx];
                if(b &0x01){
                    const double rx1 = x*scale, ry1 = y*scale;
                    QRectF r(rx1 + shift, ry1, scale, scale);
                    painter.drawRects(&r, 1);
                }
            }
        }
        QRcode_free(qr);
    }
    //painter.drawImage(QPoint(0,0),img);
}

bool GenAndPrintDialog::event(QEvent *event)
{
    // Detect Caps Lock key press.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_CapsLock) {
            fCapsLock = !fCapsLock;
        }
        if (fCapsLock) {
            ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
        } else {
            ui->capsLabel->clear();
        }
    }
    return QWidget::event(event);
}

bool GenAndPrintDialog::eventFilter(QObject *object, QEvent *event)
{
    /* Detect Caps Lock.
     * There is no good OS-independent way to check a key state in Qt, but we
     * can detect Caps Lock by checking for the following condition:
     * Shift key is down and the result is a lower case character, or
     * Shift key is not down and the result is an upper case character.
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        QString str = ke->text();
        if (str.length() != 0) {
            const QChar *psz = str.unicode();
            bool fShift = (ke->modifiers() & Qt::ShiftModifier) != 0;
            if ((fShift && *psz >= 'a' && *psz <= 'z') || (!fShift && *psz >= 'A' && *psz <= 'Z')) {
                fCapsLock = true;
                ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
            } else if (psz->isLetter()) {
                fCapsLock = false;
                ui->capsLabel->clear();
            }
        }
    }
    return QDialog::eventFilter(object, event);
}
