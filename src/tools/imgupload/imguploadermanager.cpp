// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Yurii Puchkov & Contributors
//

#include "imguploadermanager.h"
// TODO - remove this hard-code and create plugin manager in the future, you may
// include other storage headers here
#include "tools/imgupload/storages/cloudinary/cloudinaryuploader.h"
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
    // Cloudinary is the active upload backend for screenshots.
    // Keep the upload flow explicit here so legacy plugin choices cannot
    // fall back to the old Imgur implementation.
    m_urlString = QStringLiteral("https://res.cloudinary.com/%1/image/upload/")
                    .arg(ConfigHandler().cloudinaryCloudName().trimmed());
    m_imgUploaderPlugin = "cloudinary";
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
    m_imgUploaderBase =
      (ImgUploaderBase*)(new CloudinaryUploader(capture, parent));
    if (m_imgUploaderBase && !capture.isNull()) {
        m_imgUploaderBase->upload();
    }
    return m_imgUploaderBase;
}

ImgUploaderBase* ImgUploaderManager::uploader(const QString& imgUploaderPlugin,
                                              QWidget* parent)
{
    Q_UNUSED(imgUploaderPlugin)
    init();
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
