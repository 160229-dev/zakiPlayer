#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QUrl>
#include <QList>
#include <QMimeData>

class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
public:
    enum Roles {
        UrlRole = Qt::UserRole + 1,
        FileNameRole,
        FilePathRole,
        TitleRole,
        ArtistRole,
        DurationRole,
        IsCurrentRole,
    };
    Q_ENUM(Roles)

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    // 操作接口
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void addUrl(const QUrl &url);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE QUrl urlAt(int index) const;
    Q_INVOKABLE QString fileNameAt(int index) const;

    int count() const { return m_urls.size(); }
    QList<QUrl> urls() const { return m_urls; }

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    QUrl currentUrl() const;

    // 随机播放辅助
    int nextIndexSequential() const;
    int nextIndexRepeatOne() const;
    int nextIndexRepeatAll() const;
    int nextIndexShuffle(int previousIndex = -1) const;

    QStringList toStringList() const;
    void fromStringList(const QStringList &list);

signals:
    void countChanged();
    void currentIndexChanged();
    void playIndexRequested(int index);

private:
    QList<QUrl> m_urls;
    int m_currentIndex;
};

#endif // PLAYLISTMODEL_H
