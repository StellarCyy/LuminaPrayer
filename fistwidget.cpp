#include "fistwidget.h"
#include "profilemanager.h"
#include <QPainter>
#include <QPaintEvent>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <QCursor>
#include <cmath>
#include <algorithm>

// ==========================================
// StaticHalo: 原地淡出的光环 (替身)
// ==========================================
StaticHalo::StaticHalo(QRect rect, QPixmap pix, int size, QWidget *parent)
    : QWidget(parent), m_lightPix(pix), m_lightSize(size) {
    setGeometry(rect);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    show();
    startFadeOut();
}

void StaticHalo::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_lightSize > 0 && !m_lightPix.isNull()) {
        int lx = (width()  - m_lightSize) / 2;
        int ly = (height() - m_lightSize) / 2;
        painter.drawPixmap(QRect(lx, ly, m_lightSize, m_lightSize), m_lightPix);
    }
}

void StaticHalo::startFadeOut() {
    QPropertyAnimation *animFade = new QPropertyAnimation(this, "windowOpacity");
    animFade->setDuration(ProfileManager::instance()->animation().static_halo_fade_duration_ms);
    animFade->setStartValue(1.0);
    animFade->setEndValue(0.0);
    connect(animFade, &QPropertyAnimation::finished, this, &QWidget::close);
    animFade->start();
}

// ==========================================
// FistWidget 实现
// ==========================================
FistWidget::FistWidget(QPoint startPos, bool isLeftSpawn, QWidget *parent)
    : QWidget(parent),
    m_lightSize(0),
    m_showFist(false),
    m_currentSpeed(0.0),
    m_isLeftSpawn(isLeftSpawn)
{
    const auto *pm = ProfileManager::instance();
    const int fistWinSize = pm->window().fist_widget_size;
    const int fistWinHalf = fistWinSize / 2;

    setFixedSize(fistWinSize, fistWinSize);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    m_fistPix.load(pm->sprites().fist);
    // MED-12: Pre-scale fist pixmap once (SmoothTransformation is expensive)
    const int fistSpriteSize = pm->window().fist_sprite_size;
    if (!m_fistPix.isNull()) {
        m_fistScaled = m_fistPix.scaled(fistSpriteSize, fistSpriteSize,
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_lightPix.load(pm->sprites().light_symbol);

    move(startPos.x() - fistWinHalf, startPos.y() - fistWinHalf);
    m_currentMousePos = QCursor::pos();

    show();
    startAttackSequence();
}

void FistWidget::startAttackSequence() {
    const auto *pm = ProfileManager::instance();
    const auto &animCfg = pm->animation();
    const int fistWinHalf = pm->window().fist_widget_size / 2;

    QVariantAnimation *animHalo = new QVariantAnimation(this);
    animHalo->setStartValue(0);
    animHalo->setEndValue(animCfg.fist_halo_end);
    animHalo->setDuration(animCfg.fist_halo_duration_ms);
    animHalo->setEasingCurve(QEasingCurve::OutBack);

    connect(animHalo, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        m_lightSize = val.toInt();
        update();
    });

    connect(animHalo, &QVariantAnimation::finished, this, [this, fistWinHalf](){
        new StaticHalo(geometry(), m_lightPix, m_lightSize, this);
        m_lightSize = 0;
        m_showFist = true;
        update();

        QPoint startP  = pos();
        QPoint targetP = QCursor::pos() - QPoint(fistWinHalf, fistWinHalf);
        int dx = targetP.x() - startP.x();
        int dy = targetP.y() - startP.y();
        const double pullback = ProfileManager::instance()->animation().fist_pullback_factor;
        move(startP.x() - dx * pullback, startP.y() - dy * pullback);

        QTimer::singleShot(ProfileManager::instance()->animation().fist_track_start_delay_ms, this, [this](){
            const int halfW = ProfileManager::instance()->window().fist_widget_size / 2;

            m_trackTimer = new QTimer(this);
            connect(m_trackTimer, &QTimer::timeout, this, [this, halfW](){
                const auto &ac = ProfileManager::instance()->animation();
                m_currentMousePos = QCursor::pos();
                QPoint currentPos = pos();
                QPoint targetPos  = m_currentMousePos - QPoint(halfW, halfW);

                int dx = targetPos.x() - currentPos.x();
                int dy = targetPos.y() - currentPos.y();
                double distance = std::sqrt(dx*dx + dy*dy);

                if (distance < ac.fist_snap_distance || m_currentSpeed > distance) {
                    move(targetPos);
                    m_trackTimer->stop();
                    // H-06: Self-managed animation — DeleteWhenStopped ensures cleanup
                    // even if 'this' is destroyed before animation finishes
                    auto *animFade = new QPropertyAnimation(this, "opacity");
                    animFade->setDuration(ac.fist_fade_duration_ms);
                    animFade->setStartValue(1.0);
                    animFade->setEndValue(0.0);
                    connect(animFade, &QPropertyAnimation::finished, this, &QWidget::close);
                    animFade->start(QAbstractAnimation::DeleteWhenStopped);
                    return;
                }

                m_currentSpeed += ac.fist_acceleration;
                double ratio = std::min(m_currentSpeed, distance) / distance;

                int moveX = static_cast<int>(dx * ratio);
                int moveY = static_cast<int>(dy * ratio);
                move(currentPos.x() + moveX, currentPos.y() + moveY);
            });
            m_trackTimer->start(ProfileManager::instance()->animation().fist_track_interval_ms);
        });
    });
    // H-06: Self-managed animation lifecycle
    animHalo->start(QAbstractAnimation::DeleteWhenStopped);
}

void FistWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_lightSize > 0 && !m_lightPix.isNull()) {
        int lx = (width()  - m_lightSize) / 2;
        int ly = (height() - m_lightSize) / 2;
        painter.drawPixmap(QRect(lx, ly, m_lightSize, m_lightSize), m_lightPix);
    }

    if (m_showFist && !m_fistScaled.isNull()) {
        int cx = width()  / 2;
        int cy = height() / 2;

        QPoint globalCenter = mapToGlobal(QPoint(cx, cy));
        double dx = m_currentMousePos.x() - globalCenter.x();
        double dy = m_currentMousePos.y() - globalCenter.y();
        double angle = std::atan2(dy, dx) * 180.0 / 3.1415926535;

        painter.save();
        painter.translate(cx, cy);
        painter.rotate(angle);
        painter.translate(-m_fistScaled.width() / 2.0, -m_fistScaled.height() / 2.0);
        painter.drawPixmap(0, 0, m_fistScaled);
        painter.restore();
    }
}
