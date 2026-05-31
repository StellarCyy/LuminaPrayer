#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>

class AudioManager : public QObject
{
    Q_OBJECT
public:
    explicit AudioManager(QObject *parent = nullptr);

    // 给外部调用的接口：播放指定路径的声音
    void playVoice(const QString &audioPath);

private:
    void ensurePlayer();

    QMediaPlayer *player;
    QAudioOutput *audioOutput;
};

#endif // AUDIOMANAGER_H
