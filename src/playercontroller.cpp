#include "playercontroller.h"
#include "audiospectrum.h"
#include "settingsmanager.h"

#include <QMediaMetaData>
#include <QVideoFrame>
#include <QVideoSink>
#include <QImage>
#include <QPainter>
#include <QGuiApplication>
#include <QtDebug>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_hasVideo(false)
{
    m_player->setAudioOutput(m_audioOutput);

    // 初始化音量与静音状态
    int vol = SettingsManager::instance()->volume();
    m_audioOutput->setVolume(vol / 100.0);
    m_audioOutput->setMuted(SettingsManager::instance()->muted());

    connect(m_player, &QMediaPlayer::durationChanged,
            this, &PlayerController::durationChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &PlayerController::positionChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState s) {
                emit stateChanged(s);
            });
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error, const QString &err) {
                emit errorOccurred(err);
            });
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &PlayerController::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged,
            this, &PlayerController::onMetaDataChanged);

    connect(m_player, &QMediaPlayer::tracksChanged,
            this, [this]() { updateHasVideo(); });
}

PlayerController::~PlayerController()
{
    SettingsManager::instance()->setVolume(volume());
    SettingsManager::instance()->setMuted(muted());
    SettingsManager::instance()->save();
}

QVideoSink* PlayerController::videoSink() const
{
    return m_player->videoSink();
}

qint64 PlayerController::duration() const
{
    return m_player->duration();
}

qint64 PlayerController::position() const
{
    return m_player->position();
}

int PlayerController::volume() const
{
    return qRound(m_audioOutput->volume() * 100.0);
}

bool PlayerController::muted() const
{
    return m_audioOutput->isMuted();
}

QMediaPlayer::PlaybackState PlayerController::state() const
{
    return m_player->playbackState();
}

bool PlayerController::hasVideo() const
{
    return m_hasVideo;
}

QString PlayerController::currentTitle() const
{
    return m_currentTitle;
}

QString PlayerController::currentArtist() const
{
    return m_currentArtist;
}

QString PlayerController::currentAlbum() const
{
    return m_currentAlbum;
}

QPixmap PlayerController::coverArt() const
{
    return m_coverArt;
}

void PlayerController::setMedia(const QUrl &url)
{
    if (url == m_currentSource && !url.isEmpty()) {
        return;
    }
    m_currentSource = url;
    m_coverArt = QPixmap();
    m_currentTitle.clear();
    m_currentArtist.clear();
    m_currentAlbum.clear();
    emit currentMediaChanged();
    emit coverArtChanged();
    m_player->stop();
    m_player->setSource(url);
    // 显式重置时间/时长
    emit positionChanged(0);
    emit durationChanged(0);
    updateHasVideo();
}

void PlayerController::play()
{
    m_player->play();
}

void PlayerController::pause()
{
    m_player->pause();
}

void PlayerController::stop()
{
    m_player->stop();
}

void PlayerController::togglePlayPause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void PlayerController::seek(qint64 position)
{
    m_player->setPosition(position);
}

void PlayerController::seekRelative(int deltaMs)
{
    qint64 newPos = m_player->position() + deltaMs;
    newPos = qBound(qint64(0), newPos, m_player->duration());
    m_player->setPosition(newPos);
}

void PlayerController::setVolume(int volume)
{
    volume = qBound(0, volume, 100);
    m_audioOutput->setVolume(volume / 100.0);
    SettingsManager::instance()->setVolume(volume);
    emit volumeChanged(volume);
}

void PlayerController::setMuted(bool muted)
{
    m_audioOutput->setMuted(muted);
    SettingsManager::instance()->setMuted(muted);
    emit mutedChanged(muted);
}

void PlayerController::setSpectrum(AudioSpectrum *spectrum)
{
    m_spectrum = spectrum;
    if (m_spectrum) {
        m_spectrum->attachToPlayer(m_player);
    }
}

void PlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    emit mediaStatusChanged(status);
    if (status == QMediaPlayer::EndOfMedia) {
        emit endOfMedia();
    } else if (status == QMediaPlayer::LoadedMedia) {
        // 媒体加载完成后再尝试提取封面
        extractCoverArt();
    }
}

void PlayerController::onMetaDataChanged()
{
    extractCoverArt();

    const QMediaMetaData meta = m_player->metaData();

    QString title = meta.stringValue(QMediaMetaData::Title);
    QString artist = meta.stringValue(QMediaMetaData::ContributingArtist);
    QString album = meta.stringValue(QMediaMetaData::AlbumTitle);

    if (title.isEmpty()) {
        title = m_currentSource.fileName();
        if (title.isEmpty()) {
            title = m_currentSource.toString();
        }
    }
    if (artist.isEmpty()) {
        artist = tr("未知艺术家");
    }
    if (album.isEmpty()) {
        album = tr("未知专辑");
    }

    bool changed = false;
    if (title != m_currentTitle) { m_currentTitle = title; changed = true; }
    if (artist != m_currentArtist) { m_currentArtist = artist; changed = true; }
    if (album != m_currentAlbum) { m_currentAlbum = album; changed = true; }

    if (changed) {
        emit currentMediaChanged();
    }
}

void PlayerController::updateHasVideo()
{
    bool nowHasVideo = m_player->hasVideo();
    if (nowHasVideo != m_hasVideo) {
        m_hasVideo = nowHasVideo;
        emit hasVideoChanged(m_hasVideo);
    }
}

void PlayerController::extractCoverArt()
{
    const QMediaMetaData meta = m_player->metaData();
    QVariant cover = meta.value(QMediaMetaData::CoverArtImage);
    if (cover.canConvert<QImage>()) {
        QImage img = cover.value<QImage>();
        if (!img.isNull()) {
            m_coverArt = QPixmap::fromImage(img);
            emit coverArtChanged();
            return;
        }
    }
    cover = meta.value(QMediaMetaData::ThumbnailImage);
    if (cover.canConvert<QImage>()) {
        QImage img = cover.value<QImage>();
        if (!img.isNull()) {
            m_coverArt = QPixmap::fromImage(img);
            emit coverArtChanged();
            return;
        }
    }
    // 没找到封面：清空，恢复默认音符
    if (!m_coverArt.isNull()) {
        m_coverArt = QPixmap();
        emit coverArtChanged();
    }
}
