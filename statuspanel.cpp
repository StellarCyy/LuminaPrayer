#include "statuspanel.h"
#include "statusmanager.h"
#include "fontutils.h"
#include <QPainter>
#include <QPen>
#include <QLinearGradient>
#include <QPainterPath>
#include <QMouseEvent>

StatusPanel::StatusPanel(StatusManager *mgr, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_statusMgr(mgr),
      m_opacity(0.0),
      m_fadingOut(false),
      m_fadeTimer(new QTimer(this)),
      m_fadeTarget(0.0),
      m_fadeStep(0.0),
      m_autoHideTimer(new QTimer(this)),
      m_titleFont(resolveCJKSansFont(16, QFont::Bold)),
      m_barFont(resolveCJKSansFont(12))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(PanelW, PanelH);

    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, &StatusPanel::fadeOut);

    m_fadeTimer->setInterval(FadeIntervalMs);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        m_opacity += m_fadeStep;
        if ((m_fadeStep > 0 && m_opacity >= m_fadeTarget) ||
            (m_fadeStep < 0 && m_opacity <= m_fadeTarget)) {
            m_opacity = m_fadeTarget;
            m_fadeTimer->stop();
            if (m_fadingOut) {
                hide();
                m_fadingOut = false;
            }
        }
        setWindowOpacity(m_opacity);
        update();
    });
}

void StatusPanel::showAt(const QPoint &anchorTopRight, const QSize &screenSize) {
    // Position to the right of the character; if it overflows, place to the left
    const int margin = 8;
    int x = anchorTopRight.x() + margin;
    int y = anchorTopRight.y();

    if (x + PanelW > screenSize.width()) {
        x = anchorTopRight.x() - PanelW - margin;
    }
    if (y + PanelH > screenSize.height()) {
        y = screenSize.height() - PanelH - margin;
    }
    if (y < 0) y = 0;

    QWidget::move(x, y);
}

void StatusPanel::showForDuration(const QPoint &anchorTopRight, const QSize &screenSize, int durationMs) {
    showAt(anchorTopRight, screenSize);
    fadeIn();
    m_autoHideTimer->start(durationMs);
}

void StatusPanel::fadeIn() {
    m_fadingOut = false;
    m_fadeTarget = 1.0;
    const int steps = FadeDurationMs / FadeIntervalMs;
    m_fadeStep = (m_fadeTarget - m_opacity) / std::max(1, steps);
    if (m_fadeStep <= 0.0) m_fadeStep = 0.02;
    setWindowOpacity(m_opacity);
    show();
    raise();
    m_fadeTimer->start();
}

void StatusPanel::fadeOut() {
    m_autoHideTimer->stop();
    if (m_fadingOut) return;
    m_fadingOut = true;
    m_fadeTarget = 0.0;
    const int steps = FadeDurationMs / FadeIntervalMs;
    m_fadeStep = (m_fadeTarget - m_opacity) / std::max(1, steps);
    if (m_fadeStep >= 0.0) m_fadeStep = -0.02;
    m_fadeTimer->start();
}

void StatusPanel::setPanelOpacity(double v) {
    m_opacity = v;
    setWindowOpacity(v);
    update();
}

void StatusPanel::mousePressEvent(QMouseEvent *) {
    fadeOut();
}

void StatusPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF panelRect(0.5, 0.5, width() - 1.0, height() - 1.0);
    const double radius = 14.0;

    // --- Background: pure white ---
    QPainterPath bgPath;
    bgPath.addRoundedRect(panelRect, radius, radius);
    p.fillPath(bgPath, QColor(255, 255, 255, 245));

    // --- Golden silk border (double-line style) ---
    const QColor gold(218, 165, 32);
    const QColor goldLight(255, 215, 100, 180);

    // Outer border
    QPen outerPen(gold, 2.5);
    p.setPen(outerPen);
    p.drawRoundedRect(panelRect.adjusted(1, 1, -1, -1), radius, radius);

    // Inner border (thin, lighter gold)
    QPen innerPen(goldLight, 0.8);
    p.setPen(innerPen);
    p.drawRoundedRect(panelRect.adjusted(5, 5, -5, -5), radius - 3, radius - 3);

    // --- Geometric accent lines (blue-white, subtle) ---
    QPen accentPen(QColor(100, 160, 255, 60), 0.6);
    p.setPen(accentPen);
    // Horizontal lines
    for (int y = 30; y < height() - 30; y += 52) {
        p.drawLine(20, y, width() - 20, y);
    }

    // --- Title ---
    p.setPen(QColor(80, 80, 80));
    p.setFont(m_titleFont);
    p.drawText(QRectF(20, 12, width() - 40, 30), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("状态"));

    // --- Draw status bars ---
    if (!m_statusMgr) return;

    static const QString labels[StatusManager::StatusCount] = {
        //快乐 (Happiness) 兴致 (Interest) 理智 (Sanity) 饱食 (Satiety) 亲密 (Affection)
        QStringLiteral("快乐"),
        QStringLiteral("兴致"),
        QStringLiteral("理智"),
        QStringLiteral("饱食"),
        QStringLiteral("亲密"),
    };

    p.setFont(m_barFont);

    const int barLeft = 22;
    const int barRight = width() - 22;
    const int barH = 14;
    const int startY = 52;
    const int rowH = 46;

    for (int i = 0; i < StatusManager::StatusCount; ++i) {
        const int y = startY + i * rowH;
        const int val = m_statusMgr->value(static_cast<StatusManager::StatusKind>(i));
        const double frac = val / 100.0;

        // Label (height 22 for CJK baseline clearance)
        p.setPen(QColor(100, 100, 100));
        p.drawText(QRectF(barLeft, y, 200, 22), Qt::AlignLeft | Qt::AlignVCenter, labels[i]);

        // Value text
        p.setPen(QColor(60, 60, 60));
        p.drawText(QRectF(barRight - 40, y, 40, 22), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(val));

        // Bar background
        const QRectF barBg(barLeft, y + 20, barRight - barLeft, barH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(230, 230, 230));
        p.drawRoundedRect(barBg, barH / 2.0, barH / 2.0);

        // Bar fill with gradient
        if (frac > 0.0) {
            const QRectF barFill(barLeft, y + 20, (barRight - barLeft) * frac, barH);
            QLinearGradient grad(barFill.topLeft(), barFill.topRight());
            grad.setColorAt(0.0, QColor(100, 180, 255));
            grad.setColorAt(1.0, gold);
            p.setBrush(grad);
            p.drawRoundedRect(barFill, barH / 2.0, barH / 2.0);
        }
    }
}
