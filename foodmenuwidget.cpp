#include "foodmenuwidget.h"
#include "profilemanager.h"
#include "fontutils.h"

#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QFontInfo>

// ═══════════════════════════════════════════════════════════
//  FoodGridWidget — inner scrollable canvas with flow layout
// ═══════════════════════════════════════════════════════════

FoodGridWidget::FoodGridWidget(const QVector<FoodItem> &foods, QWidget *parent)
    : QWidget(parent)
    , m_foods(foods)
{
    // ── Golden serif font (shared CJK resolution) ──
    m_serifFont = resolveCJKSerifFont(12, QFont::DemiBold);

    m_nameFont = m_serifFont;
    m_nameFont.setPixelSize(14);
    m_nameFont.setWeight(QFont::Bold);

    setMouseTracking(true);

    // Flash animation: 6 steps (~100ms total), toggles golden overlay
    m_flashTimer.setInterval(50);
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        ++m_flashStep;
        if (m_flashStep >= 6) {
            m_flashTimer.stop();
            m_flashIndex = -1;
            m_flashStep = 0;
        }
        update();
    });
}

QRect FoodGridWidget::cardRect(int index) const {
    const int col = index % m_cols;
    const int row = index / m_cols;
    const int x = m_marginLeft + col * (CardW + GridSpacing);
    const int y = PadX + row * (CardH + GridSpacing);
    return QRect(x, y, CardW, CardH);
}

void FoodGridWidget::reflowCards(int viewportWidth) {
    // Compute how many columns fit (like Windows Explorer icon layout)
    m_cols = qMax(1, (viewportWidth - PadX * 2 + GridSpacing) / (CardW + GridSpacing));

    // Center the grid horizontally
    const int totalGridW = m_cols * CardW + (m_cols - 1) * GridSpacing;
    m_marginLeft = (viewportWidth - totalGridW) / 2;
    if (m_marginLeft < PadX) m_marginLeft = PadX;

    // Compute total height
    const int rows = (m_foods.size() + m_cols - 1) / m_cols;
    const int totalH = PadX + rows * (CardH + GridSpacing) + PadX;
    setFixedHeight(totalH);

    update();
}

void FoodGridWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    for (int i = 0; i < m_foods.size(); ++i) {
        const QRect cr = cardRect(i);

        // Card background
        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(cr), 10, 10);
        p.fillPath(cardPath, QColor(250, 248, 242));

        // Card border — gold tint
        p.setPen(QPen(QColor(218, 165, 32, 80), 1.0));
        p.drawRoundedRect(QRectF(cr), 10, 10);

        // ── Food image (200×200, centered in card) ──
        const auto &food = m_foods[i];
        if (!food.pixmap.isNull()) {
            const int imgX = cr.x() + (CardW - food.pixmap.width()) / 2;
            const int imgY = cr.y() + 6;
            p.drawPixmap(imgX, imgY, food.pixmap);
        }

        // ── Food name (dark serif, centered) ──
        p.setPen(QColor(80, 60, 20));
        p.setFont(m_nameFont);
        QRectF nameRect(cr.x() + 4, cr.y() + ImgSize + 10, CardW - 8, 22);
        p.drawText(nameRect, Qt::AlignCenter, food.name);

        // ── Benefit description (golden serif) ──
        p.setPen(QColor(218, 165, 32, 210));
        p.setFont(m_serifFont);
        QRectF descRect(cr.x() + 4, cr.y() + ImgSize + 34, CardW - 8, 36);
        p.drawText(descRect, Qt::AlignCenter | Qt::TextWordWrap, food.description);

        // ── Flash overlay (golden pulse on selected card) ──
        if (i == m_flashIndex && m_flashStep > 0) {
            const int alpha = (m_flashStep % 2 == 1) ? 80 : 0;
            if (alpha > 0) {
                QPainterPath flashPath;
                flashPath.addRoundedRect(QRectF(cr), 10, 10);
                p.fillPath(flashPath, QColor(218, 165, 32, alpha));
            }
        }
    }
}

void FoodGridWidget::mousePressEvent(QMouseEvent *event) {
    const QPoint pos = event->pos();
    for (int i = 0; i < m_foods.size(); ++i) {
        if (cardRect(i).contains(pos)) {
            flashCard(i);
            emit foodClicked(i);
            return;
        }
    }
}

void FoodGridWidget::flashCard(int index) {
    m_flashIndex = index;
    m_flashStep = 0;
    m_flashTimer.start();
}

// ═══════════════════════════════════════════════════════════
//  FoodMenuWidget — top-level window
// ═══════════════════════════════════════════════════════════

FoodMenuWidget::FoodMenuWidget(QWidget *parent)
    : QWidget(parent, Qt::Window)          // standard title bar: minimize, maximize, close
{
    setWindowTitle(QStringLiteral("✦ 投喂菜单 ✦"));
    setMinimumSize(300, 300);
    resize(760, 540);

    // ── Food data ──
    buildFoodList();

    // ── Scroll area + grid widget ──
    m_gridWidget = new FoodGridWidget(m_foods, this);
    m_gridWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setWidget(m_gridWidget);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Scroll area background
    m_scrollArea->setStyleSheet(
        QStringLiteral("QScrollArea { background: rgb(255,255,253); }"
                       "QScrollBar:vertical {"
                       "  background: rgb(245,243,237); width: 10px; border: none; }"
                       "QScrollBar::handle:vertical {"
                       "  background: rgba(218,165,32,120); border-radius: 5px; min-height: 30px; }"
                       "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scrollArea);

    // Forward selection signal
    connect(m_gridWidget, &FoodGridWidget::foodClicked, this, &FoodMenuWidget::foodSelected);

}

void FoodMenuWidget::buildFoodList() {
    const FoodMenuProfile &fmp = ProfileManager::instance()->foodMenu();
    const int count = fmp.items.size();
    m_foods.resize(count);
    for (int i = 0; i < count; ++i) {
        const FoodItemDef &def = fmp.items[i];
        auto &f = m_foods[i];
        f.name        = def.name;
        f.description = def.description;
        // Per-item image override, or pattern-based path
        f.imagePath   = def.imagePath.isEmpty()
                            ? fmp.image_pattern.arg(i)
                            : def.imagePath;
        if (f.pixmap.load(f.imagePath)) {
            // Pre-scale to card display size (once at load, not per-frame in paintEvent)
            f.pixmap = f.pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        for (int k = 0; k < 5; ++k)
            f.effects[k] = def.effects[k];
    }
}

void FoodMenuWidget::showCentered() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    move(avail.center() - QPoint(width() / 2, height() / 2));
    show();
    raise();
    activateWindow();
}

void FoodMenuWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Reflow after layout is settled — viewport now has its real width
    if (m_gridWidget && m_scrollArea) {
        m_gridWidget->reflowCards(m_scrollArea->viewport()->width());
    }
}

void FoodMenuWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_gridWidget && m_scrollArea) {
        m_gridWidget->reflowCards(m_scrollArea->viewport()->width());
    }
}
