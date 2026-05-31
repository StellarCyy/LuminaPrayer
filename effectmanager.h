#ifndef EFFECTMANAGER_H
#define EFFECTMANAGER_H

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

class EffectManager : public QObject
{
    Q_OBJECT
public:
    explicit EffectManager(QObject *parent = nullptr);
    ~EffectManager() override;

    void setHaloTexture(const QPixmap &pixmap);
    void setCpuUsage(float usage);
    void setEffectActive(bool active);
    bool isEffectActive() const { return m_active; }

    const QImage& currentFrame() const { return m_renderedFrame; }
    int frameSize() const;

signals:
    void frameReady();

private:
    void initGL();
    void cleanupGL();
    void renderFrame();

    std::unique_ptr<QOffscreenSurface>        m_surface;
    std::unique_ptr<QOpenGLContext>            m_context;
    std::unique_ptr<QOpenGLFramebufferObject>  m_fbo;
    std::unique_ptr<QOpenGLShaderProgram>      m_program;
    unsigned int              m_textureId = 0;      // raw GL texture (requires GL deletion)

    QPixmap  m_pendingPixmap;
    bool     m_needsTextureUpload = false;
    bool     m_glInitialized      = false;
    bool     m_contextCurrent     = false;
    bool     m_active             = false;

    QImage        m_renderedFrame;
    QElapsedTimer m_elapsed;
    QTimer       *m_renderTimer = nullptr;
    float         m_cpuUsage    = 0.0f;
};

#endif // EFFECTMANAGER_H
