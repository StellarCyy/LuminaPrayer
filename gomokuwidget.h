#ifndef GOMOKUWIDGET_H
#define GOMOKUWIDGET_H

#include "gomokuengine.h"
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPropertyAnimation>
#include <QVector>
#include <QPointF>
#include <QFutureWatcher>

class GomokuWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal boardOpacity READ boardOpacity WRITE setBoardOpacity)
public:
    explicit GomokuWidget(bool humanPlaysWhite, QWidget *parent = nullptr);
    ~GomokuWidget() override;

    // Convert board position to screen position (global coordinates)
    QPoint boardToScreen(int row, int col) const;

    // Called by Widget after character fly animation finishes
    void confirmAIPlace();

    // Called by Widget to quit the game early
    void quitGame();

    bool isGameActive() const { return m_gameActive; }

    qreal boardOpacity() const { return m_boardOpacity; }
    void setBoardOpacity(qreal v);

signals:
    // Board fade-in finished — ready to play
    void boardReady();
    // AI computed its move — Widget should fly character to screenPos
    void aiMoveReady(QPoint screenPos);

    // AI piece + particle finished — Widget can fly character back
    void aiPlaceDone();

    // Game completely over (after win glow + fade out)
    void gameFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void startGame();
    void humanPlace(int row, int col);
    void startAITurn();
    void startParticleEffect(int row, int col);
    void onParticleTick();
    void checkGameEnd();
    void startWinGlow();
    void startFadeOut();

    QPoint intersectionPos(int row, int col) const;

    GomokuEngine *m_engine;
    bool m_humanPlaysWhite;
    GomokuEngine::Cell m_humanColor;
    GomokuEngine::Cell m_aiColor;
    bool m_humanTurn;
    bool m_gameActive;
    bool m_waitingForCharacter;
    QPoint m_pendingAIMove;
    QFutureWatcher<QPoint> *m_aiWatcher = nullptr;

    // Layout
    int m_cellSize;
    int m_padding;
    int m_boardGridSize;

    // Particle effect
    struct Particle {
        QPointF startPos;
        QPointF endPos;
        double hue;
        double size;
    };
    QVector<Particle> m_particles;
    QTimer *m_particleTimer;
    QElapsedTimer m_particleElapsed;
    bool m_particleActive;
    int m_particleDurationMs;

    // Win glow
    bool m_winGlowActive;
    QTimer *m_winGlowEndTimer;
    QTimer *m_winGlowRepaintTimer;
    QElapsedTimer m_winGlowElapsed;

    // Fade out
    double m_fadeOpacity;
    bool m_fadeActive;
    QTimer *m_fadeTimer;

    // Fade in (entry)
    qreal m_boardOpacity;
};

#endif // GOMOKUWIDGET_H
