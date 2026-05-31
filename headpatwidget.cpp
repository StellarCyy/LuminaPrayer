#include "headpatwidget.h"
#include "fistwidget.h"
#include "profilemanager.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QtMath>
#include <QRandomGenerator>
#include <algorithm>

// ==========================================
// Pre-cached unit 4-pointed star (Meyers singleton)
// ==========================================
const QPainterPath& HeadPatWidget::unitStarPath()
{
    static QPainterPath path = []() {
        QPainterPath p;
        constexpr double outer = 1.0;
        constexpr double inner = 0.35;
        p.moveTo(0, -outer);
        for (int i = 1; i < 8; ++i) {
            double r = (i % 2 == 0) ? outer : inner;
            double a = -M_PI / 2.0 + i * M_PI / 4.0;
            p.lineTo(r * qCos(a), r * qSin(a));
        }
        p.closeSubpath();
        return p;
    }();
    return path;
}

// ==========================================
// HeadPatWidget
// ==========================================
HeadPatWidget::HeadPatWidget(QPoint pivotScreen, double radius,
                             double startDeg, double endDeg,
                             QWidget *parent)
    : QWidget(parent)
    , m_pivot(pivotScreen)
    , m_radius(radius)
    , m_startDeg(startDeg)
    , m_endDeg(endDeg)
    , m_angle(startDeg)
    , m_angularVel(0.0)
    , m_swingDown(true)
    , m_patsLeft(patCount)
    , m_inContact(false)
    , m_running(false)
    , m_wSize(0)
{
    m_handPix.load(ProfileManager::instance()->sprites().hand_sprite);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    connect(&m_timer, &QTimer::timeout, this, &HeadPatWidget::onTick);
}

QPointF HeadPatWidget::handLocalPos() const
{
    const double cx = m_wSize / 2.0;
    const double cy = m_wSize / 2.0;
    const double rad = qDegreesToRadians(m_angle);
    const double hx = mirrored ? cx - m_radius * qCos(rad) : cx + m_radius * qCos(rad);
    const double hy = cy - m_radius * qSin(rad);
    return QPointF(hx, hy);
}

QPoint HeadPatWidget::handScreenPos() const
{
    const double rad = qDegreesToRadians(m_angle);
    const double hx = mirrored
        ? m_pivot.x() - m_radius * qCos(rad)
        : m_pivot.x() + m_radius * qCos(rad);
    const double hy = m_pivot.y() - m_radius * qSin(rad);
    return QPoint(static_cast<int>(hx), static_cast<int>(hy));
}

void HeadPatWidget::start()
{
    m_patsLeft  = patCount;
    m_angle     = m_startDeg;
    m_angularVel = 0.0;
    m_swingDown = true;
    m_inContact = false;
    m_running   = true;
    m_ripples.clear();
    m_starBursts.clear();

    const int pw = m_handPix.width();
    const int ph = m_handPix.height();
    m_wSize = static_cast<int>(m_radius * 2 + std::max(pw, ph) + 40);
    setFixedSize(m_wSize, m_wSize);

    const int halfW = m_wSize / 2;
    move(m_pivot.x() - halfW, m_pivot.y() - halfW);

    show();

    // Appear halo at hand's starting position
    QPoint startScreenPos = handScreenPos();
    QPixmap lightPix(ProfileManager::instance()->sprites().light_symbol);
    if (!lightPix.isNull()) {
        const int haloSize = std::max(pw, ph);
        QRect haloRect(startScreenPos.x() - haloSize / 2,
                       startScreenPos.y() - haloSize / 2,
                       haloSize, haloSize);
        new StaticHalo(haloRect, lightPix, haloSize);
    }

    m_timer.start(tickMs);
}

void HeadPatWidget::spawnContactEffects()
{
    const QPointF lp = handLocalPos();
    auto *rng = QRandomGenerator::global();

    // Expanding ripple ring
    RippleEffect ripple;
    ripple.cx = lp.x();
    ripple.cy = lp.y();
    m_ripples.append(ripple);

    // Star burst (random 5-8 particles)
    const int n = 5 + rng->bounded(4);
    StarBurstEffect burst;
    burst.cx = lp.x();
    burst.cy = lp.y();
    burst.stars.resize(n);
    for (int i = 0; i < n; ++i) {
        auto &s = burst.stars[i];
        s.angle = rng->generateDouble() * 2.0 * M_PI;
        s.speed = 30.0 + rng->generateDouble() * 25.0;
        s.size  = 3.0 + rng->generateDouble() * 4.0;
        int hue = 40 + rng->bounded(30);
        s.color = QColor::fromHsv(hue, 200 + rng->bounded(55), 255);
    }
    m_starBursts.append(burst);
}

