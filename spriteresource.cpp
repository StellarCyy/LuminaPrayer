#include "spriteresource.h"
#include "profilemanager.h"
#include <QTransform>
#include <algorithm>

const QPixmap& SpriteResource::nullPixmap() {
    static const QPixmap s_null;
    return s_null;
}

SpriteResource::SpriteResource(QObject *parent)
    : QObject(parent) {}

void SpriteResource::loadAll() {
    m_formMaps.clear();
    m_cache.clear();

    const SpritesProfile &sp = ProfileManager::instance()->sprites();
    const int displaySize = sp.role_display_size;
    const QSize roleSize(displaySize, displaySize);

    // --- Load all registered forms generically ---
    // First form loaded becomes the "base"; subsequent forms inherit from it then override.
    QMap<RoleAct, QList<QString>> baseMap;

    for (auto it = sp.forms.constBegin(); it != sp.forms.constEnd(); ++it) {
        const CharacterForm form = it.key();
        const FormDef &fd = it.value();

        // Start from base form (first form's data) or empty
        QMap<RoleAct, QList<QString>> &actMap = m_formMaps[form];
        if (!baseMap.isEmpty() && actMap.isEmpty())
            actMap = baseMap;

        addResource(actMap, RoleAct::Stand,    fd.stand.pattern,      fd.stand.count,      roleSize);
        addMoveResource(actMap, fd.move_left.pattern, fd.move_right.pattern, fd.move_left.count);
        addResource(actMap, RoleAct::Sleeping, fd.sleeping.pattern,   fd.sleeping.count,   roleSize);
        addResource(actMap, RoleAct::Angry,    fd.angry.pattern,      fd.angry.count,      roleSize);
        if (!fd.sitting_1.pattern.isEmpty())
            addResource(actMap, RoleAct::Sitting_1, fd.sitting_1.pattern, fd.sitting_1.count);
        if (!fd.sitting_2.pattern.isEmpty())
            addResource(actMap, RoleAct::Sitting_2, fd.sitting_2.pattern, fd.sitting_2.count);

        // Snapshot first form as base for inheritance
        if (baseMap.isEmpty())
            baseMap = actMap;
    }

    // --- Pull (hand-grabbing) sprites (scaled + pre-flipped) ---
    for (auto it = sp.pull.constBegin(); it != sp.pull.constEnd(); ++it) {
        const QString &path = it.value();
        if (!path.isEmpty()) {
            cachePixmapScaled(path, roleSize);
            cacheMirroredVariant(path);
        }
    }

    // --- Hint text sprites ---
    cachePixmap(sp.hint_text_ok);
    cachePixmap(sp.hint_text_can_sing);
    cachePixmap(sp.hint_text_go_to_sleep);
    cachePixmap(sp.hint_text_start_listening);
    cachePixmap(sp.hint_text_angry);
}

QList<QString> SpriteResource::actionPaths(RoleAct act, CharacterForm form) const {
    auto it = m_formMaps.constFind(form);
    if (it != m_formMaps.constEnd())
        return it->value(act);
    // Fallback: first available form
    if (!m_formMaps.isEmpty())
        return m_formMaps.first().value(act);
    return {};
}

QList<QString> SpriteResource::alternateMovePaths(CharacterForm currentForm) const {
    // Return Move paths from the next form (cyclic)
    if (m_formMaps.size() < 2) return {};
    auto it = m_formMaps.constFind(currentForm);
    if (it == m_formMaps.constEnd()) return {};
    ++it;
    if (it == m_formMaps.constEnd())
        it = m_formMaps.constBegin();
    return it->value(RoleAct::Move);
}

const QPixmap& SpriteResource::pixmap(const QString &path) const {
    auto it = m_cache.constFind(path);
    if (it != m_cache.constEnd()) {
        return it.value();
    }
    return nullPixmap();
}

bool SpriteResource::hasPixmap(const QString &path) const {
    return m_cache.contains(path);
}

void SpriteResource::addResource(QMap<RoleAct, QList<QString>> &targetMap,
                                  RoleAct act, const QString &pattern, int count,
                                  const QSize &targetSize) {
    QList<QString> paths;
    for (int i = 0; i < count; ++i) {
        QString finalPath = QString(pattern).replace(QLatin1String("%d"), QString::number(i));
        paths.append(finalPath);
        if (m_cache.contains(finalPath)) continue;   // R-1: skip redundant I/O
        QPixmap p;
        if (p.load(finalPath)) {
            if (targetSize.isValid()) {
                p = p.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            m_cache.insert(finalPath, p);
        }
    }
    targetMap.insert(act, paths);
}

void SpriteResource::addMoveResource(QMap<RoleAct, QList<QString>> &targetMap,
                                      const QString &leftPattern, const QString &rightPattern,
                                      int frameCount) {
    QList<QString> leftPaths, rightPaths;
    for (int i = 0; i < frameCount; ++i) {
        QString leftPath  = QString(leftPattern).replace(QLatin1String("%d"), QString::number(i));
        QString rightPath = QString(rightPattern).replace(QLatin1String("%d"), QString::number(i));
        // R-1: skip redundant I/O for already-cached paths
        if (m_cache.contains(leftPath)) {
            leftPaths.append(leftPath);
        } else {
            QPixmap p;
            if (p.load(leftPath)) {
                m_cache.insert(leftPath, p);
                leftPaths.append(leftPath);
            }
        }
        if (m_cache.contains(rightPath)) {
            rightPaths.append(rightPath);
        } else {
            QPixmap p;
            if (p.load(rightPath)) {
                m_cache.insert(rightPath, p);
                rightPaths.append(rightPath);
            }
        }
    }

    QList<QString> paths;
    const int validCount = std::min(leftPaths.size(), rightPaths.size());
    if (validCount > 0) {
        for (int i = 0; i < validCount; ++i) paths.append(leftPaths[i]);
        for (int i = 0; i < validCount; ++i) paths.append(rightPaths[i]);
    } else if (!leftPaths.isEmpty()) {
        paths = leftPaths;
    } else {
        paths = rightPaths;
    }
    targetMap.insert(RoleAct::Move, paths);
}

void SpriteResource::cachePixmap(const QString &path) {
    if (m_cache.contains(path)) return;   // R-1: dedup
    QPixmap p;
    if (p.load(path)) {
        m_cache.insert(path, p);
    }
}

void SpriteResource::cachePixmapScaled(const QString &path, const QSize &targetSize) {
    if (m_cache.contains(path)) return;   // R-1: dedup
    QPixmap p;
    if (p.load(path)) {
        if (targetSize.isValid())
            p = p.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_cache.insert(path, p);
    }
}

void SpriteResource::cacheMirroredVariant(const QString &path) {
    auto it = m_cache.constFind(path);
    if (it == m_cache.constEnd() || it->isNull()) return;
    // Horizontal flip via QTransform — done once at load, never in paintEvent
    QTransform flipH;
    flipH.scale(-1, 1);
    m_cache.insert(mirroredKey(path), it->transformed(flipH));
}
