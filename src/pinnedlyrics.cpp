#include "pinnedlyrics.h"
#include "lyricsview.h"

#include <QPainter>
#include <QPaintEvent>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QApplication>
#include <QScreen>
#include <QFont>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QHBoxLayout>

PinnedLyricsWindow::PinnedLyricsWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 142);
    setMouseTracking(true);

    // 标题（歌曲名）
    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("color: rgba(255,255,255,140); font-size: 10px; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setText(QString());
    m_titleLabel->setFixedHeight(14);

    // 三行歌词
    m_prevLabel = new QLabel(this);
    m_prevLabel->setStyleSheet("color: rgba(220,230,245,150); font-size: 12px; background: transparent;");
    m_prevLabel->setAlignment(Qt::AlignCenter);
    m_prevLabel->setText("");
    m_prevLabel->setFixedHeight(20);

    m_currLabel = new QLabel(this);
    m_currLabel->setStyleSheet("color: white; font-size: 17px; font-weight: bold; background: transparent;");
    m_currLabel->setAlignment(Qt::AlignCenter);
    m_currLabel->setText("");
    m_currLabel->setFixedHeight(32);

    m_nextLabel = new QLabel(this);
    m_nextLabel->setStyleSheet("color: rgba(220,230,245,150); font-size: 12px; background: transparent;");
    m_nextLabel->setAlignment(Qt::AlignCenter);
    m_nextLabel->setText("");
    m_nextLabel->setFixedHeight(20);

    // 关闭按钮：始终可见，hover 时变红色明显
    m_closeBtn = new QPushButton("×", this);
    m_closeBtn->setFixedSize(26, 26);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,28); color: rgba(255,255,255,220);"
        " border: none; font-size: 20px; font-weight: bold; padding: 0; margin: 0;"
        " border-radius: 13px; }"
        "QPushButton:hover { background: rgba(220,60,60,220); color: white; }");
    connect(m_closeBtn, &QPushButton::clicked, this, &PinnedLyricsWindow::onCloseClicked);

    // 用布局精确控制
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 32, 10, 10);   // 顶部留出 32px 给关闭按钮
    layout->setSpacing(2);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_prevLabel);
    layout->addWidget(m_currLabel);
    layout->addWidget(m_nextLabel);
    layout->addStretch(1);

    // 关闭按钮放右上角，往内挪避开圆角
    m_closeBtn->raise();
    m_closeBtn->move(width() - m_closeBtn->width() - 8, 8);

    // 初始位置：屏幕右下角
    QScreen *scr = QApplication::primaryScreen();
    if (scr) {
        const QRect g = scr->availableGeometry();
        move(g.right() - width() - 40, g.bottom() - height() - 40);
    }
}

void PinnedLyricsWindow::setSource(const QString &title)
{
    m_titleLabel->setText(title);
}

void PinnedLyricsWindow::updateLines(const QVector<LyricsView::Line> &lines, int currentIndex)
{
    auto textAt = [&](int i) -> QString {
        if (i < 0 || i >= lines.size()) return QString();
        return lines.at(i).text.trimmed();
    };
    const int cur = qBound(0, currentIndex, lines.size() - 1);
    m_prevLabel->setText(textAt(cur - 1));
    m_currLabel->setText(textAt(cur));
    m_nextLabel->setText(textAt(cur + 1));
}

void PinnedLyricsWindow::clearAll()
{
    m_prevLabel->clear();
    m_currLabel->clear();
    m_nextLabel->clear();
    m_titleLabel->clear();
}

void PinnedLyricsWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 背景：半透明深色圆角矩形
    QRectF r(0, 0, width(), height());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(15, 18, 28, 220));
    p.drawRoundedRect(r, 12, 12);

    // hover 灰色边框
    if (m_hovered) {
        QPen pen(QColor(255, 255, 255, 70), 1.5);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.75, 0.75, -0.75, -0.75), 12, 12);
    }
}

void PinnedLyricsWindow::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_hovered = true;
    m_closeBtn->raise();
    update();
}

void PinnedLyricsWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    // 不要因为鼠标在子控件上而误触：用全局鼠标位置判断
    const QPoint gp = QCursor::pos();
    if (QRect(mapToGlobal(QPoint(0, 0)), size()).contains(gp)) {
        return;  // 鼠标还在窗口内（可能在某个子按钮上）
    }
    m_hovered = false;
    update();
}

void PinnedLyricsWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 避免点到关闭按钮时触发拖动
        if (childAt(event->pos()) == m_closeBtn) {
            return;
        }
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PinnedLyricsWindow::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && !m_dragOffset.isNull()) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
    }
}

void PinnedLyricsWindow::onCloseClicked()
{
    hide();
}