void HeadPatWidget::advanceEffects()
{
    for (auto &r : m_ripples)    r.elapsedMs += tickMs;
    for (auto &s : m_starBursts) s.elapsedMs += tickMs;

    m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(),
                    [](const RippleEffect &r) { return r.done(); }), m_ripples.end());
    m_starBursts.erase(std::remove_if(m_starBursts.begin(), m_starBursts.end(),
                       [](const StarBurstEffect &s) { return s.done(); }), m_starBursts.end());
}

void HeadPatWidget::drawEffects(QPainter &p)
{
    // Ripples
    for (const auto &r : m_ripples) {
        const double pr = r.progress();
        const double radius = r.maxRadius * pr;
        const double opacity = r.startOpacity * (1.0 - pr);
        QPen pen(QColor(255, 255, 255, static_cast<int>(opacity * 255)));
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(r.cx, r.cy), radius, radius);
    }

    // Star bursts (uses pre-cached unit star path)
    const QPainterPath &star = unitStarPath();
    for (const auto &burst : m_starBursts) {
        const double pr = burst.progress();
        p.setPen(Qt::NoPen);
        for (const auto &s : burst.stars) {
            const double dist = s.speed * pr;
            const double x = burst.cx + dist * qCos(s.angle);
            const double y = burst.cy + dist * qSin(s.angle);
            const double sz = s.size * (1.0 - pr * 0.5);
            QColor c = s.color;
            c.setAlphaF(static_cast<float>(1.0 - pr));
            p.setBrush(c);
            p.save();
            p.translate(x, y);
            p.scale(sz, sz);
            p.drawPath(star);
            p.restore();
        }
    }
}

void HeadPatWidget::onTick()
{
    if (!m_running) {
        // Drain residual effects during fade-out
        if (!m_ripples.isEmpty() || !m_starBursts.isEmpty()) {
            advanceEffects();
            update();
        } else {
            m_timer.stop();  // all effects done — no more ticks needed
        }
        return;
    }

    if (m_swingDown) {
        m_angularVel -= accelRate;
        if (m_angularVel < -maxAngularSpeed)
            m_angularVel = -maxAngularSpeed;

        m_angle += m_angularVel;

        if (m_angle <= m_endDeg) {
            m_angle = m_endDeg;
            m_angularVel = -m_angularVel;
            m_swingDown = false;

            if (!m_inContact) {
                m_inContact = true;
                emit contactStartAt(handScreenPos());
                spawnContactEffects();
            }
        }
    } else {
        m_angularVel -= decelRate;
        if (m_angularVel <= 0.0) {
            m_angularVel = 0.0;

            if (m_inContact) {
                m_inContact = false;
                emit contactEnd();
            }

            --m_patsLeft;
            if (m_patsLeft <= 0) {
                m_running = false;
                // Timer keeps running to drain residual effects
                emit finished();
                beginFadeOut();
                return;
            }
            m_swingDown = true;
        }

        m_angle += m_angularVel;

        if (m_inContact && m_angle > m_endDeg + 2.0) {
            m_inContact = false;
            emit contactEnd();
        }
    }

    advanceEffects();
    update();
}

void HeadPatWidget::beginFadeOut()
{
    auto *fade = new QPropertyAnimation(this, "opacity");
    fade->setDuration(250);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    connect(fade, &QPropertyAnimation::finished, this, &QWidget::close);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void HeadPatWidget::paintEvent(QPaintEvent *)
{
    if (m_handPix.isNull()) return;

    const int pw = m_handPix.width();
    const int ph = m_handPix.height();

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);

    const QPointF hp = handLocalPos();

    p.save();
    p.translate(hp);
    if (mirrored) {
        p.rotate(m_angle);
        p.scale(-1, 1);
    } else {
        p.rotate(-m_angle);
    }
    p.drawPixmap(-pw / 2, -ph / 2, m_handPix);
    p.restore();

    // Draw effects ON TOP of the hand so they're clearly visible
    drawEffects(p);
}
