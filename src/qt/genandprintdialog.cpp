// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015 The SibCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "genandprintdialog.h"
#include "ui_genandprintdialog.h"

#include "guiconstants.h"
#include "guiutil.h"
#include "walletmodel.h"

#include "allocators.h"
#include "../rpcserver.h"
//#include "../rpcprotocol.h"
#include "json/json_spirit_writer.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QPainter>
#include <QTextStream>
#include <QTextDocument>
#include <QUrl>

#if QT_VERSION >= 0x050000
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#else
#include <QPrintDialog>
#include <QPrinter>
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

GenAndPrintDialog::GenAndPrintDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenAndPrintDialog),
    mode(mode),
    model(0),
    fCapsLock(false),
    salt("12345678")
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
            ui->importButton->hide();
            ui->passLabel1->setText(tr("Account name"));
            ui->passLabel2->setText(tr("Password"));
            ui->passEdit2->setEchoMode(QLineEdit::Password);
            ui->passLabel3->setText(tr("Repeat password"));
            ui->passEdit3->setEchoMode(QLineEdit::Password);
            ui->warningLabel->setText(tr("Enter account and passphrase to the encrypt private key"));
            break;
        case Import: // Ask old passphrase + new passphrase x2
            setWindowTitle(tr("Import private key"));
            ui->printButton->hide();
            ui->passLabel1->setText(tr("Private key"));
            ui->passLabel2->setText(tr("Key password"));
            ui->passLabel3->setText(tr("Account name"));
            ui->passEdit3->setEchoMode(QLineEdit::Normal);
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

QString GenAndPrintDialog::getURI(){
    return uri;
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

    switch(mode)
    {
    case Export: 
        if (uri != "") {
            QDialog::accept();
            return;
        }
        break;
    case Import:
        QDialog::reject();
        break;
    }
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

void GenAndPrintDialog::on_importButton_clicked()
{
    json_spirit::Array params;

    QString privkey_str = ui->passEdit1->text();
    QString passwd = ui->passEdit2->text();
    QString label_str = ui->passEdit3->text();
    
    std::vector<unsigned char> priv_data;
    DecodeBase58(privkey_str.toStdString(), priv_data);

    CKey key;
    model->decryptKey(priv_data, passwd.toStdString(), salt, key);
    
    if (!key.IsValid())
    {
        QMessageBox::critical(this, tr("Error"), tr("Invalid private key! (Is password correct?)"));
        return;
    }
    
    std::string secret = CBitcoinSecret(key).ToString();
    cerr << "Decrypted: " << secret << endl;
   
    params.push_back(json_spirit::Value(secret.c_str()));
    params.push_back(json_spirit::Value(label_str.toStdString().c_str()));
    for (json_spirit::Array::iterator it = params.begin(); it != params.end(); ++it) {
        cerr << it->get_str().c_str() << endl;
    }

    WalletModel::EncryptionStatus encStatus = model->getEncryptionStatus();
    if(encStatus == model->Locked || encStatus == model->UnlockedForAnonymizationOnly)
    {
        WalletModel::UnlockContext ctx(model->requestUnlock(true));
        if(!ctx.isValid())
        {
            // Unlock wallet was cancelled
            QMessageBox::critical(this, tr("Error"), tr("Cant import key into locked wallet"));
            return;
        }
        
        try
        {
            importprivkey(params, false);
            QMessageBox::information(this, tr(""), tr("Private key imported"));
            close();
        }
        //catch (json_spirit::Object &err)
        // TODO: Cant catch expetion& To be investigate
        catch (...)
        {
            cerr << "Import private key error!" << endl;            
//            for (json_spirit::Object::iterator it = err.begin(); it != err.end(); ++it)
//            {
//                cerr << it->name_ << " = " << it->value_.get_str() << endl;
//            }
            QMessageBox::critical(this, tr("Error"), tr("Private key import error"));
        }
    }    
}

bool readHtmlTemplate(const QString &res_name, QString &htmlContent)
{
    QFile  htmlFile(res_name);
    if (!htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)){
        cerr << "Cant open " << res_name.toStdString() << endl;
        return false;
    }

    QTextStream in(&htmlFile);
    htmlContent = in.readAll();
    return true;
}

