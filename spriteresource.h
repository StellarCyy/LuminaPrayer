#ifndef SPRITERESOURCE_H
#define SPRITERESOURCE_H

#include "roleact.h"
#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include <QPixmap>
#include <QSize>

class SpriteResource : public QObject {
    Q_OBJECT
public:
    explicit SpriteResource(QObject *parent = nullptr);

    void loadAll();

    QList<QString> actionPaths(RoleAct act, CharacterForm form) const;
    QList<QString> alternateMovePaths(CharacterForm currentForm) const;
    const QPixmap& pixmap(const QString &path) const;
    bool hasPixmap(const QString &path) const;

    // Returns the cache key for the pre-flipped (mirrored) variant of a pull sprite.
    static QString mirroredKey(const QString &path) { return path + QStringLiteral("#mirrored"); }

    // D-2: Centralized frame resolution for directional (Move) and non-directional actions.
    // Move paths are packed as [left0..leftN, right0..rightN]; this helper picks the
    // correct sub-range based on faceRight and frame index.
    static QString resolveFramePath(const QList<QString> &paths, RoleAct act,
                                    bool faceRight, int frameIndex)
    {
        if (paths.isEmpty()) return QString();
        if (act == RoleAct::Move && paths.size() >= 2) {
            const int frameCount = paths.size() / 2;
            if (frameCount > 0) {
                const int base = faceRight ? frameCount : 0;
                return paths[base + (frameIndex % frameCount)];
            }
        }
        return paths[frameIndex % paths.size()];
    }

private:
    void addResource(QMap<RoleAct, QList<QString>> &targetMap,
                     RoleAct act, const QString &pattern, int count,
                     const QSize &targetSize = QSize());
    void addMoveResource(QMap<RoleAct, QList<QString>> &targetMap,
                         const QString &leftPattern, const QString &rightPattern,
                         int frameCount);
    void cachePixmap(const QString &path);
    void cachePixmapScaled(const QString &path, const QSize &targetSize);
    void cacheMirroredVariant(const QString &path);

    QMap<CharacterForm, QMap<RoleAct, QList<QString>>> m_formMaps;
    QMap<QString, QPixmap> m_cache;
    static const QPixmap& nullPixmap();
};

#endif // SPRITERESOURCE_H
