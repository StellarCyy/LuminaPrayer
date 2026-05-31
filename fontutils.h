#ifndef FONTUTILS_H
#define FONTUTILS_H

#include <QFont>
#include <QFontInfo>
#include <QStringList>

// Shared CJK serif font resolution — picks the first available family
// from a preferred fallback chain. Used by FoodGridWidget, ChatBubbleWidget, etc.
inline QFont resolveCJKSerifFont(int pixelSize = 12, QFont::Weight weight = QFont::DemiBold)
{
    static const QStringList candidates = {
        QStringLiteral("Source Han Serif SC"),
        QStringLiteral("Noto Serif CJK SC"),
        QStringLiteral("STSong"),
        QStringLiteral("Microsoft YaHei"),
    };

    QFont font;
    bool matched = false;
    for (const auto &family : candidates) {
        font.setFamily(family);
        if (QFontInfo(font).family().contains(family.left(4), Qt::CaseInsensitive)) {
            matched = true;
            break;
        }
    }
    if (!matched) font.setFamily(QStringLiteral("Microsoft YaHei"));

    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

// Shared CJK sans-serif font resolution — for UI labels (StatusPanel, etc.)
inline QFont resolveCJKSansFont(int pixelSize = 12, QFont::Weight weight = QFont::Normal)
{
    static const QStringList candidates = {
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Source Han Sans SC"),
        QStringLiteral("PingFang SC"),
    };

    QFont font;
    bool matched = false;
    for (const auto &family : candidates) {
        font.setFamily(family);
        if (QFontInfo(font).family().contains(family.left(4), Qt::CaseInsensitive)) {
            matched = true;
            break;
        }
    }
    if (!matched) font.setFamily(QStringLiteral("Microsoft YaHei"));

    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

#endif // FONTUTILS_H
