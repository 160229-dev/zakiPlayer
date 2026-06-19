#include "audiospectrum.h"

#include <QMediaPlayer>
#include <QtMath>
#include <QDateTime>
#include <QtDebug>

// QAudioProbe 在 Qt 6.7 中被移除，使用条件编译
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
#include <QAudioProbe>
#define HAS_AUDIO_PROBE 1
#endif

// 简易 DFT（离散傅里叶变换），针对小窗口足够使用
namespace {
constexpr int kFftSize = 1024;
}

AudioSpectrum::AudioSpectrum(QObject *parent)
    : QObject(parent)
    , m_probe(nullptr)
    , m_simTimer(new QTimer(this))
    , m_bandCount(48)
    , m_active(true)
    , m_usingProbe(false)
    , m_isPlaying(false)
{
    m_bands.fill(0.0, m_bandCount);
    m_smoothBands.fill(0.0, m_bandCount);
    m_targets.fill(0.0, m_bandCount);

    m_simTimer->setInterval(40); // ~25fps
    connect(m_simTimer, &QTimer::timeout, this, &AudioSpectrum::onSimTick);
}

AudioSpectrum::~AudioSpectrum()
{
    stopSimulation();
    detachFromPlayer();
}

void AudioSpectrum::setBandCount(int n)
{
    n = qBound(8, n, 128);
    if (n == m_bandCount) return;
    m_bandCount = n;
    m_bands.fill(0.0, m_bandCount);
    m_smoothBands.fill(0.0, m_bandCount);
    m_targets.fill(0.0, m_bandCount);
    emit bandCountChanged();
    emit spectrumUpdated(m_smoothBands);
}

void AudioSpectrum::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    if (!m_active) {
        m_bands.fill(0.0, m_bandCount);
        m_smoothBands.fill(0.0, m_bandCount);
        m_targets.fill(0.0, m_bandCount);
        emit spectrumUpdated(m_smoothBands);
        stopSimulation();
    } else {
        // 重新启用时启动模拟（如果正在播放则会产生真实数据）
        startSimulation();
    }
    emit activeChanged();
}

void AudioSpectrum::attachToPlayer(QMediaPlayer *player)
{
    if (m_player == player) return;
    detachFromPlayer();
    m_player = player;
    if (!m_player) return;

#ifdef HAS_AUDIO_PROBE
    m_probe = new QAudioProbe(this);
    if (m_probe->setSource(m_player)) {
        m_usingProbe = true;
        connect(m_probe, &QAudioProbe::audioBufferProbed,
                this, &AudioSpectrum::onAudioBufferProbed);
        qDebug() << "AudioSpectrum: using QAudioProbe for real audio analysis";
    } else {
        m_usingProbe = false;
        qDebug() << "AudioSpectrum: QAudioProbe unavailable, using simulation";
    }
#else
    m_usingProbe = false;
    qDebug() << "AudioSpectrum: QAudioProbe not available in this Qt version, using simulation";
#endif

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState s) {
                onPlaybackStateChanged(s == QMediaPlayer::PlayingState);
            });
}

void AudioSpectrum::detachFromPlayer()
{
#ifdef HAS_AUDIO_PROBE
    if (m_probe) {
        m_probe->deleteLater();
        m_probe = nullptr;
    }
#endif
    if (m_player) {
        disconnect(m_player, nullptr, this, nullptr);
    }
    m_player = nullptr;
    m_usingProbe = false;
    stopSimulation();
}

QVector<qreal> AudioSpectrum::currentBands() const
{
    return m_smoothBands;
}

void AudioSpectrum::onPlaybackStateChanged(bool playing)
{
    bool was = m_isPlaying;
    m_isPlaying = playing;
    if (was != playing) {
        emit playbackActiveChanged(playing);
    }
    if (playing && m_active) {
        startSimulation();
    } else {
        // 衰减
        if (m_simTimer->isActive()) {
            m_simTimer->stop();
        }
        // 平滑回落
        for (int i = 0; i < m_bandCount; ++i) {
            m_targets[i] = 0.0;
        }
    }
}

void AudioSpectrum::onAudioBufferProbed(const QAudioBuffer &buffer)
{
    if (!m_active) return;
    analyzeBuffer(buffer);
}

