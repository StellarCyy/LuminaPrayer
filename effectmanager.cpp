#include "effectmanager.h"
#include "profilemanager.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QDebug>
#include <cmath>


// =============================================
// Construction / destruction
// =============================================

EffectManager::EffectManager(QObject *parent)
    : QObject(parent),
      m_renderTimer(new QTimer(this))
{
    m_elapsed.start();

    const auto &bfx = ProfileManager::instance()->breathEffect();
    int fps = qBound(1, bfx.render_fps, 120);
    m_renderTimer->setInterval(1000 / fps);
    connect(m_renderTimer, &QTimer::timeout, this, &EffectManager::renderFrame);
}

EffectManager::~EffectManager() {
    cleanupGL();
}

// =============================================
// Public interface
// =============================================

void EffectManager::setHaloTexture(const QPixmap &pixmap) {
    m_pendingPixmap = pixmap;
    m_needsTextureUpload = true;
}

void EffectManager::setCpuUsage(float usage) {
    m_cpuUsage = qBound(0.0f, usage, 1.0f);
}

void EffectManager::setEffectActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    if (active) {
        if (!m_glInitialized) initGL();
        if (!m_glInitialized) return;  // M-06: initGL failed
        // H-07: Acquire context once and hold it while effect is active
        if (!m_contextCurrent && m_context && m_surface) {
            if (m_context->makeCurrent(m_surface.get()))
                m_contextCurrent = true;
        }
        m_elapsed.restart();
        m_renderTimer->start();
    } else {
        m_renderTimer->stop();
        // H-07: Release context when effect goes inactive
        if (m_contextCurrent && m_context) {
            m_context->doneCurrent();
            m_contextCurrent = false;
        }
    }
}

int EffectManager::frameSize() const {
    return ProfileManager::instance()->breathEffect().base_halo_size;
}

// =============================================
// OpenGL lifecycle
// =============================================

void EffectManager::initGL() {
    if (m_glInitialized) return;

    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);

    m_surface = std::make_unique<QOffscreenSurface>();
    m_surface->setFormat(fmt);
    m_surface->create();

    m_context = std::make_unique<QOpenGLContext>();
    m_context->setFormat(fmt);
    if (!m_context->create()) {
        qWarning("[EffectManager] Failed to create OpenGL context");
        m_context.reset();
        m_surface.reset();
        return;
    }

    if (!m_context->makeCurrent(m_surface.get())) {
        qWarning("[EffectManager] Failed to makeCurrent");
        m_context.reset();
        m_surface.reset();
        return;
    }

    const int sz = frameSize();
    QOpenGLFramebufferObjectFormat fboFmt;
    fboFmt.setInternalTextureFormat(GL_RGBA8);
    m_fbo = std::make_unique<QOpenGLFramebufferObject>(sz, sz, fboFmt);

    m_program = std::make_unique<QOpenGLShaderProgram>();
    bool okV = m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                                   QStringLiteral(":/shaders/breathe.vert"));
    bool okF = m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                                   QStringLiteral(":/shaders/breathe.frag"));
    m_program->bindAttributeLocation("a_position", 0);
    m_program->bindAttributeLocation("a_texCoord", 1);
    bool linked = m_program->link();

    // M-06: If shader pipeline is broken, tear down and abort
    if (!linked || !okV || !okF) {
        qWarning("[EffectManager] SHADER PIPELINE BROKEN: V=%d F=%d L=%d", okV, okF, linked);
        m_context->doneCurrent();
        cleanupGL();
        return;
    }

    // H-07: Leave context current — setEffectActive will manage the lifecycle
    m_context->doneCurrent();
    m_glInitialized = true;
}

