#ifndef FISTWIDGET_H
#define FISTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QTimer>

class StaticHalo : public QWidget {
public:
    StaticHalo(QRect rect, QPixmap pix, int size, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    void startFadeOut();
    QPixmap m_lightPix;
    int m_lightSize;
};

class FistWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double opacity READ windowOpacity WRITE setWindowOpacity)
public:
    explicit FistWidget(QPoint startPos, bool isLeftSpawn, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    void startAttackSequence();

    QPixmap m_fistPix;
    QPixmap m_fistScaled;   // MED-12: pre-scaled once, not per-frame
    QPixmap m_lightPix;
    int m_lightSize;
    bool m_showFist;
    QTimer *m_trackTimer;
    double m_currentSpeed;
    bool m_isLeftSpawn;
    QPoint m_currentMousePos;
};

#endif // FISTWIDGET_H
