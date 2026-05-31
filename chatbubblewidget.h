#ifndef CHATBUBBLEWIDGET_H
#define CHATBUBBLEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QTimer>

class QLabel;
class QScrollArea;
class QGraphicsDropShadowEffect;

class ChatBubbleWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double bubbleOpacity READ bubbleOpacity WRITE setBubbleOpacity)
    Q_PROPERTY(double textOpacity READ textOpacity WRITE setTextOpacity)

public:
    explicit ChatBubbleWidget(QWidget *parent = nullptr);

    void showAt(const QPoint &anchorTopCenter);
    void setLoading();
    void setResponseText(const QString &text);
    void fadeOutAndHide();
    void setAutoCloseMs(int ms);
    void setContentPadding(int px);

    double bubbleOpacity() const { return m_bubbleOpacity; }
    void setBubbleOpacity(double v);
    double textOpacity() const { return m_textOpacity; }
    void setTextOpacity(double v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPixmap m_bgPixmap;
    QString m_displayText;
    bool    m_loading;
    double  m_bubbleOpacity;
    double  m_textOpacity;

    QTimer *m_fadeTimer;
    double  m_fadeTarget;
    double  m_fadeStep;
    enum FadeTarget { FadeBubble, FadeText, FadeOut } m_fadeMode;

    QTimer *m_autoCloseTimer;

    void startFade(FadeTarget mode, double from, double to, int durationMs);
    void onBubbleFadeComplete();
    void updateTextStyle();   // sync label color + glow alpha to current opacity
    void fitFontToContent();   // auto-size font to fit m_contentRect

    QScrollArea               *m_scrollArea;
    QLabel                    *m_textLabel;
    QGraphicsDropShadowEffect *m_glowEffect;

    bool    m_bubbleReady;   // true once background fully faded in
    bool    m_pendingTextFade; // deferred text fade after bubble ready
    QString m_pendingText;     // text to show after bubble ready

    static constexpr int FadeIntervalMs = 16;
    int m_autoCloseMs = 15000;

    // Content area: computed dynamically from pixmap size
    QRect m_contentRect;
    int m_contentPadding = 50;
    void recomputeContentRect();
    static constexpr int FontSizeMin    = 10;
    static constexpr int FontSizeMax    = 26;
    QFont m_baseFont;   // family + weight + strategy (no size)
};

#endif // CHATBUBBLEWIDGET_H
