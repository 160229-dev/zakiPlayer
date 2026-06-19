#include "settingsmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

SettingsManager* SettingsManager::instance()
{
    static SettingsManager s_instance;
    return &s_instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings(QSettings::IniFormat,
                               QSettings::UserScope,
                               QCoreApplication::organizationName(),
                               QCoreApplication::applicationName(),
                               this);
    if (m_settings->value("firstRun", true).toBool()) {
        m_settings->setValue("firstRun", false);
        m_settings->setValue("volume", 80);
        m_settings->setValue("muted", false);
        m_settings->setValue("playMode", Sequential);
        // 默认打开路径为用户主目录
        m_settings->setValue("lastOpenPath",
                             QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
        m_settings->sync();
    }
}

void SettingsManager::setLastOpenPath(const QString &path)
{
    m_settings->setValue("lastOpenPath", path);
}

QString SettingsManager::lastOpenPath() const
{
    return m_settings->value("lastOpenPath",
                             QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString();
}

void SettingsManager::setLastOpenDir(const QUrl &dir)
{
    m_settings->setValue("lastOpenDir", dir);
}

QUrl SettingsManager::lastOpenDir() const
{
    return m_settings->value("lastOpenDir").toUrl();
}

void SettingsManager::setVolume(int volume)
{
    m_settings->setValue("volume", qBound(0, volume, 100));
}

int SettingsManager::volume() const
{
    return m_settings->value("volume", 80).toInt();
}

void SettingsManager::setMuted(bool muted)
{
    m_settings->setValue("muted", muted);
}

bool SettingsManager::muted() const
{
    return m_settings->value("muted", false).toBool();
}

void SettingsManager::setPlayMode(PlayMode mode)
{
    m_settings->setValue("playMode", static_cast<int>(mode));
}

SettingsManager::PlayMode SettingsManager::playMode() const
{
    return static_cast<PlayMode>(m_settings->value("playMode", Sequential).toInt());
}

void SettingsManager::setWindowGeometry(const QByteArray &geom)
{
    m_settings->setValue("windowGeometry", geom);
}

QByteArray SettingsManager::windowGeometry() const
{
    return m_settings->value("windowGeometry").toByteArray();
}

void SettingsManager::setWindowState(const QByteArray &state)
{
    m_settings->setValue("windowState", state);
}

QByteArray SettingsManager::windowState() const
{
    return m_settings->value("windowState").toByteArray();
}

void SettingsManager::setPlaylist(const QStringList &paths)
{
    m_settings->setValue("playlist", paths);
}

QStringList SettingsManager::playlist() const
{
    return m_settings->value("playlist").toStringList();
}

void SettingsManager::setLastIndex(int index)
{
    m_settings->setValue("lastIndex", index);
}

int SettingsManager::lastIndex() const
{
    return m_settings->value("lastIndex", -1).toInt();
}

void SettingsManager::save()
{
    m_settings->sync();
}

void SettingsManager::load()
{
    m_settings->sync();
}
