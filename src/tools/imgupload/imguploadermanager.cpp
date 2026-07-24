// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Yurii Puchkov & Contributors
//

#include "imguploadermanager.h"
// TODO - remove this hard-code and create plugin manager in the future, you may
// include other storage headers here
#include "tools/imgupload/storages/cloudinary/cloudinaryuploader.h"
#include "tools/imgupload/storages/freeimagehost/freeimagehostuploader.h"
#include "utils/confighandler.h"

#include <QPixmap>
#include <QWidget>

ImgUploaderManager::ImgUploaderManager(QObject* parent)
  : QObject(parent)
  , m_imgUploaderBase(nullptr)
{
    // TODO - implement ImgUploader for other Storages and selection among them
    m_imgUploaderPlugin = IMG_UPLOADER_STORAGE_DEFAULT;
    init();
}

void ImgUploaderManager::init()
{
    m_imgUploaderPlugin = ConfigHandler().uploadProvider();
    if (m_imgUploaderPlugin == QStringLiteral("freeimage.host")) {
        m_urlString = QStringLiteral("https://iili.io/");
    } else {
        m_imgUploaderPlugin = QStringLiteral("cloudinary");
        m_urlString =
          QStringLiteral("https://res.cloudinary.com/%1/image/upload/")
            .arg(ConfigHandler().cloudinaryCloudName().trimmed());
    }
}

ImgUploaderBase* ImgUploaderManager::uploader(const QPixmap& capture,
                                              QWidget* parent)
{
    // TODO - implement ImgUploader for other Storages and selection among them,
    // example:
    // if (uploaderPlugin().compare("s3") == 0) {
    //    m_imgUploaderBase =
    //      (ImgUploaderBase*)(new ImgS3Uploader(capture, parent));
    //} else {
    //    m_imgUploaderBase =
    //      (ImgUploaderBase*)(new ImgurUploader(capture, parent));
    //}
    if (m_imgUploaderPlugin == QStringLiteral("freeimage.host")) {
        m_imgUploaderBase = new FreeImageHostUploader(capture, parent);
    } else {
        m_imgUploaderBase = new CloudinaryUploader(capture, parent);
    }
    if (m_imgUploaderBase && !capture.isNull()) {
        m_imgUploaderBase->upload();
    }
    return m_imgUploaderBase;
}

ImgUploaderBase* ImgUploaderManager::uploader(const QString& imgUploaderPlugin,
                                              QWidget* parent)
{
    init();
    if (imgUploaderPlugin == QStringLiteral("freeimage.host")) {
        m_imgUploaderPlugin = imgUploaderPlugin;
        m_urlString = QStringLiteral("https://iili.io/");
    } else if (imgUploaderPlugin == QStringLiteral("cloudinary")) {
        m_imgUploaderPlugin = imgUploaderPlugin;
        m_urlString =
          QStringLiteral("https://res.cloudinary.com/%1/image/upload/")
            .arg(ConfigHandler().cloudinaryCloudName().trimmed());
    }
    return uploader(QPixmap(), parent);
}

const QString& ImgUploaderManager::uploaderPlugin()
{
    return m_imgUploaderPlugin;
}

const QString& ImgUploaderManager::url()
{
    return m_urlString;
}
