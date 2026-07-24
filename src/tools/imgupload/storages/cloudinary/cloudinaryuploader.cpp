// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Contributors

#include "cloudinaryuploader.h"
#include "utils/confighandler.h"
#include "utils/filenamehandler.h"
#include "utils/history.h"
#include "widgets/loadspinner.h"
#include "widgets/notificationwidget.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShortcut>
#include <QDateTime>
#include <QUrlQuery>

CloudinaryUploader::CloudinaryUploader(const QPixmap& capture, QWidget* parent)
  : ImgUploaderBase(capture, parent)
{
    m_NetworkAM = new QNetworkAccessManager(this);
    connect(m_NetworkAM,
            &QNetworkAccessManager::finished,
            this,
            &CloudinaryUploader::handleReply);
}

void CloudinaryUploader::handleReply(QNetworkReply* reply)
{
    if (reply->property("cloudinaryOperation").toString() ==
        QStringLiteral("delete")) {
        handleDeleteReply(reply);
        reply->deleteLater();
        return;
    }

    spinner()->deleteLater();
    m_currentImageName.clear();

    const QByteArray responseData = reply->readAll();
    const QJsonDocument response = QJsonDocument::fromJson(responseData);
    const QJsonObject json = response.object();

    if (reply->error() == QNetworkReply::NoError) {
        QString imageUrl = json.value(QStringLiteral("secure_url")).toString();
        if (imageUrl.isEmpty()) {
            imageUrl = json.value(QStringLiteral("url")).toString();
        }

        if (!imageUrl.isEmpty()) {
            setImageURL(QUrl(imageUrl));

            m_currentImageName = imageURL().toString();
            const int lastSlash = m_currentImageName.lastIndexOf('/');
            if (lastSlash >= 0) {
                m_currentImageName = m_currentImageName.mid(lastSlash + 1);
            }

            const QString version =
              json.value(QStringLiteral("version")).toVariant().toString();

            History history;
            m_currentImageName =
              history.packFileName("cloudinary", version, m_currentImageName);
            history.save(pixmap(), m_currentImageName);

            emit uploadOk(imageURL());
        } else {
            setInfoLabelText(tr("Cloudinary did not return a usable image URL."));
        }
    } else {
        QString status;
        if (json.contains(QStringLiteral("error")) &&
            json.value(QStringLiteral("error")).isObject()) {
            const QJsonObject errorObj =
              json.value(QStringLiteral("error")).toObject();
            status = errorObj.value(QStringLiteral("message")).toString();
        }

        setInfoLabelText(reply->errorString() + "\n" + status);
    }

    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void CloudinaryUploader::upload()
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    pixmap().save(&buffer, "PNG");

    const QString cloudName = ConfigHandler().cloudinaryCloudName().trimmed();
    const QString uploadPreset =
      ConfigHandler().cloudinaryUploadPreset().trimmed();
    const bool useSignedPreset =
      ConfigHandler().cloudinaryUseSignedPreset();
    const QString apiSecret =
      ConfigHandler().cloudinaryApiSecret().trimmed();
        const QString apiKey =
            ConfigHandler().cloudinaryApiKey().trimmed();

    if (cloudName.isEmpty() || uploadPreset.isEmpty()) {
        setInfoLabelText(tr("Cloudinary is not configured. Set your cloud name and upload preset in Settings."));
        return;
    }

    if (useSignedPreset && apiSecret.isEmpty()) {
        setInfoLabelText(tr("Signed preset selected but API secret not configured."));
        return;
    }
    if (useSignedPreset && apiKey.isEmpty()) {
        setInfoLabelText(tr("Signed preset selected but API key not configured."));
        return;
    }

    QUrl url(QStringLiteral("https://api.cloudinary.com/v1_1/%1/image/upload")
               .arg(cloudName));
    QNetworkRequest request(url);

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"flameshot.png\"")));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant(QStringLiteral("image/png")));
    filePart.setBody(byteArray);
    multiPart->append(filePart);

    QHttpPart presetPart;
    presetPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant(QStringLiteral("form-data; name=\"upload_preset\"")));
    presetPart.setBody(uploadPreset.toUtf8());
    multiPart->append(presetPart);

    // Pre-compute context value to include in signature
    const QString description = FileNameHandler().parsedPattern();
    QString contextValue;
    if (!description.isEmpty()) {
        contextValue = QStringLiteral("caption=%1").arg(description);
    }

    // Add signature for signed presets (must be computed with all parameters in alphabetical order)
    if (useSignedPreset) {
        const qint64 timestamp = QDateTime::currentSecsSinceEpoch();
        const QString timestampStr = QString::number(timestamp);

        // Build the string to sign with all parameters in alphabetical order:
        // context (if present), timestamp, upload_preset
        QStringList params;
        if (!contextValue.isEmpty()) {
            params.append(QStringLiteral("context=%1").arg(contextValue));
        }
        params.append(QStringLiteral("timestamp=%1").arg(timestampStr));
        params.append(QStringLiteral("upload_preset=%1").arg(uploadPreset));

        const QString toSign = params.join(QStringLiteral("&"));

        // Generate SHA-256 signature
        const QByteArray signatureData = QCryptographicHash::hash(
          (toSign + apiSecret).toUtf8(), QCryptographicHash::Sha256);
        const QString signature = QString::fromLatin1(signatureData.toHex());

        QHttpPart timestampPart;
        timestampPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                QVariant(QStringLiteral("form-data; name=\"timestamp\"")));
        timestampPart.setBody(timestampStr.toUtf8());
        multiPart->append(timestampPart);

        QHttpPart signaturePart;
        signaturePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                QVariant(QStringLiteral("form-data; name=\"signature\"")));
        signaturePart.setBody(signature.toUtf8());
        multiPart->append(signaturePart);

        // Add API key for signed uploads
        QHttpPart apiKeyPart;
        apiKeyPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                             QVariant(QStringLiteral("form-data; name=\"api_key\"")));
        apiKeyPart.setBody(apiKey.toUtf8());
        multiPart->append(apiKeyPart);
    }

    if (!contextValue.isEmpty()) {
        QHttpPart contextPart;
        contextPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                              QVariant(QStringLiteral("form-data; name=\"context\"")));
        contextPart.setBody(contextValue.toUtf8());
        multiPart->append(contextPart);
    }

    m_NetworkAM->post(request, multiPart);
}

