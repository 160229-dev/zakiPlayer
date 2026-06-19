#include "playlistwidget.h"

#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QItemSelectionModel>

PlaylistWidget::PlaylistWidget(QWidget *parent)
    : QListView(parent)
    , m_model(nullptr)
{
    setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(true);
    setAlternatingRowColors(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
}

void PlaylistWidget::setModel(QAbstractItemModel *model)
{
    m_model = qobject_cast<PlaylistModel*>(model);
    QListView::setModel(model);
}

void PlaylistWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    auto *drag = new QDrag(this);
    auto *mime = new QMimeData();
    QModelIndexList idxs = selectionModel()->selectedIndexes();
    if (idxs.isEmpty()) {
        idxs << currentIndex();
    }
    if (idxs.isEmpty()) {
        return;
    }
    QList<QUrl> urls;
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    for (const QModelIndex &idx : idxs) {
        if (!idx.isValid()) continue;
        QUrl u = idx.data(PlaylistModel::UrlRole).toUrl();
        urls.append(u);
        stream << idx.row();
    }
    mime->setUrls(urls);
    mime->setData("application/x-zakiplayer-row", encoded);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction | Qt::CopyAction);
}

void PlaylistWidget::dropEvent(QDropEvent *event)
{
    if (!m_model) {
        QListView::dropEvent(event);
        return;
    }
    // 内部移动
    if (event->source() == this && event->mimeData()->hasFormat("application/x-zakiplayer-row")) {
        QList<int> rows;
        QByteArray encoded = event->mimeData()->data("application/x-zakiplayer-row");
        QDataStream stream(&encoded, QIODevice::ReadOnly);
        while (!stream.atEnd()) {
            int r; stream >> r;
            rows << r;
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        int targetRow = indexAt(event->position().toPoint()).row();
        if (targetRow < 0) targetRow = m_model->rowCount();
        for (int r : rows) {
            if (r < targetRow) targetRow--;
            m_model->move(r, targetRow);
        }
        event->acceptProposedAction();
        return;
    }
    // 外部文件拖入
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        m_model->addUrls(urls);
        event->acceptProposedAction();
        return;
    }
    QListView::dropEvent(event);
}

void PlaylistWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->source() == this) {
        event->acceptProposedAction();
    } else {
        QListView::dragEnterEvent(event);
    }
}

void PlaylistWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls() || event->source() == this) {
        event->acceptProposedAction();
    } else {
        QListView::dragMoveEvent(event);
    }
}

void PlaylistWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_model) return;
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) {
        emit playRequested(idx.row());
    }
}

void PlaylistWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        if (!m_model) return;
        QModelIndexList sel = selectionModel()->selectedRows();
        std::sort(sel.begin(), sel.end(), [](const QModelIndex &a, const QModelIndex &b){
            return a.row() > b.row();
        });
        for (const QModelIndex &idx : sel) {
            m_model->removeAt(idx.row());
        }
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QModelIndex idx = currentIndex();
        if (idx.isValid()) {
            emit playRequested(idx.row());
        }
        return;
    }
    QListView::keyPressEvent(event);
}