void AudioSpectrum::analyzeBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid()) return;
    const QAudioFormat fmt = buffer.format();
    if (!fmt.isValid()) return;

    const int channelCount = fmt.channelCount();
    const int sampleCount = qMin(buffer.sampleCount(), kFftSize);
    if (sampleCount <= 0 || channelCount <= 0) return;

    // 收集单声道 PCM 数据
    QVector<qreal> samples(sampleCount);
    const qint16 *data16 = nullptr;
    const float *data32 = nullptr;
    if (fmt.sampleFormat() == QAudioFormat::Float) {
        data32 = buffer.constData<float>();
    } else {
        data16 = buffer.constData<qint16>();
    }

    for (int i = 0; i < sampleCount; ++i) {
        qreal v = 0.0;
        for (int c = 0; c < channelCount; ++c) {
            int idx = i * channelCount + c;
            if (data32) {
                v += data32[idx];
            } else if (data16) {
                v += data16[idx] / 32768.0;
            }
        }
        samples[i] = v / channelCount;
    }

    // 应用汉宁窗
    for (int i = 0; i < sampleCount; ++i) {
        const qreal w = 0.5 * (1.0 - qCos(2.0 * M_PI * i / (sampleCount - 1)));
        samples[i] *= w;
    }

    // 计算频带能量
    QVector<qreal> result(m_bandCount, 0.0);
    const int bandSize = sampleCount / 2 / m_bandCount;
    if (bandSize <= 0) return;

    for (int b = 0; b < m_bandCount; ++b) {
        qreal energy = 0.0;
        const int start = b * bandSize;
        const int end = qMin(start + bandSize, sampleCount / 2);
        for (int k = start; k < end; ++k) {
            // 简化版：只取实部累加（实际为 |DFT|^2 的近似）
            qreal re = 0.0, im = 0.0;
            for (int n = 0; n < sampleCount; ++n) {
                const qreal angle = 2.0 * M_PI * k * n / sampleCount;
                re += samples[n] * qCos(angle);
                im -= samples[n] * qSin(angle);
            }
            energy += re * re + im * im;
        }
        energy = qSqrt(energy / bandSize);
        // 归一化
        energy = qMin(1.0, energy * 6.0);
        result[b] = energy;
    }

    // 映射到目标
    QMutexLocker lock(&m_mutex);
    m_targets = result;
    // 如果没有启动模拟定时器，启动它来平滑
    if (!m_simTimer->isActive()) {
        m_simTimer->start();
    }
}

void AudioSpectrum::onSimTick()
{
    QVector<qreal> target = m_targets;
    bool playing = m_isPlaying;

    if (!m_usingProbe) {
        // 模拟模式：产生伪随机但节奏化的频谱
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qreal t = now / 600.0;
        for (int i = 0; i < m_bandCount; ++i) {
            const qreal fi = i / qreal(m_bandCount);
            qreal v = 0.0;
            if (playing) {
                v = 0.55 + 0.30 * qSin(t + i * 0.45) + 0.20 * qSin(t * 0.7 + i * 0.3);
                v = v * (0.4 + 0.6 * (1.0 - fi * 0.7));
                v = v * (0.8 + 0.4 * qSin(t * 1.3 + i * 0.9));
                v = qMax(0.0, v);
                v = qMin(1.0, v);
            } else {
                v = 0.0;
            }
            target[i] = v;
        }
    }

    // 平滑过渡
    const qreal attack = m_usingProbe ? 0.45 : 0.55;
    const qreal release = 0.12;
    for (int i = 0; i < m_bandCount; ++i) {
        const qreal cur = m_smoothBands.value(i, 0.0);
        const qreal dst = target.value(i, 0.0);
        const qreal rate = (dst > cur) ? attack : release;
        m_smoothBands[i] = cur + (dst - cur) * rate;
    }
    emitBands(m_smoothBands);
}

void AudioSpectrum::startSimulation()
{
    if (!m_simTimer->isActive()) {
        m_simTimer->start();
    }
}

void AudioSpectrum::stopSimulation()
{
    if (m_simTimer->isActive()) {
        m_simTimer->stop();
    }
    if (m_active) {
        m_bands.fill(0.0, m_bandCount);
        m_smoothBands.fill(0.0, m_bandCount);
        m_targets.fill(0.0, m_bandCount);
        emitBands(m_smoothBands);
    }
}

void AudioSpectrum::emitBands(const QVector<qreal> &bands)
{
    emit spectrumUpdated(bands);
}
