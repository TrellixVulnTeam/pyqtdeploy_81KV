/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT3DRENDER_RENDER_OPENGL_SUBMISSIONCONTEXT_H
#define QT3DRENDER_RENDER_OPENGL_SUBMISSIONCONTEXT_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//


#include <glbuffer_p.h>
#include <glfence_p.h>
#include <graphicscontext_p.h>
#include <texturesubmissioncontext_p.h>
#include <imagesubmissioncontext_p.h>
#include <Qt3DRender/qclearbuffers.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/private/handle_types_p.h>
#include <Qt3DRender/private/attachmentpack_p.h>

QT_BEGIN_NAMESPACE

class QAbstractOpenGLFunctions;

namespace Qt3DRender {

namespace Render {

class Material;
class AttachmentPack;
class Attribute;
class Buffer;
class ShaderManager;
struct StateVariant;
class RenderTarget;
class RenderStateSet;

namespace OpenGL {

class Renderer;
class GraphicsHelperInterface;
class GLTexture;
class RenderCommand;

typedef QPair<QString, int> NamedUniformLocation;

class Q_AUTOTEST_EXPORT SubmissionContext : public GraphicsContext
{
public:
    SubmissionContext();
    ~SubmissionContext();

    int id() const; // unique, small integer ID of this context
    void setRenderer(Renderer *renderer) { m_renderer = renderer; }

    bool beginDrawing(QSurface *surface);
    void endDrawing(bool swapBuffers);
    void releaseOpenGL();
    void setOpenGLContext(QOpenGLContext* ctx);

    // Viewport
    void setViewport(const QRectF &viewport, const QSize &surfaceSize);
    QRectF viewport() const { return m_viewport; }

    // Shaders
    bool activateShader(GLShader *shader);
    QOpenGLShaderProgram *activeShader() const { return m_activeShader; }

    // FBO
    GLuint activeFBO() const { return m_activeFBO; }
    void activateRenderTarget(const Qt3DCore::QNodeId id, const AttachmentPack &attachments, GLuint defaultFboId);
    void releaseRenderTarget(const Qt3DCore::QNodeId id);
    QSize renderTargetSize(const QSize &surfaceSize) const;
    QImage readFramebuffer(const QRect &rect);
    void blitFramebuffer(Qt3DCore::QNodeId outputRenderTargetId, Qt3DCore::QNodeId inputRenderTargetId,
                         QRect inputRect,
                         QRect outputRect, uint defaultFboId,
                         QRenderTargetOutput::AttachmentPoint inputAttachmentPoint,
                         QRenderTargetOutput::AttachmentPoint outputAttachmentPoint,
                         QBlitFramebuffer::InterpolationMethod interpolationMethod);

    // Attributes
    void specifyAttribute(const Attribute *attribute,
                          Buffer *buffer,
                          const ShaderAttribute *attributeDescription);
    void specifyIndices(Buffer *buffer);

    // Buffer
    void updateBuffer(Buffer *buffer);
    QByteArray downloadBufferContent(Buffer *buffer);
    void releaseBuffer(Qt3DCore::QNodeId bufferId);
    bool hasGLBufferForBuffer(Buffer *buffer);
    GLBuffer *glBufferForRenderBuffer(Buffer *buf);

    // Parameters
    bool setParameters(ShaderParameterPack &parameterPack, GLShader *shader);

    // RenderState
    void setCurrentStateSet(RenderStateSet* ss);
    RenderStateSet *currentStateSet() const;
    void applyState(const StateVariant &state);

    void resetMasked(qint64 maskOfStatesToReset);
    void applyStateSet(RenderStateSet *ss);

    // Wrappers
    void    clearColor(const QColor &color);
    void    clearDepthValue(float depth);
    void    clearStencilValue(int stencil);


    // Fences
    GLFence fenceSync();
    void    clientWaitSync(GLFence sync, GLuint64 nanoSecTimeout);
    void    waitSync(GLFence sync);
    bool    wasSyncSignaled(GLFence sync);
    void    deleteSync(GLFence sync);

    // Textures
    void setUpdatedTexture(const Qt3DCore::QNodeIdVector &updatedTextureIds);

private:
    struct RenderTargetInfo {
        GLuint fboId;
        QSize size;
        AttachmentPack attachments;
    };

    void initialize();

    // Material
    Material* activeMaterial() const { return m_material; }
    void setActiveMaterial(Material* rmat);

    // FBO
    RenderTargetInfo bindFrameBufferAttachmentHelper(GLuint fboId, const AttachmentPack &attachments);
    void activateDrawBuffers(const AttachmentPack &attachments);
    void resolveRenderTargetFormat();
    GLuint createRenderTarget(Qt3DCore::QNodeId renderTargetNodeId, const AttachmentPack &attachments);
    GLuint updateRenderTarget(Qt3DCore::QNodeId renderTargetNodeId, const AttachmentPack &attachments, bool isActiveRenderTarget);

    // Buffers
    HGLBuffer createGLBufferFor(Buffer *buffer);
    void uploadDataToGLBuffer(Buffer *buffer, GLBuffer *b, bool releaseBuffer = false);
    QByteArray downloadDataFromGLBuffer(Buffer *buffer, GLBuffer *b);
    bool bindGLBuffer(GLBuffer *buffer, GLBuffer::Type type);

    bool m_ownCurrent;
    const unsigned int m_id;
    QSurface *m_surface;
    QSize m_surfaceSize;

    QOpenGLShaderProgram *m_activeShader;

    QHash<Qt3DCore::QNodeId, HGLBuffer> m_renderBufferHash;


    QHash<Qt3DCore::QNodeId, RenderTargetInfo> m_renderTargets;
    QAbstractTexture::TextureFormat m_renderTargetFormat;

    // cache some current state, to make sure we don't issue unnecessary GL calls
    int m_currClearStencilValue;
    float m_currClearDepthValue;
    QColor m_currClearColorValue;

    Material* m_material;
    QRectF m_viewport;
    GLuint m_activeFBO;
    Qt3DCore::QNodeId m_activeFBONodeId;

    GLBuffer *m_boundArrayBuffer;
    RenderStateSet* m_stateSet;
    Renderer *m_renderer;
    QByteArray m_uboTempArray;

    TextureSubmissionContext m_textureContext;
    ImageSubmissionContext m_imageContext;

    // Attributes
    friend class OpenGLVertexArrayObject;

    struct VAOVertexAttribute
    {
        HGLBuffer bufferHandle;
        GLBuffer::Type attributeType;
        int location;
        GLint dataType;
        uint byteOffset;
        uint vertexSize;
        uint byteStride;
        uint divisor;
        GLenum shaderDataType;
    };

    using VAOIndexAttribute = HGLBuffer;
    void enableAttribute(const VAOVertexAttribute &attr);
    void disableAttribute(const VAOVertexAttribute &attr);

    Qt3DCore::QNodeIdVector m_updateTextureIds;
};

} // namespace OpenGL
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_OPENGL_SUBMISSIONCONTEXT_H
