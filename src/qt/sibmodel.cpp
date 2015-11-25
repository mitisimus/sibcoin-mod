// Copyright (c) 2015 The Sibcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sibmodel.h"
#include "util.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QResource>
#include <QCryptographicHash>
#include <QTextStream>
#include <QFile>
#include <QTemporaryFile>

const int MAX_GOODS_URLS = 2;

const QString GOODS_URLS[MAX_GOODS_URLS] = {
		"http://sibcoin.net/goods/",
		"http://chervonec.com/goods/"
};

const QString goods_data = "sibcoin.rcc";
const QString goods_md5 = "sibcoin.md5";


SibModel::SibModel(CSibDB *sibdb, QObject *parent) :
    QObject(parent),
    res_prefix("/dev"),
    sibDB(sibdb),
    net_manager(0),
    state(ST_INIT),
	try_idx(0)
{
    net_manager = new QNetworkAccessManager(this);
    connect(net_manager, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinished(QNetworkReply*)));

//    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)),
//            this,  SLOT(onError(QNetworkReply::NetworkError)));
}

SibModel::~SibModel()
{
    delete net_manager;
}

bool SibModel::registerRes() {
    QTemporaryFile file;
    if (!file.open()) {
        LogPrintf("Could not open %s for writing\n", file.fileName().toStdString().c_str());
        return false;
    }

    file.write(rccData);
    file.close();

    if (!QResource::registerResource(file.fileName(), res_prefix)) {
        LogPrintf("Can't load sib resource\n");
        return false;
    }
    return true;
}

void SibModel::loadLocalResource()
{      
    
    if (readResourceWithMD5()) {
        registerRes();
        Q_EMIT resourceReady(res_prefix.toStdString());
    }
}

void SibModel::fetch()
{
    loadLocalResource();
    fetch_url(0);
}

void SibModel::fetch_url(int _idx) {
	try_idx = _idx;

	if (try_idx >= MAX_GOODS_URLS)
		return;

	data_url = GOODS_URLS[try_idx];
	LogPrintf("Try loading webpage from %s\n", data_url.toStdString().c_str());

    QNetworkReply *rep = net_manager->get(QNetworkRequest(QUrl(data_url + goods_data)));
    if (rep == NULL) {
    	fetch_url(try_idx + 1);
    	return;
    }
    state = ST_LOADING_RCC;
}

void SibModel::replyFinished(QNetworkReply* p_reply)
{
	QNetworkReply::NetworkError err = p_reply->error();
	if (err != QNetworkReply::NoError) {
        state = ST_ERROR;
        LogPrintf("Error loading webpage: %s\n", QString::number(err).toStdString().c_str());
       	fetch_url(try_idx + 1);
       	return;
	}

    if (state == ST_LOADING_RCC) {
		rccData = p_reply->readAll();
		net_manager->get(QNetworkRequest(QUrl(data_url + goods_md5)));
		state = ST_LOADING_MD5;
    }
    else if (state == ST_LOADING_MD5) {   
        rccMD5 = QString(QCryptographicHash::hash((rccData), QCryptographicHash::Md5).toHex());
        QString remoteMD5 = p_reply->readAll();
        
        LogPrintf("remoteMD5: %s;", remoteMD5.toStdString().c_str());
        LogPrintf("rccMD5: %s\n", rccMD5.toStdString().c_str());
        
        if (remoteMD5 == rccMD5) {
            registerRes();
            saveResourceWithMD5();
            Q_EMIT resourceReady(res_prefix.toStdString());
        }
        else {
            LogPrintf("md5 not valid. Calculated: %s, received: %s\n",
                    rccMD5.toStdString().c_str(),
                    remoteMD5.toStdString().c_str());
           	fetch_url(try_idx + 1);
        }
    }
}


bool SibModel::saveResourceWithMD5()
{
    return sibDB->WriteName("res_dev", rccData.toBase64().constData())
        && sibDB->WriteName("res_dev_md5", rccMD5.toStdString());
}
    
bool SibModel::readResourceWithMD5() 
{
    std::string s_rccMD5;
    std::string s_rccData;
 
    bool b1 = sibDB->ReadName("res_dev", s_rccData);
    bool b2 = sibDB->ReadName("res_dev_md5", s_rccMD5);
    
    LogPrintf("read md5 from DB: %s\n", s_rccMD5.c_str());
    
    if (!b1 || !b2) {
        return false;
    }
    rccData = QByteArray::fromBase64(QByteArray(s_rccData.c_str()));
    rccMD5 = QString(s_rccMD5.c_str());
    return true;
}
