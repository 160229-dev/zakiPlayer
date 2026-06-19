#include "playlistmodel.h"
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMimeData>
#include <QUrl>
#include <QSet>
#include <QRandomGenerator>
#include <QDir>
#include <QtDebug>

namespace {
const QStringList kSupportedAudioSuffix = {
    "mp3", "wav", "flac", "aac", "m4a", "ogg", "oga", "opus",
    "wma", "aiff", "aif", "ape", "mp2", "amr", "ac3", "alac"
};
const QStringList kSupportedVideoSuffix = {
    "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "mpg", "mpeg",
    "m4v", "3gp", "ts", "vob", "rm", "rmvb", "asf"
};

bool isMediaFile(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    const QString suffix = info.suffix().toLower();
    return kSupportedAudioSuffix.contains(suffix) || kSupportedVideoSuffix.contains(suffix);
}

void collectMediaInDir(const QString &dirPath, QList<QUrl> &out)
{
    QDir dir(dirPath);
    if (!dir.exists()) return;
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &info : entries) {
        if (isMediaFile(info.absoluteFilePath())) {
            out.append(QUrl::fromLocalFile(info.absoluteFilePath()));
        }
    }
}
}

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_currentIndex(-1)
{
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_urls.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_urls.size()) {
        return {};
    }
    const QUrl &url = m_urls.at(index.row());
    const QFileInfo info(url.toLocalFile());
    switch (role) {
    case Qt::DisplayRole:
    case FileNameRole:
        return info.fileName().isEmpty() ? url.toString() : info.fileName();
    case FilePathRole:
        return url.toLocalFile();
    case UrlRole:
        return url;
    case TitleRole:
        return info.completeBaseName();
    case ArtistRole:
        return QString();
    case IsCurrentRole:
        return index.row() == m_currentIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {
        { UrlRole, "url" },
        { FileNameRole, "fileName" },
        { FilePathRole, "filePath" },
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { IsCurrentRole, "isCurrent" }
    };
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid()) {
        return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }
    return defaultFlags | Qt::ItemIsDropEnabled;
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

QStringList PlaylistModel::mimeTypes() const
{
    return { "text/uri-list", "application/x-zakiplayer-row" };
}

QMimeData *PlaylistModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mime = new QMimeData();
    QList<QUrl> urls;
    for (const QModelIndex &idx : indexes) {
        if (idx.isValid() && idx.row() >= 0 && idx.row() < m_urls.size()) {
            urls.append(m_urls.at(idx.row()));
        }
    }
    mime->setUrls(urls);
    return mime;
}

bool PlaylistModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column);
    Q_UNUSED(parent);
    if (action == Qt::IgnoreAction) return true;

    // 内部移动
    if (data->hasFormat("application/x-zakiplayer-row")) {
        return true; // 由 move() 处理
    }

    if (data->hasUrls()) {
        int insertRow = (row < 0) ? m_urls.size() : row;
        QList<QUrl> newUrls;
        for (const QUrl &url : data->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                QFileInfo info(path);
                if (info.isDir()) {
                    collectMediaInDir(path, newUrls);
                } else if (isMediaFile(path)) {
                    newUrls.append(url);
                }
            }
        }
        if (newUrls.isEmpty()) return false;

        beginInsertRows(QModelIndex(), insertRow, insertRow + newUrls.size() - 1);
        for (int i = 0; i < newUrls.size(); ++i) {
            m_urls.insert(insertRow + i, newUrls.at(i));
        }
        endInsertRows();
        emit countChanged();
        return true;
    }
    return false;
}

void PlaylistModel::addUrls(const QList<QUrl> &urls)
{
    QList<QUrl> valid;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) continue;
        QString path = url.toLocalFile();
        QFileInfo info(path);
        if (info.isDir()) {
            collectMediaInDir(path, valid);
        } else if (isMediaFile(path)) {
            valid.append(url);
        }
    }
    if (valid.isEmpty()) return;

    // 去重：已存在的 URL 不再添加
    QSet<QString> existing;
    for (const QUrl &u : m_urls) {
        existing.insert(u.toLocalFile());
    }
    QList<QUrl> toAdd;
    for (const QUrl &u : valid) {
        const QString p = u.toLocalFile();
        if (!existing.contains(p)) {
            toAdd.append(u);
            existing.insert(p);
        }
    }
    if (toAdd.isEmpty()) return;

    int start = m_urls.size();
    beginInsertRows(QModelIndex(), start, start + toAdd.size() - 1);
    m_urls.append(toAdd);
    endInsertRows();
    emit countChanged();
}

