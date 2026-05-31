#include "gomokuwidget.h"
#include "profilemanager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <cmath>
#include <QtConcurrent>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GomokuWidget::GomokuWidget(bool humanPlaysWhite, QWidget *parent)
    : QWidget(parent),
      m_humanPlaysWhite(humanPlaysWhite),
      m_gameActive(false),
      m_waitingForCharacter(false),
      m_particleActive(false),
      m_winGlowActive(false),
      m_fadeOpacity(1.0),
      m_fadeActive(false),
      m_boardOpacity(0.0)
{

    const auto &gcfg = ProfileManager::instance()->gomoku();
    m_boardGridSize = gcfg.board_size;
    m_cellSize  = gcfg.cell_size;
    m_padding   = gcfg.board_padding;
    m_particleDurationMs = gcfg.particle_duration_ms;

    m_humanColor = humanPlaysWhite ? GomokuEngine::White : GomokuEngine::Black;
    m_aiColor    = humanPlaysWhite ? GomokuEngine::Black : GomokuEngine::White;

    m_engine = new GomokuEngine(m_boardGridSize, this);

    // Window setup — frameless transparent top-level
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    int totalSize = (m_boardGridSize - 1) * m_cellSize + 2 * m_padding;
    setFixedSize(totalSize, totalSize);

    // Center on primary screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        move(sg.center() - QPoint(totalSize / 2, totalSize / 2));
    }

    // Particle timer
    m_particleTimer = new QTimer(this);
    connect(m_particleTimer, &QTimer::timeout, this, &GomokuWidget::onParticleTick);

    // Win glow end timer (single-shot, triggers fade out)
    m_winGlowEndTimer = new QTimer(this);
    m_winGlowEndTimer->setSingleShot(true);
    connect(m_winGlowEndTimer, &QTimer::timeout, this, &GomokuWidget::startFadeOut);

    // Win glow repaint timer (periodic, for pulsing animation)
    m_winGlowRepaintTimer = new QTimer(this);
    connect(m_winGlowRepaintTimer, &QTimer::timeout, this, [this]() { update(); });

    // Fade timer
    m_fadeTimer = new QTimer(this);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        const auto &gcfg = ProfileManager::instance()->gomoku();
        m_fadeOpacity -= 16.0 / gcfg.fade_out_duration_ms;
        if (m_fadeOpacity <= 0.0) {
            m_fadeOpacity = 0.0;
            m_fadeTimer->stop();
            emit gameFinished();
            close();
            return;
        }
        update();
    });

    // Start invisible, then fade in
    setWindowOpacity(0.0);
    show();

    auto *fadeIn = new QPropertyAnimation(this, "boardOpacity", this);
    fadeIn->setDuration(800);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    connect(fadeIn, &QPropertyAnimation::finished, this, [this]() {
        emit boardReady();
        startGame();
    });
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

GomokuWidget::~GomokuWidget() {
    // H-03: Ensure async AI future is resolved before destruction
    if (m_aiWatcher) {
        m_aiWatcher->cancel();
        m_aiWatcher->waitForFinished();
        delete m_aiWatcher;
        m_aiWatcher = nullptr;
    }
}

void GomokuWidget::setBoardOpacity(qreal v) {
    m_boardOpacity = v;
    setWindowOpacity(v);
    update();
}

QPoint GomokuWidget::intersectionPos(int row, int col) const {
    return QPoint(m_padding + col * m_cellSize, m_padding + row * m_cellSize);
}

QPoint GomokuWidget::boardToScreen(int row, int col) const {
    return mapToGlobal(intersectionPos(row, col));
}

void GomokuWidget::startGame() {
    m_engine->reset(m_boardGridSize);
    m_gameActive = true;
    m_humanTurn = m_humanPlaysWhite; // White goes first

    if (!m_humanTurn) {
        startAITurn();
    }
}

void GomokuWidget::humanPlace(int row, int col) {
    if (!m_gameActive || !m_humanTurn || m_waitingForCharacter || m_particleActive) return;
    if (!m_engine->isEmpty(row, col)) return;

    m_engine->placePiece(row, col, m_humanColor);
    startParticleEffect(row, col);

    QTimer::singleShot(m_particleDurationMs + 50, this, [this]() {
        if (m_engine->lastResult() != GomokuEngine::InProgress) {
            checkGameEnd();
            return;
        }
        m_humanTurn = false;
        startAITurn();
    });
}

void GomokuWidget::startAITurn() {
    if (!m_gameActive || m_engine->isBoardFull()) return;

    const auto &gcfg = ProfileManager::instance()->gomoku();
    QTimer::singleShot(gcfg.ai_think_delay_ms, this, [this]() {
        if (!m_gameActive || m_engine->isBoardFull()) return;

        // H-03: Run AI computation off the main thread
        GomokuEngine *engine = m_engine;
        GomokuEngine::Cell aiColor = m_aiColor;

        if (m_aiWatcher) {
            m_aiWatcher->cancel();
            delete m_aiWatcher;
        }
        m_aiWatcher = new QFutureWatcher<QPoint>(this);
        connect(m_aiWatcher, &QFutureWatcher<QPoint>::finished, this, [this]() {
            if (!m_gameActive || !m_aiWatcher) return;
            m_pendingAIMove = m_aiWatcher->result();
            m_waitingForCharacter = true;

            QPoint screenPos = boardToScreen(m_pendingAIMove.x(), m_pendingAIMove.y());
            emit aiMoveReady(screenPos);
        });
        m_aiWatcher->setFuture(QtConcurrent::run([engine, aiColor]() {
            return engine->computeAIMove(aiColor);
        }));
    });
}

