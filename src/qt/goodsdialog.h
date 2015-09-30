// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015 The SibCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GOODSDIALOG_H
#define	GOODSDIALOG_H

#include "walletmodel.h"
#include "sibmodel.h"

#include <QDialog>
#include <QObject>

namespace Ui {
    class GoodsDialog;
}

/** Dialog to show the HTML page with sales points that accept sibcoins */
class GoodsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoodsDialog(QWidget *parent=0);
    ~GoodsDialog();

    void setModel(WalletModel *model);
    void setSibModel(SibModel *sibModel);
    
private:
    Ui::GoodsDialog *ui;
    QString header;
    QString coreOptions;
    QString uiOptions;
    
    WalletModel *model;
    SibModel *sibModel;
    
    const QString page_name;
    
public slots:
    void on_resourceReady(std::string res_root);

};


#endif	/* GOODSDIALOG_H */

