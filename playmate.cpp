#include "playmate.h"
#include "profilemanager.h"

#include <QPaintEvent>
#include <QPainter>
#include <QVariantAnimation>
#include <algorithm>

Playmate::Playmate(const QList<QString> &movePaths, QWidget *parent)
    : QWidget(parent),
    frame_count(0),
    frame_index(0),
    move_face_right(false),
    frame_timer(new QTimer(this)),
    current_light_size(0),
    show_light(false),
    current_opacity(1.0)
{
    const auto *pm = ProfileManager::instance();
    setFixedSize(pm->window().widget_size, pm->window().widget_size);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    light_pix.load(pm->sprites().light_symbol);

    frame_timer->setInterval(pm->animation().frame_interval_ms);
    connect(frame_timer, &QTimer::timeout, this, &Playmate::advanceMoveFrame);

    setMovePaths(movePaths);
    frame_timer->start();
}

void Playmate::setMovePaths(const QList<QString> &movePaths) {
    move_paths = movePaths;
    frame_index = 0;
    if (move_paths.size() >= 2) {
        frame_count = move_paths.size() / 2;
    } else {
        frame_count = move_paths.size();
    }

    // M-05: Pre-load all frames into cache to avoid per-frame disk I/O
    m_frameCache.resize(move_paths.size());
    for (int i = 0; i < move_paths.size(); ++i) {
        m_frameCache[i].load(move_paths[i]);
    }

    advanceMoveFrame();
}

void Playmate::setFacingRight(bool facingRight) {
    move_face_right = facingRight;
}

void Playmate::playEntryAnimation() {
    const auto &anim = ProfileManager::instance()->animation();
    show_light = true;
    current_light_size = anim.light_intro_start;
    current_opacity = 1.0;

    QVariantAnimation *animSize = new QVariantAnimation(this);
    animSize->setStartValue(anim.light_intro_start);
    animSize->setEndValue(anim.light_intro_end);
    animSize->setDuration(anim.light_intro_size_duration_ms);
    animSize->setEasingCurve(QEasingCurve::OutQuad);
    connect(animSize, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        current_light_size = val.toInt();
        update();
    });

    QVariantAnimation *animFade = new QVariantAnimation(this);
    animFade->setStartValue(anim.light_intro_fade_start_opacity);
    animFade->setEndValue(0.0);
    animFade->setDuration(anim.light_intro_fade_duration_ms);
    animFade->setEasingCurve(QEasingCurve::Linear);
    connect(animFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        current_opacity = val.toDouble();
        update();
    });

    connect(animSize, &QVariantAnimation::finished, this, [animFade, animSize](){
        animFade->start();
        animSize->deleteLater();
    });

    connect(animFade, &QVariantAnimation::finished, this, [this, animFade](){
        show_light = false;
        current_opacity = 1.0;
        update();
        animFade->deleteLater();
    });

    animSize->start();
}

void Playmate::playExitAnimation() {
    const auto &anim = ProfileManager::instance()->animation();
    show_light = true;
    current_light_size = anim.exit_light_start;
    current_opacity = 0.0;

    QVariantAnimation *animFadeIn = new QVariantAnimation(this);
    animFadeIn->setStartValue(0.0);
    animFadeIn->setEndValue(1.0);
    animFadeIn->setDuration(anim.exit_fade_in_duration_ms);
    animFadeIn->setEasingCurve(QEasingCurve::InQuad);
    connect(animFadeIn, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        current_opacity = val.toDouble();
        update();
    });

    QVariantAnimation *animShrink = new QVariantAnimation(this);
    animShrink->setStartValue(anim.exit_light_start);
    animShrink->setEndValue(0);
    animShrink->setDuration(anim.exit_shrink_duration_ms);
    animShrink->setEasingCurve(QEasingCurve::InBack);
    connect(animShrink, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        current_light_size = val.toInt();
        if (current_light_size < ProfileManager::instance()->animation().exit_hide_character_threshold) {
            current_pix = QPixmap();
        }
        update();
    });

    connect(animFadeIn, &QVariantAnimation::finished, this, [animFadeIn, animShrink](){
        animShrink->start();
        animFadeIn->deleteLater();
    });

    connect(animShrink, &QVariantAnimation::finished, this, [this, animShrink](){
        close();
        deleteLater();
        animShrink->deleteLater();
    });

    animFadeIn->start();
}

void Playmate::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!current_pix.isNull()) {
        const int cx = (width() - current_pix.width()) / 2;
        const int cy = (height() - current_pix.height()) / 2;
        painter.drawPixmap(cx, cy, current_pix);
    }

    if (show_light && !light_pix.isNull()) {
        painter.save();
        painter.setOpacity(current_opacity);
        const int lx = (width() - current_light_size) / 2;
        const int ly = (height() - current_light_size) / 2;
        painter.drawPixmap(QRect(lx, ly, current_light_size, current_light_size), light_pix);
        painter.restore();
    }
}

void Playmate::advanceMoveFrame() {
    if (m_frameCache.isEmpty()) {
        current_pix = QPixmap();
        update();
        return;
    }

    if (frame_count <= 0) {
        frame_count = m_frameCache.size();
    }

    // M-05: Index pre-loaded cache instead of QPixmap::load() per frame
    if (m_frameCache.size() >= 2 && frame_count > 0) {
        const int localIndex = frame_index++ % frame_count;
        const int base = move_face_right ? frame_count : 0;
        const int maxIndex = static_cast<int>(m_frameCache.size()) - 1;
        const int idx = std::min(base + localIndex, maxIndex);
        current_pix = m_frameCache[idx];
    } else {
        const int idx = frame_index++ % m_frameCache.size();
        current_pix = m_frameCache[idx];
    }

    update();
}