void PlaylistModel::addUrl(const QUrl &url)
{
    addUrls({ url });
}

void PlaylistModel::removeAt(int index)
{
    if (index < 0 || index >= m_urls.size()) return;
    beginRemoveRows(QModelIndex(), index, index);
    m_urls.removeAt(index);
    endRemoveRows();
    if (m_currentIndex == index) {
        m_currentIndex = -1;
        emit currentIndexChanged();
    } else if (m_currentIndex > index) {
        m_currentIndex--;
        emit currentIndexChanged();
    }
    emit countChanged();
}

void PlaylistModel::clear()
{
    if (m_urls.isEmpty()) return;
    beginResetModel();
    m_urls.clear();
    m_currentIndex = -1;
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
}

void PlaylistModel::move(int from, int to)
{
    if (from < 0 || from >= m_urls.size()) return;
    if (to < 0) to = 0;
    if (to >= m_urls.size()) to = m_urls.size() - 1;
    if (from == to) return;

    // QAbstractItemModel 的 moveRows 语义：先 beginMoveRows
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    QUrl url = m_urls.takeAt(from);
    m_urls.insert(to, url);
    endMoveRows();

    if (m_currentIndex == from) {
        m_currentIndex = to;
    } else if (m_currentIndex > from && m_currentIndex <= to) {
        m_currentIndex--;
    } else if (m_currentIndex < from && m_currentIndex >= to) {
        m_currentIndex++;
    }
    emit currentIndexChanged();
    // 通知视图重新请求
    emit dataChanged(index(0), index(m_urls.size() - 1), { IsCurrentRole });
}

QUrl PlaylistModel::urlAt(int index) const
{
    if (index < 0 || index >= m_urls.size()) return {};
    return m_urls.at(index);
}

QString PlaylistModel::fileNameAt(int index) const
{
    if (index < 0 || index >= m_urls.size()) return {};
    QFileInfo info(m_urls.at(index).toLocalFile());
    return info.fileName();
}

void PlaylistModel::setCurrentIndex(int index)
{
    if (index == m_currentIndex) return;
    int old = m_currentIndex;
    m_currentIndex = index;
    if (old >= 0 && old < m_urls.size()) {
        emit dataChanged(this->index(old), this->index(old), { IsCurrentRole });
    }
    if (m_currentIndex >= 0 && m_currentIndex < m_urls.size()) {
        emit dataChanged(this->index(m_currentIndex), this->index(m_currentIndex), { IsCurrentRole });
    }
    emit currentIndexChanged();
}

QUrl PlaylistModel::currentUrl() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_urls.size()) return {};
    return m_urls.at(m_currentIndex);
}

int PlaylistModel::nextIndexSequential() const
{
    if (m_urls.isEmpty()) return -1;
    if (m_currentIndex + 1 < m_urls.size()) return m_currentIndex + 1;
    return -1; // 列表末尾，停止
}

int PlaylistModel::nextIndexRepeatOne() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_urls.size()) return -1;
    return m_currentIndex;
}

int PlaylistModel::nextIndexRepeatAll() const
{
    if (m_urls.isEmpty()) return -1;
    if (m_currentIndex + 1 < m_urls.size()) return m_currentIndex + 1;
    return 0;
}

int PlaylistModel::nextIndexShuffle(int previousIndex) const
{
    if (m_urls.isEmpty()) return -1;
    if (m_urls.size() == 1) return 0;
    int idx;
    do {
        idx = QRandomGenerator::global()->bounded(m_urls.size());
    } while (idx == m_currentIndex || idx == previousIndex);
    Q_UNUSED(previousIndex);
    return idx;
}

QStringList PlaylistModel::toStringList() const
{
    QStringList list;
    list.reserve(m_urls.size());
    for (const QUrl &u : m_urls) {
        list << u.toString();
    }
    return list;
}

void PlaylistModel::fromStringList(const QStringList &list)
{
    beginResetModel();
    m_urls.clear();
    m_currentIndex = -1;
    for (const QString &s : list) {
        QUrl url(s);
        if (url.isLocalFile()) {
            QString path = url.toLocalFile();
            if (isMediaFile(path)) {
                m_urls.append(url);
            }
        }
    }
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
}