void CloudinaryUploader::deleteImage(const QString& fileName,
                                     const QString& deleteToken)
{
    Q_UNUSED(deleteToken)

    const QString cloudName = ConfigHandler().cloudinaryCloudName().trimmed();
    const QString apiKey = ConfigHandler().cloudinaryApiKey().trimmed();
    const QString apiSecret = ConfigHandler().cloudinaryApiSecret().trimmed();
    if (cloudName.isEmpty() || apiKey.isEmpty() || apiSecret.isEmpty()) {
        emit deleteFailed(tr("Cloudinary deletion requires cloud name, API key, "
                             "and API secret in Settings."));
        return;
    }

    const QString publicId = QFileInfo(fileName).completeBaseName();
    if (publicId.isEmpty()) {
        emit deleteFailed(
          tr("Unable to determine Cloudinary public ID for this screenshot."));
        return;
    }

    const QString timestamp =
      QString::number(QDateTime::currentSecsSinceEpoch());
    const QString toSign =
      QStringLiteral("invalidate=true&public_id=%1&timestamp=%2")
        .arg(publicId, timestamp);
    const QByteArray signatureData = QCryptographicHash::hash(
      (toSign + apiSecret).toUtf8(), QCryptographicHash::Sha256);
    const QString signature = QString::fromLatin1(signatureData.toHex());

    QUrl url(QStringLiteral("https://api.cloudinary.com/v1_1/%1/image/destroy")
               .arg(cloudName));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("invalidate"), QStringLiteral("true"));
    params.addQueryItem(QStringLiteral("public_id"), publicId);
    params.addQueryItem(QStringLiteral("timestamp"), timestamp);
    params.addQueryItem(QStringLiteral("api_key"), apiKey);
    params.addQueryItem(QStringLiteral("signature"), signature);

    QNetworkReply* reply =
      m_NetworkAM->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("cloudinaryOperation", QStringLiteral("delete"));
}

void CloudinaryUploader::handleDeleteReply(QNetworkReply* reply)
{
    const QByteArray responseData = reply->readAll();
    const QJsonDocument response = QJsonDocument::fromJson(responseData);
    const QJsonObject json = response.object();

    const QString result = json.value(QStringLiteral("result")).toString();
    if (reply->error() == QNetworkReply::NoError &&
        (result == QStringLiteral("ok") ||
         result == QStringLiteral("not found"))) {
        emit deleteOk();
        return;
    }

    QString message = reply->errorString();
    if (json.contains(QStringLiteral("error")) &&
        json.value(QStringLiteral("error")).isObject()) {
        const QJsonObject errorObj =
          json.value(QStringLiteral("error")).toObject();
        message = errorObj.value(QStringLiteral("message")).toString(message);
    } else if (!result.isEmpty()) {
        message = tr("Cloudinary returned: %1").arg(result);
    }

    emit deleteFailed(message);
}