void EffectManager::cleanupGL() {
    if (!m_glInitialized) return;

    // M-04: Guard against makeCurrent failure — only call GL functions if context is valid
    bool contextOk = m_contextCurrent;
    if (!contextOk && m_context && m_surface)
        contextOk = m_context->makeCurrent(m_surface.get());

    if (contextOk && m_context) {
        auto *f = m_context->functions();
        if (m_textureId) {
            f->glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
    }

    // M-07: unique_ptr::reset() replaces manual delete
    m_program.reset();
    m_fbo.reset();

    if (m_context && (contextOk || m_contextCurrent))
        m_context->doneCurrent();
    m_contextCurrent = false;

    m_context.reset();
    m_surface.reset();

    m_glInitialized = false;
}

// =============================================
// Render one frame to FBO, read back as QImage
// =============================================

void EffectManager::renderFrame() {
    if (!m_active || !m_glInitialized || !m_fbo || !m_program || !m_program->isLinked())
        return;

    // H-07: Context is kept current while effect is active — only re-acquire if lost
    if (!m_contextCurrent) {
        if (!m_context || !m_surface || !m_context->makeCurrent(m_surface.get()))
            return;
        m_contextCurrent = true;
    }

    auto *f = m_context->functions();

    // ---- Lazy texture upload (CPU-side RGBA8888 → GL_RGBA) ----
    //
    // QImage::Format_RGBA8888 guarantees byte order R, G, B, A in memory
    // regardless of CPU endianness.  GL_RGBA + GL_UNSIGNED_BYTE reads
    // bytes in the same order → zero ambiguity, no driver-dependent swaps.
    //
    if (m_needsTextureUpload && !m_pendingPixmap.isNull()) {
        if (m_textureId) {
            f->glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }

        QImage img = m_pendingPixmap.toImage()
                         .convertToFormat(QImage::Format_RGBA8888)
                         .mirrored();

        f->glGenTextures(1, &m_textureId);
        f->glBindTexture(GL_TEXTURE_2D, m_textureId);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                        img.width(), img.height(), 0,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        img.constBits());

        m_needsTextureUpload = false;
    }

    if (!m_textureId) {
        return;
    }

    // ---- Draw to FBO ----
    m_fbo->bind();

    const int sz = m_fbo->width();
    f->glViewport(0, 0, sz, sz);
    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);

    f->glEnable(GL_BLEND);
    // Separate blend: color premultiplied, alpha accumulated correctly
    f->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ONE,       GL_ONE_MINUS_SRC_ALPHA);

    m_program->bind();
    f->glBindTexture(GL_TEXTURE_2D, m_textureId);

    const auto &bfx = ProfileManager::instance()->breathEffect();
    const float elapsed = static_cast<float>(m_elapsed.elapsed()) / 1000.0f;

    m_program->setUniformValue("u_haloTex",       0);
    m_program->setUniformValue("u_time",          elapsed);
    m_program->setUniformValue("u_cpuUsage",      m_cpuUsage);
    m_program->setUniformValue("u_baseSpeed",     static_cast<float>(bfx.base_speed));
    m_program->setUniformValue("u_scaleFactor",   static_cast<float>(bfx.cpu_scale_factor));
    m_program->setUniformValue("u_minAlpha",      static_cast<float>(bfx.min_alpha_factor));
    m_program->setUniformValue("u_warmThreshold", static_cast<float>(bfx.warm_tint_threshold));
    m_program->setUniformValue("u_warmColor",
        QVector3D(static_cast<float>(bfx.warm_tint_color.redF()),
                  static_cast<float>(bfx.warm_tint_color.greenF()),
                  static_cast<float>(bfx.warm_tint_color.blueF())));
    m_program->setUniformValue("u_warmIntensity", static_cast<float>(bfx.warm_tint_intensity));
    m_program->setUniformValue("u_forceSwapRB",   bfx.force_swap_rb ? 1.0f : 0.0f);

    // Fullscreen quad
    static const float vertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->setAttributeArray(0, GL_FLOAT, vertices,     2, 4 * sizeof(float));
    m_program->setAttributeArray(1, GL_FLOAT, vertices + 2, 2, 4 * sizeof(float));

    f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);
    m_program->release();
    f->glDisable(GL_BLEND);

    m_fbo->release();

    // ---- Read back FBO as QImage (reuse buffer to avoid per-frame allocation) ----
    const int fbSz = m_fbo->width();
    if (m_renderedFrame.width() != fbSz || m_renderedFrame.height() != fbSz
        || m_renderedFrame.format() != QImage::Format_RGBA8888) {
        m_renderedFrame = QImage(fbSz, fbSz, QImage::Format_RGBA8888);
    }
    m_fbo->bind();
    f->glReadPixels(0, 0, fbSz, fbSz, GL_RGBA, GL_UNSIGNED_BYTE, m_renderedFrame.bits());
    m_fbo->release();

    // H-07: Context stays current — no doneCurrent() per frame
    emit frameReady();
}
