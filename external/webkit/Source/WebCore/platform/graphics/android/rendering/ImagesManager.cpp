/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "ImagesManager"
#define LOG_NDEBUG 1

#include "config.h"
#include "ImagesManager.h"

#include "AndroidLog.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkRefCnt.h"
#include "ImageTexture.h"

namespace WebCore {

ImagesManager* ImagesManager::instance()
{
    if (!gInstance)
        gInstance = new ImagesManager();

    return gInstance;
}

ImagesManager* ImagesManager::gInstance = 0;

ImageTexture* ImagesManager::setImage(SkBitmapRef* imgRef)
{
    if (!imgRef)
        return 0;

    TRACE_METHOD();
    SkBitmap* bitmap = &imgRef->bitmap();
    ImageTexture* image = 0;
    SkBitmap* img = 0;
    unsigned crc = 0;

    crc = ImageTexture::computeCRC(bitmap);

    {
        android::Mutex::Autolock lock(m_imagesLock);
        if (m_images.contains(crc)) {
            image = m_images.get(crc);
            SkSafeRef(image);
            return image;
        }
    }

    // the image is not in the map, we add it

    img = ImageTexture::convertBitmap(bitmap);
    image = new ImageTexture(img, crc);

    android::Mutex::Autolock lock(m_imagesLock);
    m_images.set(crc, image);

    return image;
}

ImageTexture* ImagesManager::retainImage(unsigned imgCRC)
{
    if (!imgCRC)
        return 0;

    android::Mutex::Autolock lock(m_imagesLock);
    ImageTexture* image = 0;
    if (m_images.contains(imgCRC)) {
        image = m_images.get(imgCRC);
        SkSafeRef(image);
    }
    return image;
}

void ImagesManager::releaseImage(unsigned imgCRC)
{
    if (!imgCRC)
        return;

    android::Mutex::Autolock lock(m_imagesLock);
    if (m_images.contains(imgCRC)) {
        ImageTexture* image = m_images.get(imgCRC);
        if (image->getRefCnt() == 1)
            m_images.remove(imgCRC);
        SkSafeUnref(image);
    }
}

int ImagesManager::nbTextures()
{
    android::Mutex::Autolock lock(m_imagesLock);
    HashMap<unsigned, ImageTexture*>::iterator end = m_images.end();
    int i = 0;
    int nb = 0;
    for (HashMap<unsigned, ImageTexture*>::iterator it = m_images.begin(); it != end; ++it) {
        // TODO: Do we need to remove the obsolete textures here?
        nb += it->second->nbTextures();
        i++;
    }
    return nb;
}

bool ImagesManager::prepareTextures(GLWebViewState* state)
{
    bool ret = false;
    android::Mutex::Autolock lock(m_imagesLock);
    HashMap<unsigned, ImageTexture*>::iterator end = m_images.end();
    for (HashMap<unsigned, ImageTexture*>::iterator it = m_images.begin(); it != end; ++it) {
        // When the texture was deleted getRefCnt() will not return expected value...
        if (it->second->getRefCnt() == 0 || !ImageTexture::isAlive(it->second))
        {
            m_images.remove(it->first);
            continue;
        }
        ret |= it->second->prepareGL(state);
    }
    return ret;
}

} // namespace WebCore