void GomokuWidget::confirmAIPlace() {
    if (!m_waitingForCharacter || !m_gameActive) return;
    m_waitingForCharacter = false;

    int row = m_pendingAIMove.x();
    int col = m_pendingAIMove.y();
    m_engine->placePiece(row, col, m_aiColor);
    startParticleEffect(row, col);

    QTimer::singleShot(m_particleDurationMs + 50, this, [this]() {
        emit aiPlaceDone();
        if (m_engine->lastResult() != GomokuEngine::InProgress) {
            checkGameEnd();
            return;
        }
        m_humanTurn = true;
        update();
    });
}

void GomokuWidget::quitGame() {
    // M-7: Re-entrancy guard — block if already fading or already quit
    if (m_fadeActive || (!m_gameActive && !m_winGlowActive)) return;

    m_gameActive = false;

    // H-03: Cancel pending async AI computation
    if (m_aiWatcher) {
        m_aiWatcher->cancel();
        m_aiWatcher->waitForFinished();
        delete m_aiWatcher;
        m_aiWatcher = nullptr;
    }
    m_waitingForCharacter = false;
    m_particleTimer->stop();
    m_particleActive = false;
    m_winGlowEndTimer->stop();
    m_winGlowRepaintTimer->stop();
    m_winGlowActive = false;
    startFadeOut();
}

void GomokuWidget::startParticleEffect(int row, int col) {
    const auto &gcfg = ProfileManager::instance()->gomoku();
    QPointF center(intersectionPos(row, col));
    m_particles.clear();

    int count = gcfg.particle_count;
    for (int i = 0; i < count; i++) {
        Particle p;
        double angle = QRandomGenerator::global()->generateDouble() * 2.0 * M_PI;
        double spawnRadius = m_cellSize * 0.8;
        double dist  = spawnRadius + QRandomGenerator::global()->generateDouble() * spawnRadius;
        p.startPos = QPointF(center.x() + dist * std::cos(angle),
                             center.y() + dist * std::sin(angle));
        p.endPos = center;
        p.hue  = QRandomGenerator::global()->generateDouble();
        double baseSize = m_cellSize * 0.06;
        p.size = baseSize + QRandomGenerator::global()->generateDouble() * baseSize * 1.5;
        m_particles.append(p);
    }
    m_particleActive = true;
    m_particleElapsed.start();
    m_particleTimer->start(16);
    update();
}

void GomokuWidget::onParticleTick() {
    if (!m_particleActive) {
        m_particleTimer->stop();
        return;
    }
    int elapsed = m_particleElapsed.elapsed();
    if (elapsed >= m_particleDurationMs) {
        m_particleActive = false;
        m_particles.clear();
        m_particleTimer->stop();
    }
    update();
}

void GomokuWidget::checkGameEnd() {
    auto result = m_engine->lastResult();
    if (result == GomokuEngine::InProgress) return;

    m_gameActive = false;

    if (result == GomokuEngine::BlackWins || result == GomokuEngine::WhiteWins) {
        startWinGlow();
    } else {
        // Draw
        QTimer::singleShot(500, this, &GomokuWidget::startFadeOut);
    }
}

void GomokuWidget::startWinGlow() {
    const auto &gcfg = ProfileManager::instance()->gomoku();
    m_winGlowActive = true;
    m_winGlowElapsed.start();
    m_winGlowRepaintTimer->start(30);
    m_winGlowEndTimer->start(gcfg.win_glow_duration_ms);
    update();
}

void GomokuWidget::startFadeOut() {
    m_winGlowActive = false;
    m_winGlowRepaintTimer->stop();
    m_fadeActive = true;
    m_fadeOpacity = 1.0;
    m_fadeTimer->start(16);
}

void GomokuWidget::mousePressEvent(QMouseEvent *event) {
    if (!m_gameActive || !m_humanTurn || m_waitingForCharacter || m_particleActive) {
        event->ignore();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    // Snap-to-nearest-intersection logic
    QPoint clickPos = event->pos();
    double fx = static_cast<double>(clickPos.x() - m_padding) / m_cellSize;
    double fy = static_cast<double>(clickPos.y() - m_padding) / m_cellSize;
    int col = qRound(fx);
    int row = qRound(fy);

    // Clamp to valid board range
    if (row < 0 || row >= m_boardGridSize || col < 0 || col >= m_boardGridSize) return;

    // Effective click radius: accept if within 0.4 * cellSize of the intersection
    QPoint intPos = intersectionPos(row, col);
    double dx = clickPos.x() - intPos.x();
    double dy = clickPos.y() - intPos.y();
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist > m_cellSize * 0.4) return;

    humanPlace(row, col);
}

void GomokuWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Near-invisible fill: alpha=1 prevents Windows click-through on transparent areas
    painter.fillRect(rect(), QColor(0, 0, 0, 1));

    if (m_fadeActive) {
        painter.setOpacity(m_fadeOpacity);
    }

    const auto &gcfg = ProfileManager::instance()->gomoku();

    QColor lineColor(255, 215, 0, 180);

    // -- Border (thick golden silk lines) --
    QPen borderPen(lineColor, gcfg.border_line_width);
    borderPen.setCapStyle(Qt::RoundCap);
    borderPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    QPoint topLeft     = intersectionPos(0, 0);
    QPoint bottomRight = intersectionPos(m_boardGridSize - 1, m_boardGridSize - 1);
    painter.drawRect(QRect(topLeft, bottomRight));

    // -- Inner grid (thin golden silk lines) --
    QColor innerColor(255, 215, 0, 120);
    QPen gridPen(innerColor, gcfg.inner_line_width);
    gridPen.setCapStyle(Qt::RoundCap);
    painter.setPen(gridPen);
    for (int i = 1; i < m_boardGridSize - 1; i++) {
        painter.drawLine(intersectionPos(i, 0), intersectionPos(i, m_boardGridSize - 1));
        painter.drawLine(intersectionPos(0, i), intersectionPos(m_boardGridSize - 1, i));
    }

    // -- Star points (天元 + corner stars) for standard 15x15 --
    if (m_boardGridSize == 15) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 215, 0, 200));
        static const int starPts[][2] = {{3,3},{3,11},{7,7},{11,3},{11,11}};
        for (const auto &sp : starPts) {
            int starR = qMax(3, m_cellSize / 12);
            painter.drawEllipse(intersectionPos(sp[0], sp[1]), starR, starR);
        }
    }

    // -- Pieces --
    int pieceR = gcfg.piece_radius;
    for (int r = 0; r < m_boardGridSize; r++) {
        for (int c = 0; c < m_boardGridSize; c++) {
            GomokuEngine::Cell cell = m_engine->cellAt(r, c);
            if (cell == GomokuEngine::Empty) continue;

            QPoint pos = intersectionPos(r, c);

            if (cell == GomokuEngine::White) {
                QRadialGradient grad(pos - QPoint(2, 2), pieceR);
                grad.setColorAt(0.0, QColor(255, 255, 255));
                grad.setColorAt(0.8, QColor(230, 230, 230));
                grad.setColorAt(1.0, QColor(200, 200, 200));
                painter.setPen(QPen(QColor(150, 150, 150), 1));
                painter.setBrush(grad);
            } else {
                QRadialGradient grad(pos - QPoint(2, 2), pieceR);
                grad.setColorAt(0.0, QColor(90, 90, 90));
                grad.setColorAt(0.7, QColor(30, 30, 30));
                grad.setColorAt(1.0, QColor(10, 10, 10));
                painter.setPen(QPen(QColor(0, 0, 0), 1));
                painter.setBrush(grad);
            }
            painter.drawEllipse(pos, pieceR, pieceR);

            // Win glow overlay
            if (m_winGlowActive) {
                bool isWin = false;
                for (const QPoint &wp : m_engine->winningLine()) {
                    if (wp.x() == r && wp.y() == c) { isWin = true; break; }
                }
                if (isWin) {
                    double t = (m_winGlowElapsed.elapsed() % 800) / 800.0;
                    double glowAlpha = 0.3 + 0.5 * (0.5 + 0.5 * std::sin(t * 2.0 * M_PI));
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(255, 215, 0, static_cast<int>(255 * glowAlpha)));
                    int glowExtra = qMax(5, m_cellSize / 8);
                    painter.drawEllipse(pos, pieceR + glowExtra, pieceR + glowExtra);
                }
            }
        }
    }

    // -- Particle convergence effect --
    if (m_particleActive && !m_particles.isEmpty()) {
        int elapsed = m_particleElapsed.elapsed();
        double progress = std::min(1.0, static_cast<double>(elapsed) / m_particleDurationMs);

        for (const Particle &p : m_particles) {
            double easedT = progress * progress; // ease-in: accelerate toward center
            QPointF pos = p.startPos + (p.endPos - p.startPos) * easedT;
            double alpha = 1.0 - progress;
            double sz    = p.size * (1.0 - progress * 0.7);

            // Golden-white sparkle
            int rv = 255;
            int gv = static_cast<int>(200 + 55 * p.hue);
            int bv = static_cast<int>(80 + 80 * p.hue);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(rv, gv, bv, static_cast<int>(255 * alpha)));
            painter.drawEllipse(pos, sz, sz);

            // Bright core
            painter.setBrush(QColor(255, 255, 255, static_cast<int>(200 * alpha)));
            painter.drawEllipse(pos, sz * 0.4, sz * 0.4);
        }
    }
}
