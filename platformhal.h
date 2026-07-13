#ifndef PLATFORMHAL_H
#define PLATFORMHAL_H

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QRect>

// 【核心修复 1】禁止 Windows 定义 min/max 宏，防止与 std::min 冲突
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

struct SittableWindow {
    bool valid = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int top = 0;
#ifdef Q_OS_WIN
    HWND hwnd = nullptr;
    RECT rect = {};
#endif
};

class PlatformHAL : public QObject {
    Q_OBJECT
public:
    explicit PlatformHAL(QObject *parent = nullptr);

    SittableWindow findSittableWindow(quintptr excludeWinId) const;
    bool isTargetWindowValid() const;
    bool hasTargetWindowMoved() const;
    void setTargetWindow(const SittableWindow &win);
    void clearTargetWindow();
    const SittableWindow& targetWindow() const;

    static QPoint clampToScreen(const QPoint &topLeft, const QSize &windowSize);

    // v4 perception: title of the current foreground window (empty when
    // unavailable or on unsupported platforms).
    static QString foregroundWindowTitle();

private:
#ifdef Q_OS_WIN
    HWND m_targetHwnd = nullptr;
    RECT m_targetRect = {};
#endif
    SittableWindow m_targetWindow;
};

#endif // PLATFORMHAL_H
