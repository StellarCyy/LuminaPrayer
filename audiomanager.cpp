#include "audiomanager.h"
#include <QDebug>
#include <QMediaDevices>
#include <QAudioDevice>

AudioManager::AudioManager(QObject *parent)
    : QObject{parent}, player(nullptr), audioOutput(nullptr)
{
}

void AudioManager::ensurePlayer()
{
    if (player) return;

    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isNull()) {
        audioOutput->setDevice(device);
    }
    audioOutput->setVolume(1.0);
    player->setAudioOutput(audioOutput);

    connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString){
        qWarning() << "AudioManager error:" << error << errorString;
        // H-04: On fatal error, disconnect + synchronous delete to avoid
        // device contention if ensurePlayer() is called before deleteLater fires.
        if (error != QMediaPlayer::NoError) {
            QMediaPlayer  *oldPlayer = player;
            QAudioOutput  *oldOutput = audioOutput;
            player      = nullptr;
            audioOutput = nullptr;
            oldPlayer->stop();
            oldPlayer->disconnect();
            delete oldPlayer;
            delete oldOutput;
        }
    });
}

void AudioManager::playVoice(const QString &audioPath)
{
    ensurePlayer();
    if (!player) return;

    player->stop();
    player->setSource(QUrl(audioPath));
    player->play();
}
