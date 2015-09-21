// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015 The SibCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "goodsdialog.h"
#include "ui_goodsdialog.h"

#include <QFile>
#include <QTextStream>

/** "Help message" dialog box */
GoodsDialog::GoodsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GoodsDialog)
{
    ui->setupUi(this);
    //GUIUtil::restoreWindowGeometry("nHelpMessageDialogWindow", this->size(), this);
    
    QString res_name = ":/html/sibcoindesc";
    QString htmlContent;
    
    QFile  htmlFile(res_name);
    if (!htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)){
        return;
    }

    QTextStream in(&htmlFile);
    htmlContent = in.readAll();
    
    // Set help message text
    //ui->helpMessageLabel->setText(htmlContent);
}

GoodsDialog::~GoodsDialog()
{
    //GUIUtil::saveWindowGeometry("nHelpMessageDialogWindow", this);
    delete ui;
}

void GoodsDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    QString strUsage = header + "\n" + coreOptions + "\n" + uiOptions + "\n";
    fprintf(stdout, "%s", strUsage.toStdString().c_str());
}

void GoodsDialog::showOrPrint()
{
#if defined(WIN32)
        // On Windows, show a message box, as there is no stderr/stdout in windowed applications
        exec();
#else
        // On other operating systems, print help text to console
        printToConsole();
#endif
}

//void GoodsDialog::on_okButton_accepted()
//{
//    close();
//}
