#ifndef PLAYMATE_H
#define PLAYMATE_H

#include <QWidget>
#include <QList>
#include <QVector>
#include <QString>
#include <QPixmap>
#include <QTimer>

class Playmate : public QWidget
{
    Q_OBJECT

public:
    explicit Playmate(const QList<QString> &movePaths, QWidget *parent = nullptr);

    void setMovePaths(const QList<QString> &movePaths);
    void setFacingRight(bool facingRight);
    void playEntryAnimation();
    void playExitAnimation();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void advanceMoveFrame();

private:
    QList<QString> move_paths;
    QVector<QPixmap> m_frameCache;   // M-05: pre-loaded pixmaps
    int frame_count;
    int frame_index;
    bool move_face_right;

    QPixmap current_pix;
    QTimer *frame_timer;

    QPixmap light_pix;
    int current_light_size;
    bool show_light;
    double current_opacity;
};

#endif // PLAYMATE_H
