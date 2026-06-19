#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QPointer>
#include <QUrl>
#include <QString>
#include <QPixmap>

class AudioSpectrum;

class PlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QMediaPlayer::PlaybackState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentMediaChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY currentMediaChanged)
    Q_PROPERTY(QPixmap coverArt READ coverArt NOTIFY coverArtChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController();

    QMediaPlayer* mediaPlayer() const { return m_player; }
    QAudioOutput* audioOutput() const { return m_audioOutput; }
    QVideoSink* videoSink() const;

    qint64 duration() const;
    qint64 position() const;
    int volume() const;
    bool muted() const;
    QMediaPlayer::PlaybackState state() const;
    bool hasVideo() const;

    QString currentTitle() const;
    QString currentArtist() const;
    QString currentAlbum() const;
    QPixmap coverArt() const;
    QUrl currentSource() const { return m_currentSource; }

    void setMedia(const QUrl &url);
    void play();
    void pause();
    void stop();
    void togglePlayPause();
    void seek(qint64 position);
    void seekRelative(int deltaMs);
    void setVolume(int volume);
    void setMuted(bool muted);

    void setSpectrum(AudioSpectrum *spectrum);

signals:
    void durationChanged(qint64 duration);
    void positionChanged(qint64 position);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void stateChanged(QMediaPlayer::PlaybackState state);
    void hasVideoChanged(bool hasVideo);
    void currentMediaChanged();
    void coverArtChanged();
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void errorOccurred(const QString &message);
    void endOfMedia();

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onMetaDataChanged();

private:
    void updateHasVideo();
    void extractCoverArt();

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QPointer<AudioSpectrum> m_spectrum;

    QPixmap m_coverArt;
    QString m_currentTitle;
    QString m_currentArtist;
    QString m_currentAlbum;
    bool m_hasVideo;
    QUrl m_currentSource;
};

#endif // PLAYERCONTROLLER_H
