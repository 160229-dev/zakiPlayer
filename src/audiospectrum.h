#ifndef AUDIOSPECTRUM_H
#define AUDIOSPECTRUM_H

#include <QObject>
#include <QPointer>
#include <QAudioBuffer>
#include <QVector>
#include <QTimer>
#include <QMutex>

class QMediaPlayer;
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
class QAudioProbe;
#endif

/**
 * @brief 频谱分析器。
 * 优先使用 QAudioProbe 从 QMediaPlayer 中抓取音频缓冲做频谱分析。
 * 当 QAudioProbe 不可用时（例如 Qt 新版本已移除）将启动一个"模拟"模式，
 * 根据播放状态产生伪随机但悦动的频谱数据。
 */
class AudioSpectrum : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int bandCount READ bandCount WRITE setBandCount NOTIFY bandCountChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
public:
    explicit AudioSpectrum(QObject *parent = nullptr);
    ~AudioSpectrum();

    int bandCount() const { return m_bandCount; }
    void setBandCount(int n);

    bool isActive() const { return m_active; }
    void setActive(bool active);

    void attachToPlayer(QMediaPlayer *player);
    void detachFromPlayer();

    QVector<qreal> currentBands() const;

signals:
    void bandCountChanged();
    void activeChanged();
    void spectrumUpdated(const QVector<qreal> &bands);
    void playbackActiveChanged(bool playing);

public slots:
    void onPlaybackStateChanged(bool playing);

private slots:
    void onAudioBufferProbed(const QAudioBuffer &buffer);
    void onSimTick();

private:
    void startSimulation();
    void stopSimulation();
    void analyzeBuffer(const QAudioBuffer &buffer);
    void emitBands(const QVector<qreal> &bands);

    QPointer<QMediaPlayer> m_player;
    QObject *m_probe;   // 实际是 QAudioProbe（Qt < 6.7），新版本不使用
    QTimer *m_simTimer;
    QVector<qreal> m_bands;
    QVector<qreal> m_smoothBands;
    QVector<qreal> m_targets;
    int m_bandCount;
    bool m_active;
    bool m_usingProbe;
    bool m_isPlaying;
    mutable QMutex m_mutex;
};

#endif // AUDIOSPECTRUM_H
