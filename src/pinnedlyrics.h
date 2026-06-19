#ifndef ZAKIPLAYER_PINNEDLYRICS_H
#define ZAKIPLAYER_PINNEDLYRICS_H

#include <QWidget>
#include <QVector>
#include <QString>

#include "lyricsview.h"

class QPushButton;
class QLabel;

class PinnedLyricsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PinnedLyricsWindow(QWidget *parent = nullptr);

    void updateLines(const QVector<LyricsView::Line> &lines, int currentIndex);
    void setSource(const QString &title);
    void clearAll();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onCloseClicked();

private:
    QLabel *m_prevLabel = nullptr;
    QLabel *m_currLabel = nullptr;
    QLabel *m_nextLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_closeBtn = nullptr;

    QPoint m_dragOffset;
    bool m_hovered = false;
};

#endif // ZAKIPLAYER_PINNEDLYRICS_H
