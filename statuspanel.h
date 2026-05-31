#ifndef STATUSPANEL_H
#define STATUSPANEL_H

#include <QWidget>
#include <QTimer>
#include <QFont>

class StatusManager;

class StatusPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double panelOpacity READ panelOpacity WRITE setPanelOpacity)

public:
    explicit StatusPanel(StatusManager *mgr, QWidget *parent = nullptr);

    void showAt(const QPoint &anchorTopRight, const QSize &screenSize);
    void showForDuration(const QPoint &anchorTopRight, const QSize &screenSize, int durationMs = 20000);
    void fadeIn();
    void fadeOut();
    bool isFadingOut() const { return m_fadingOut; }

    double panelOpacity() const { return m_opacity; }
    void setPanelOpacity(double v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    StatusManager *m_statusMgr;
    double m_opacity;
    bool   m_fadingOut;
    QTimer *m_fadeTimer;
    double m_fadeTarget;
    double m_fadeStep;

    static constexpr int PanelW = 300;
    static constexpr int PanelH = 300;
    static constexpr int FadeDurationMs = 500;
    static constexpr int FadeIntervalMs = 16;

    QTimer *m_autoHideTimer;

    QFont m_titleFont;
    QFont m_barFont;
};

#endif // STATUSPANEL_H
