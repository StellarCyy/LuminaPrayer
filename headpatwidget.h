#ifndef HEADPATWIDGET_H
#define HEADPATWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QPainterPath>
#include <QColor>

class HeadPatWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double opacity READ windowOpacity WRITE setWindowOpacity)
public:
    explicit HeadPatWidget(QPoint pivotScreen, double radius,
                           double startDeg, double endDeg,
                           QWidget *parent = nullptr);

    // --- Tunable parameters (all public so Widget can override before start) ---
    double maxAngularSpeed = 0.5;   // deg per tick at peak
    double accelRate       = 0.015; // angular acceleration per tick (down swing)
    double decelRate       = 0.012; // angular deceleration per tick (up swing)
    int    tickMs          = 16;    // timer interval (~60 fps)
    int    patCount        = 3;     // number of pats before finishing
    bool   mirrored        = false; // true = hand from upper-right; false = from upper-left

    void start();
    QPoint handScreenPos() const;

signals:
    void contactStartAt(QPoint screenPos);  // hand reached lowest point (pat)
    void contactEnd();                      // hand leaving lowest point
    void finished();                        // entire animation done

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void onTick();
    void beginFadeOut();
    QPointF handLocalPos() const;
    void spawnContactEffects();
    void advanceEffects();
    void drawEffects(QPainter &p);

    QPixmap m_handPix;
    QTimer  m_timer;

    QPoint  m_pivot;       // global screen coords of arc center
    double  m_radius;
    double  m_startDeg;
    double  m_endDeg;

    double  m_angle;       // current angle (degrees)
    double  m_angularVel;  // deg/tick
    bool    m_swingDown;   // true = moving toward endDeg (contact)
    int     m_patsLeft;
    bool    m_inContact;
    bool    m_running;
    int     m_wSize;

    // --- Integrated visual effects (painted in-widget, no extra HWNDs) ---
    struct RippleEffect {
        double cx, cy;               // widget-local center
        int maxRadius    = 40;
        double startOpacity = 0.7;
        int durationMs   = 300;
        int elapsedMs    = 0;
        // OutQuad easing: fast start, slow end
        double progress() const {
            double t = qBound(0.0, double(elapsedMs) / durationMs, 1.0);
            return t * (2.0 - t);
        }
        bool done() const { return elapsedMs >= durationMs; }
    };

    struct StarParticle { double angle, speed, size; QColor color; };

    struct StarBurstEffect {
        QVector<StarParticle> stars;
        double cx, cy;
        int durationMs = 400;
        int elapsedMs  = 0;
        // OutCubic easing: burst fast, decelerate
        double progress() const {
            double t = qBound(0.0, double(elapsedMs) / durationMs, 1.0);
            double inv = 1.0 - t;
            return 1.0 - inv * inv * inv;
        }
        bool done() const { return elapsedMs >= durationMs; }
    };

    QVector<RippleEffect>    m_ripples;
    QVector<StarBurstEffect> m_starBursts;
    static const QPainterPath& unitStarPath(); // pre-cached 4-pointed star at origin, radius 1
};

#endif // HEADPATWIDGET_H
