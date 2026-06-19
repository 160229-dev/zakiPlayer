#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QUrl>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    enum PlayMode {
        Sequential = 0,    // 顺序播放
        RepeatOne,         // 单曲循环
        RepeatAll,         // 列表循环
        Shuffle            // 随机播放
    };
    Q_ENUM(PlayMode)

    static SettingsManager* instance();

    void setLastOpenPath(const QString &path);
    QString lastOpenPath() const;

    void setLastOpenDir(const QUrl &dir);
    QUrl lastOpenDir() const;

    void setVolume(int volume);
    int volume() const;

    void setMuted(bool muted);
    bool muted() const;

    void setPlayMode(PlayMode mode);
    PlayMode playMode() const;

    void setWindowGeometry(const QByteArray &geom);
    QByteArray windowGeometry() const;

    void setWindowState(const QByteArray &state);
    QByteArray windowState() const;

    void setPlaylist(const QStringList &paths);
    QStringList playlist() const;

    void setLastIndex(int index);
    int lastIndex() const;

    void save();
    void load();

private:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    QSettings *m_settings;
};

#endif // SETTINGSMANAGER_H
