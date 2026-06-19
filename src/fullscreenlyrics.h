#ifndef ZAKIPLAYER_FULLSCREENLYRICS_H
#define ZAKIPLAYER_FULLSCREENLYRICS_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QPoint>

#include "lyricsview.h"

class QPushButton;
class QLabel;

class FullscreenLyricsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FullscreenLyricsWindow(QWidget *parent = nullptr);

    void updateLines(const QVector<LyricsView::Line> &lines, int currentIndex);
    void setSource(const QString &title);
    void clearAll();
    void enterFullScreenMode();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onCloseClicked();

private:
    QVector<LyricsView::Line> m_lines;
    int m_currentIndex = -1;
    QString m_title;

    QPushButton *m_closeBtn = nullptr;
    bool m_hovered = false;
};

#endif // ZAKIPLAYER_FULLSCREENLYRICS_H
