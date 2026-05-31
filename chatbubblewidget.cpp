#include "chatbubblewidget.h"
#include "profilemanager.h"
#include "fontutils.h"
#include <QPainter>
#include <QLabel>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QGuiApplication>

// ─── Construction ───────────────────────────────────────────

ChatBubbleWidget::ChatBubbleWidget(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_loading(false)
    , m_bubbleOpacity(0.0)
    , m_textOpacity(0.0)
    , m_fadeTimer(new QTimer(this))
    , m_fadeTarget(0.0)
    , m_fadeStep(0.0)
    , m_fadeMode(FadeBubble)
    , m_autoCloseTimer(new QTimer(this))
    , m_textLabel(nullptr)
    , m_glowEffect(nullptr)
    , m_bubbleReady(false)
    , m_pendingTextFade(false)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    m_bgPixmap.load(ProfileManager::instance()->sprites().chatbox_bg);
    const int imgW = m_bgPixmap.isNull() ? 500 : m_bgPixmap.width();
    const int imgH = m_bgPixmap.isNull() ? 300 : m_bgPixmap.height();
    setFixedSize(imgW, imgH);

    // ── Dynamic content safe rect ──
    recomputeContentRect();

    // ── Scroll area wrapping text content ──
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setGeometry(m_contentRect);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAttribute(Qt::WA_TranslucentBackground);
    m_scrollArea->viewport()->setAutoFillBackground(false);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,236,100,120); border-radius: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    ));

    m_textLabel = new QLabel();
    m_textLabel->setAlignment(Qt::AlignCenter);
    m_textLabel->setWordWrap(true);
    m_textLabel->setAttribute(Qt::WA_TranslucentBackground);
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setStyleSheet(QStringLiteral("padding: 0px 10px;"));
    m_scrollArea->setWidget(m_textLabel);

    // Forward mouse clicks from scroll content to dismiss
    m_scrollArea->viewport()->installEventFilter(this);
    m_textLabel->installEventFilter(this);

    // Font: CJK Serif (shared resolution, then set to max size for fitting)
    m_baseFont = resolveCJKSerifFont(FontSizeMax, QFont::DemiBold);
    m_textLabel->setFont(m_baseFont);

    // Celestial gold glow via QGraphicsDropShadowEffect
    m_glowEffect = new QGraphicsDropShadowEffect(m_textLabel);
    m_glowEffect->setBlurRadius(8.0);
    m_glowEffect->setColor(QColor(255, 236, 100, 210)); // bright gold glow
    m_glowEffect->setOffset(0, 0);
    m_textLabel->setGraphicsEffect(m_glowEffect);

    // Start hidden — shown only after bubble fully fades in
    m_scrollArea->hide();

    // ── Fade animation tick ──
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        switch (m_fadeMode) {
        case FadeBubble:
            m_bubbleOpacity += m_fadeStep;
            if ((m_fadeStep > 0 && m_bubbleOpacity >= m_fadeTarget) ||
                (m_fadeStep < 0 && m_bubbleOpacity <= m_fadeTarget)) {
                m_bubbleOpacity = m_fadeTarget;
                m_fadeTimer->stop();
                onBubbleFadeComplete();
            }
            break;
        case FadeText:
            m_textOpacity += m_fadeStep;
            if ((m_fadeStep > 0 && m_textOpacity >= m_fadeTarget) ||
                (m_fadeStep < 0 && m_textOpacity <= m_fadeTarget)) {
                m_textOpacity = m_fadeTarget;
                m_fadeTimer->stop();
            }
            updateTextStyle();
            break;
        case FadeOut:
            m_bubbleOpacity += m_fadeStep;
            m_textOpacity   += m_fadeStep;
            if (m_bubbleOpacity <= 0.0) {
                m_bubbleOpacity = 0.0;
                m_textOpacity   = 0.0;
                m_fadeTimer->stop();
                hide();
                return;
            }
            updateTextStyle();
            break;
        }
        update();
    });

    m_autoCloseTimer->setSingleShot(true);
    connect(m_autoCloseTimer, &QTimer::timeout, this, &ChatBubbleWidget::fadeOutAndHide);
}

// ─── Public API ─────────────────────────────────────────────

void ChatBubbleWidget::showAt(const QPoint &anchorTopCenter) {
    // ── Reset all state for reuse ──
    m_fadeTimer->stop();
    m_autoCloseTimer->stop();
    m_bubbleOpacity   = 0.0;
    m_textOpacity     = 0.0;
    m_bubbleReady     = false;
    m_pendingTextFade = false;
    m_pendingText.clear();
    m_displayText.clear();
    m_loading = false;
    m_textLabel->clear();
    m_scrollArea->hide();
    updateTextStyle();

    // ── Position ──
    const int x = anchorTopCenter.x() - width() / 2;
    const int y = anchorTopCenter.y() - height() - 10;

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    move(qBound(avail.left(), x, avail.right()  - width()),
         qBound(avail.top(),  y, avail.bottom() - height()));

    show();
    raise();

    startFade(FadeBubble, 0.0, 1.0, 600);
}

void ChatBubbleWidget::setLoading() {
    m_loading     = true;
    m_displayText = QStringLiteral("正在回答......");

    if (m_bubbleReady) {
        m_textOpacity = 1.0;
        m_textLabel->setText(m_displayText);
        fitFontToContent();
        updateTextStyle();
        m_scrollArea->show();
    } else {
        m_textOpacity = 0.0;
        m_pendingTextFade = false;
        m_pendingText.clear();
    }
}

