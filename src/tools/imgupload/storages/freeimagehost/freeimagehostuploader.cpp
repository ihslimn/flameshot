// SPDX-License-Identifier: GPL-3.0-or-later

#include "freeimagehostuploader.h"
#include "utils/confighandler.h"
#include "utils/history.h"
#include "widgets/loadspinner.h"

#include <QBuffer>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShortcut>

FreeImageHostUploader::FreeImageHostUploader(const QPixmap& capture,
                                             QWidget* parent)
  : ImgUploaderBase(capture, parent)
  , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager,
            &QNetworkAccessManager::finished,
            this,
            &FreeImageHostUploader::handleReply);
}

void FreeImageHostUploader::upload()
{
    const QString apiKey = ConfigHandler().freeImageHostApiKey().trimmed();
    if (apiKey.isEmpty()) {
        setInfoLabelText(
          tr("freeimage.host is not configured. Set your API key in Settings."));
        return;
    }

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    pixmap().save(&buffer, "PNG");

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    const auto appendTextPart = [multiPart](const QString& name,
                                            const QByteArray& value) {
        QHttpPart part;
        part.setHeader(
          QNetworkRequest::ContentDispositionHeader,
          QStringLiteral("form-data; name=\"%1\"").arg(name));
        part.setBody(value);
        multiPart->append(part);
    };
    appendTextPart(QStringLiteral("key"), apiKey.toUtf8());
    appendTextPart(QStringLiteral("action"), QByteArrayLiteral("upload"));
    appendTextPart(QStringLiteral("format"), QByteArrayLiteral("json"));

    QHttpPart sourcePart;
    sourcePart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QStringLiteral("form-data; name=\"source\"; filename=\"flameshot.png\""));
    sourcePart.setHeader(QNetworkRequest::ContentTypeHeader,
                         QStringLiteral("image/png"));
    sourcePart.setBody(imageData);
    multiPart->append(sourcePart);

    QNetworkReply* reply = m_networkManager->post(
      QNetworkRequest(QUrl(QStringLiteral("https://freeimage.host/api/1/upload"))),
      multiPart);
    multiPart->setParent(reply);
}

void FreeImageHostUploader::handleReply(QNetworkReply* reply)
{
    spinner()->deleteLater();
    const QJsonObject json =
      QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonObject image = json.value(QStringLiteral("image")).toObject();
    const QString imageUrl = image.value(QStringLiteral("url")).toString();

    if (reply->error() == QNetworkReply::NoError && !imageUrl.isEmpty()) {
        setImageURL(QUrl(imageUrl));
        const QString fileName = QFileInfo(imageURL().path()).fileName();
        m_currentImageName =
          History().packFileName(QStringLiteral("freeimage.host"),
                                 QString(),
                                 fileName);
        History().save(pixmap(), m_currentImageName);
        emit uploadOk(imageURL());
    } else {
        QString message = json.value(QStringLiteral("status_txt")).toString();
        if (message.isEmpty()) {
            message = reply->errorString();
        }
        setInfoLabelText(
          tr("freeimage.host upload failed: %1").arg(message));
    }

    reply->deleteLater();
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void FreeImageHostUploader::deleteImage(const QString& fileName,
                                        const QString& deleteToken)
{
    Q_UNUSED(fileName)
    Q_UNUSED(deleteToken)
    emit deleteFailed(
      tr("The freeimage.host API does not provide image deletion."));
}
