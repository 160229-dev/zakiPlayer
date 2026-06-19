#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include <QListView>
#include <QStandardItemModel>
#include "playlistmodel.h"

class PlaylistWidget : public QListView
{
    Q_OBJECT
public:
    explicit PlaylistWidget(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model) override;
    PlaylistModel* playlistModel() const { return m_model; }

signals:
    void playRequested(int index);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    PlaylistModel *m_model;
};

#endif // PLAYLISTWIDGET_H
