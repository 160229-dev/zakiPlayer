#ifndef ZAKIPLAYER_LYRICSVIEW_H
#define ZAKIPLAYER_LYRICSVIEW_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QPoint>

class QPushButton;

class LyricsView : public QWidget
{
    Q_OBJECT
public:
    struct Line {
        qint64 timeMs = 0;     // 时间戳（毫秒）
        QString text;          // 歌词文本
    };

    explicit LyricsView(QWidget *parent = nullptr);

    void setLines(const QVector<Line> &lines);
    void clearLines();
    bool hasLines() const { return !m_lines.isEmpty(); }
    int lineCount() const { return m_lines.size(); }

    // 解析 LRC 文件
    static QVector<Line> parseLrcFile(const QString &lrcPath);

    // 传入当前播放位置（毫秒），更新当前行
    void setPosition(qint64 ms);

    // 暴露给固定/全屏窗口读取
    const QVector<Line> lines() const { return m_lines; }
    int currentIndex() const { return m_currentIndex; }

    // 是否有可显示的歌词行（用于控制按钮显隐 & 防止空歌词打开窗口崩溃）
    void setHasLyrics(bool has);

    // 立即同步位置（外部窗口用）
    void syncPosition(qint64 ms);

signals:
    void pinRequested();
    void fullscreenRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onPinClicked();
    void onFullscreenClicked();

private:
    void updateButtonGeometry();

    QVector<Line> m_lines;
    int m_currentIndex = -1;   // 当前播放的行（-1 表示还没到第一行）
    qint64 m_positionMs = 0;
    bool m_hasLyrics = false;  // 是否有可显示的歌词

    QPushButton *m_pinBtn = nullptr;
    QPushButton *m_fsBtn = nullptr;
    bool m_hovered = false;
};

#endif // ZAKIPLAYER_LYRICSVIEW_H
