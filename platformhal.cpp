#include "platformhal.h"
#include <QScreen>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <algorithm>

PlatformHAL::PlatformHAL(QObject *parent) : QObject(parent) {}

SittableWindow PlatformHAL::findSittableWindow(quintptr excludeWinId) const {
    SittableWindow result;
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindowVisible(hwnd)) return result;
    if (hwnd == reinterpret_cast<HWND>(excludeWinId)) return result;

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    QString winTitle = QString::fromWCharArray(title);
    if (winTitle.isEmpty() || winTitle == "Program Manager") return result;

    RECT r;
    GetWindowRect(hwnd, &r);
    int winW = r.right - r.left;
    if (winW < 400) return result;
    if (r.top < 400) return result;

    result.valid  = true;
    result.x      = r.left;
    result.y      = r.top;
    result.width  = winW;
    result.height = r.bottom - r.top;
    result.top    = r.top;
    result.hwnd   = hwnd;
    result.rect   = r;
#else
    Q_UNUSED(excludeWinId)
#endif
    return result;
}

bool PlatformHAL::isTargetWindowValid() const {
#ifdef Q_OS_WIN
    return m_targetHwnd && IsWindow(m_targetHwnd);
#else
    return false;
#endif
}

bool PlatformHAL::hasTargetWindowMoved() const {
#ifdef Q_OS_WIN
    if (!m_targetHwnd) return false;
    RECT currRect;
    GetWindowRect(m_targetHwnd, &currRect);
    return (currRect.left != m_targetRect.left ||
            currRect.top  != m_targetRect.top  ||
            (currRect.right - currRect.left) != (m_targetRect.right - m_targetRect.left));
#else
    return false;
#endif
}

void PlatformHAL::setTargetWindow(const SittableWindow &win) {
    m_targetWindow = win;
#ifdef Q_OS_WIN
    m_targetHwnd = win.hwnd;
    m_targetRect = win.rect;
#endif
}

void PlatformHAL::clearTargetWindow() {
    m_targetWindow = SittableWindow();
#ifdef Q_OS_WIN
    m_targetHwnd = nullptr;
    m_targetRect = {};
#endif
}

const SittableWindow& PlatformHAL::targetWindow() const {
    return m_targetWindow;
}

QPoint PlatformHAL::clampToScreen(const QPoint &topLeft, const QSize &windowSize) {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return topLeft;

    const QRect geom = screen->availableGeometry();
    const int maxX = geom.x() + geom.width()  - windowSize.width();
    const int maxY = geom.y() + geom.height() - windowSize.height();
    const int x = std::max(geom.x(), std::min(topLeft.x(), maxX));
    const int y = std::max(geom.y(), std::min(topLeft.y(), maxY));
    return QPoint(x, y);
}
