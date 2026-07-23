// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Contributors

#pragma once

#include "tools/imgupload/storages/imguploaderbase.h"

#include <QObject>

class QPixmap;
class QWidget;
class QNetworkReply;
class QNetworkAccessManager;

class CloudinaryUploader : public ImgUploaderBase
{
    Q_OBJECT
public:
    explicit CloudinaryUploader(const QPixmap& capture,
                                QWidget* parent = nullptr);
    void deleteImage(const QString& fileName, const QString& deleteToken);

private slots:
    void handleReply(QNetworkReply* reply);

private:
    void upload();

private:
    QNetworkAccessManager* m_NetworkAM;
};