void GenAndPrintDialog::on_printButton_clicked()
{
    QString strAccount = ui->passEdit1->text();
    QString passwd = ui->passEdit2->text();

    uri = "";
    ui->passEdit2->setText("");
    ui->passEdit3->setText("");
    
    CKey secret = model->generateNewKey();
    CPrivKey privkey = secret.GetPrivKey();
    CPubKey pubkey = secret.GetPubKey();
    CKeyID keyid = pubkey.GetID();
    
    std::string secret_str = CBitcoinSecret(secret).ToString();
    std::string pubkey_str = CBitcoinAddress(keyid).ToString();
    
    QString qsecret = QString::fromStdString(secret_str);
    QString qaddress = QString::fromStdString(pubkey_str);
    
    cerr << "privkey=" << secret_str << endl;
             
    std::vector<unsigned char> crypted_key;
    model->encryptKey(secret,  passwd.toStdString(), salt, crypted_key);
    std::string crypted = EncodeBase58(crypted_key);
    QString qcrypted = QString::fromStdString(crypted);
    
    cerr << "Crypted: " << crypted << endl;
                        
    QPrinter printer;
    printer.setResolution(QPrinter::ScreenResolution);
    printer.setPageMargins(0, 10, 0, 0, QPrinter::Millimeter);
    QPrintDialog *dlg = new QPrintDialog(&printer, this);
    if(dlg->exec() == QDialog::Accepted) {
        
        QImage img1(200, 200, QImage::Format_Mono);
        QImage img2(200, 200, QImage::Format_Mono);
        QPainter painter(&img1);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setRenderHint(QPainter::TextAntialiasing, false);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.setRenderHint(QPainter::HighQualityAntialiasing, false);
        painter.setRenderHint(QPainter::NonCosmeticDefaultPen, false);
        printAsQR(painter, qaddress, 0);
        // QT bug. Painting img on pdf inverts colors
        img1.invertPixels();
        bool bEnd = painter.end();

        painter.begin(&img2);
        printAsQR(painter, qsecret, 0);
        img2.invertPixels();
        bEnd = painter.end();
        
        QString html;
        readHtmlTemplate(":/html/paperwallet", html);
        
        html.replace("__ACCOUNT__", strAccount);
        html.replace("__ADDRESS__", qaddress);
        html.replace("__PRIVATE__", qcrypted);
        
        //cerr << "size: " << html.size() << ":" << html.toStdString() << endl;
        QTextDocument *document = new QTextDocument();
        document->setHtml(html);
        document->addResource(QTextDocument::ImageResource, QUrl(":qr1.png" ), img1);
        document->addResource(QTextDocument::ImageResource, QUrl(":qr2.png" ), img2);
        document->print(&printer);
        
        bool res = model->setAddressBook(keyid, strAccount.toStdString(), "send");
        cerr << "setAddressBook: " << res << endl;
        
        SendCoinsRecipient rcp(qaddress, strAccount, 0, "");
        uri = GUIUtil::formatBitcoinURI(rcp);
        cerr << "uri=" << uri.toStdString() << endl;

        delete document;
        accept();
    }
    
    delete dlg;
}

void GenAndPrintDialog::printAsQR(QPainter &painter, QString &vchKey, int shift)
{
    QRcode *qr = QRcode_encodeString(vchKey.toStdString().c_str(), 1, QR_ECLEVEL_L, QR_MODE_8, 0);
    if(0!=qr) {
        QPaintDevice *pd = painter.device(); 
        const double w = pd->width();
        const double h = pd->height();
        QColor fg("black");
        QColor bg("white");
        painter.setBrush(bg);
        painter.fillRect(0, 0, w, h, bg);
        painter.setPen(Qt::SolidLine);
        painter.setPen(fg);
        //painter.drawRect(0, 0, width(), height());
        painter.setBrush(fg);
        const int s=qr->width > 0 ? qr->width : 1;
        const double aspect = w / h;
        const double scale = ((aspect > 1.0) ? h : w) / s;// * 0.3;
        for(int y = 0; y < s; y++){
            const int yy = y*s;
            for(int x = 0; x < s; x++){
                const int xx = yy + x;
                const unsigned char b = qr->data[xx];
                if(b & 0x01){
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
