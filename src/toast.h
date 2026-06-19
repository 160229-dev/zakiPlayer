#ifndef ZAKIPLAYER_TOAST_H
#define ZAKIPLAYER_TOAST_H

#include <QWidget>

// 屏幕顶部悬浮通知：歌曲切换时短暂显示，淡入淡出
class ToastWindow : public QWidget
{
    Q_OBJECT
public:
    // 静态便捷方法：在主屏顶部居中显示一条 toast，约 2 秒后自动淡出消失
    static void showMessage(const QString &title, const QString &subtitle = QString());

    explicit ToastWindow(QWidget *parent = nullptr);
    ~ToastWindow() override;

    void showAtTop(const QString &title, const QString &subtitle = QString());

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_subtitle;
};

#endif // ZAKIPLAYER_TOAST_H
