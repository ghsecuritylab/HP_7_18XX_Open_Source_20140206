/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef GLUtils_h
#define GLUtils_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "IntRect.h"
#include "SkBitmap.h"
#include "SkMatrix.h"
#include "TransformationMatrix.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace android {

class SurfaceTexture;

} // namespace android

namespace WebCore {

class TileRenderInfo;

class GLUtils {

public:
    // Matrix utilities
    static void toGLMatrix(GLfloat* flattened, const TransformationMatrix& matrix);
    static void toSkMatrix(SkMatrix& skmatrix, const TransformationMatrix& matrix);
    static void setOrthographicMatrix(TransformationMatrix& ortho, float left, float top,
                                      float right, float bottom, float nearZ, float farZ);
    static bool has3dTransform(const TransformationMatrix& matrix);

    // GL & EGL error checks
    static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE);
    static bool checkGlErrorOn(void* p, const char* op);
    static bool checkGlError(const char* op);
    static void checkSurfaceTextureError(const char* functionName, int status);

    // GL & EGL extension checks
    static bool isEGLImageSupported();
    static bool isEGLFenceSyncSupported();

    // Texture utilities
    static EGLContext createBackgroundContext(EGLContext sharedContext);
    static void deleteTexture(GLuint* texture);
    static GLuint createSampleColorTexture(int r, int g, int b);
    static GLuint createSampleTexture();
    static GLuint createTileGLTexture(int width, int height);

    static void createTextureWithBitmap(GLuint texture, const SkBitmap& bitmap, GLint filter = GL_LINEAR);
    static void updateTextureWithBitmap(GLuint texture, const SkBitmap& bitmap,
                                        const IntRect& inval = IntRect(), GLint filter = GL_LINEAR);
    static void createEGLImageFromTexture(GLuint texture, EGLImageKHR* image);
    static void createTextureFromEGLImage(GLuint texture, EGLImageKHR image, GLint filter = GL_LINEAR);
    static EGLSurface createPbufferSurface(EGLDisplay display, const EGLConfig& config, EGLint* errorCode);
    static EGLConfig defaultPbufferConfig(EGLDisplay display);
    static EGLContext mContext;

    static void paintTextureWithBitmap(const TileRenderInfo* renderInfo, const SkBitmap& bitmap);
    static void updateQueueWithBitmap(const TileRenderInfo* , const SkBitmap& bitmap);
    static bool updateSharedSurfaceTextureWithBitmap(ANativeWindow* anw, const SkBitmap& bitmap);
    static void convertToTransformationMatrix(const float* matrix, TransformationMatrix& transformMatrix);

    static bool deepCopyBitmapSubset(const SkBitmap& sourceBitmap,
                                     SkBitmap& subset, int left, int top);
    static bool isPureColorBitmap(const SkBitmap& bitmap, Color& pureColor);
    static bool skipTransferForPureColor(const TileRenderInfo* renderInfo,
                                         const SkBitmap& bitmap);
    static void clearBackgroundIfOpaque(const Color* backgroundColor);
    static bool allowGLLog();
    static double m_previousLogTime;
    static int m_currentLogCounter;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // GLUtils_h