void ChatBubbleWidget::setResponseText(const QString &text) {
    m_loading     = false;
    m_displayText = text;

    if (m_bubbleReady) {
        m_textLabel->setText(m_displayText);
        fitFontToContent();
        m_textOpacity = 0.0;
        updateTextStyle();
        m_scrollArea->show();
        startFade(FadeText, 0.0, 1.0, 800);
        m_autoCloseTimer->start(m_autoCloseMs);
    } else {
        m_pendingTextFade = true;
        m_pendingText     = text;
        m_textOpacity     = 0.0;
    }
}

void ChatBubbleWidget::fadeOutAndHide() {
    m_autoCloseTimer->stop();
    startFade(FadeOut, m_bubbleOpacity, 0.0, 500);
}

void ChatBubbleWidget::setAutoCloseMs(int ms) {
    m_autoCloseMs = qMax(1000, ms);
}

void ChatBubbleWidget::setContentPadding(int px) {
    m_contentPadding = qBound(10, px, 120);
    recomputeContentRect();
}

void ChatBubbleWidget::recomputeContentRect() {
    const int w = width();
    const int h = height();
    m_contentRect = QRect(m_contentPadding, m_contentPadding,
                          w - m_contentPadding * 2,
                          h - m_contentPadding * 2);
    if (m_scrollArea) m_scrollArea->setGeometry(m_contentRect);
}

void ChatBubbleWidget::setBubbleOpacity(double v) {
    m_bubbleOpacity = v;
    update();
}

void ChatBubbleWidget::setTextOpacity(double v) {
    m_textOpacity = v;
    updateTextStyle();
    update();
}

// ─── Internal ───────────────────────────────────────────────

void ChatBubbleWidget::startFade(FadeTarget mode, double from, double to, int durationMs) {
    m_fadeMode   = mode;
    m_fadeTarget = to;
    const int steps = durationMs / FadeIntervalMs;
    m_fadeStep = (steps > 0) ? (to - from) / steps : (to - from);

    switch (mode) {
    case FadeBubble: m_bubbleOpacity = from; break;
    case FadeText:   m_textOpacity   = from; break;
    case FadeOut:    break;
    }
    m_fadeTimer->start(FadeIntervalMs);
}

void ChatBubbleWidget::onBubbleFadeComplete() {
    m_bubbleReady = true;

    if (m_pendingTextFade) {
        m_pendingTextFade = false;
        m_displayText     = m_pendingText;
        m_pendingText.clear();
        m_loading = false;
        m_textLabel->setText(m_displayText);
        fitFontToContent();
        m_textOpacity = 0.0;
        updateTextStyle();
        m_scrollArea->show();
        startFade(FadeText, 0.0, 1.0, 800);
        m_autoCloseTimer->start(m_autoCloseMs);
    } else if (m_loading) {
        m_textOpacity = 1.0;
        m_textLabel->setText(m_displayText);
        fitFontToContent();
        updateTextStyle();
        m_scrollArea->show();
    }
}

void ChatBubbleWidget::updateTextStyle() {
    if (!m_textLabel) return;

    // Effective opacity: during FadeOut both bubble and text fade together
    const double eff = qBound(0.0, m_textOpacity, 1.0);
    const int alpha = int(255 * eff);

    // Text color: #FFD700 with fading alpha
    QPalette pal = m_textLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(255, 236, 100, alpha));
    m_textLabel->setPalette(pal);

    // Glow effect: modulate the glow alpha with text opacity
    if (m_glowEffect) {
        m_glowEffect->setColor(QColor(255, 236, 100, int(210 * eff)));
    }

    // Hide label entirely when invisible
    if (alpha <= 0) {
        m_scrollArea->hide();
    }
}

// ─── Font fitting ───────────────────────────────────────────

void ChatBubbleWidget::fitFontToContent() {
    if (!m_textLabel || !m_scrollArea || m_displayText.isEmpty()) return;

    const int internalPad = 10;
    const int fullW  = m_contentRect.width();
    const int availW = fullW - internalPad * 2;
    const int availH = m_contentRect.height() - internalPad * 2;
    if (availW <= 0 || availH <= 0) return;

    QFont font = m_baseFont;
    bool fits = false;

    for (int px = FontSizeMax; px >= FontSizeMin; --px) {
        font.setPixelSize(px);
        QFontMetrics fm(font);
        QRect br = fm.boundingRect(0, 0, availW, 10000,
                                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                   m_displayText);
        if (br.height() <= availH) {
            fits = true;
            break;
        }
    }

    if (!fits) font.setPixelSize(FontSizeMin);
    m_textLabel->setFont(font);

    if (fits) {
        // Text fits — no scrolling needed
        m_textLabel->setAlignment(Qt::AlignCenter);
        m_textLabel->setFixedSize(fullW, m_contentRect.height());
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        // Text overflows at min font — enable mouse-wheel scrolling
        m_textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        QFontMetrics fm(font);
        QRect br = fm.boundingRect(0, 0, availW, 100000,
                                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                   m_displayText);
        const int labelH = br.height() + internalPad * 2;
        m_textLabel->setFixedSize(fullW, qMax(m_contentRect.height(), labelH));
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_scrollArea->verticalScrollBar()->setValue(0);
    }
}

// ─── Event filter (forward clicks from scroll area to dismiss) ───

bool ChatBubbleWidget::eventFilter(QObject *watched, QEvent *event) {
    if ((watched == m_scrollArea->viewport() || watched == m_textLabel) &&
        event->type() == QEvent::MouseButtonPress) {
        fadeOutAndHide();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

// ─── Paint (background only) ────────────────────────────────

void ChatBubbleWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!m_bgPixmap.isNull() && m_bubbleOpacity > 0.0) {
        p.setOpacity(m_bubbleOpacity);
        p.drawPixmap(0, 0, m_bgPixmap);
    }
}

// ─── Input ──────────────────────────────────────────────────

void ChatBubbleWidget::mousePressEvent(QMouseEvent *) {
    fadeOutAndHide();
}
