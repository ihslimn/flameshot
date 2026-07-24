// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tools/imgupload/storages/imguploaderbase.h"

class QNetworkAccessManager;
class QNetworkReply;

class FreeImageHostUploader : public ImgUploaderBase
{
    Q_OBJECT
public:
    explicit FreeImageHostUploader(const QPixmap& capture,
                                   QWidget* parent = nullptr);

    void upload() override;
    void deleteImage(const QString& fileName,
                     const QString& deleteToken) override;

private slots:
    void handleReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkManager;
};
