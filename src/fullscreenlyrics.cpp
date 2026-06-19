#include "fullscreenlyrics.h"
#include "lyricsview.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QTimer>
#include <algorithm>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

FullscreenLyricsWindow::FullscreenLyricsWindow(QWidget *parent)
    : QWidget(parent)
{
    // 顶级窗口，Frameless + StaysOnTop；不要 Qt::Tool（那会让窗口可拖/有任务栏图标）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    // 关闭按钮（hover 时显示，加大到 50×50）
    m_closeBtn = new QPushButton("×", this);
    m_closeBtn->setFixedSize(50, 50);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,40); color: white;"
        " border-radius: 25px; border: none; font-size: 32px; font-weight: bold;"
        " padding: 0; margin: 0; }"
        "QPushButton:hover { background: rgba(220,60,60,220); }");
    m_closeBtn->hide();
    connect(m_closeBtn, &QPushButton::clicked, this, &FullscreenLyricsWindow::onCloseClicked);
}

void FullscreenLyricsWindow::setSource(const QString &title)
{
    m_title = title;
    update();
}

void FullscreenLyricsWindow::updateLines(const QVector<LyricsView::Line> &lines, int currentIndex)
{
    m_lines = lines;
    m_currentIndex = currentIndex;
    update();
}

void FullscreenLyricsWindow::clearAll()
{
    m_lines.clear();
    m_currentIndex = -1;
    m_title.clear();
    update();
}

void FullscreenLyricsWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();

    // === 美丽背景：深紫蓝径向渐变 + 顶部柔光 + 散落的小亮点 ===
    // 1) 整体垂直渐变：深蓝紫 → 深夜黑
    QLinearGradient vgrad(0, 0, 0, h);
    vgrad.setColorAt(0.0, QColor(28, 22, 58, 255));   // 顶部深紫
    vgrad.setColorAt(0.45, QColor(18, 16, 42, 255));
    vgrad.setColorAt(1.0, QColor(6, 6, 16, 255));     // 底部接近全黑
    p.fillRect(rect(), vgrad);

    // 2) 顶部中央柔光（像月光）
    QRadialGradient moon(QPointF(w * 0.5, h * 0.18), qMax(w, h) * 0.55);
    moon.setColorAt(0.0, QColor(150, 160, 230, 70));
    moon.setColorAt(0.35, QColor(100, 120, 200, 35));
    moon.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), moon);

    // 3) 角落两点装饰光晕
    QRadialGradient c1(QPointF(0, h), h * 0.7);
    c1.setColorAt(0.0, QColor(120, 60, 180, 60));
    c1.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), c1);
    QRadialGradient c2(QPointF(w, 0), h * 0.6);
    c2.setColorAt(0.0, QColor(60, 130, 200, 50));
    c2.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), c2);

    // 4) 散落的"星辰"（用伪随机保证每次一致）
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 60; ++i) {
        const int sx = (i * 137) % w;
        const int sy = (i * 211) % h;
        const int sa = 60 + ((i * 53) % 90);
        const int sr = ((i * 17) % 3) + 1;
        p.setBrush(QColor(220, 230, 255, sa));
        p.drawEllipse(QPoint(sx, sy), sr, sr);
    }

    // 当前歌词背后柔光带（让文字有"发光"感）
    if (!m_lines.isEmpty() && m_currentIndex >= 0 && m_currentIndex < m_lines.size()) {
        QRadialGradient halo(QPointF(w * 0.5, h * 0.5), h * 0.35);
        halo.setColorAt(0.0, QColor(140, 180, 255, 55));
        halo.setColorAt(0.6, QColor(120, 100, 200, 25));
        halo.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), halo);
    }

    // 标题
    if (!m_title.isEmpty()) {
        QFont fT = font();
        fT.setPointSize(11);
        p.setFont(fT);
        p.setPen(QColor(255, 255, 255, 130));
        p.drawText(QRect(0, 24, w, 20), Qt::AlignCenter, m_title);
    }

    if (m_lines.isEmpty()) {
        QFont f = font();
        f.setPointSize(20);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 100));
        p.drawText(rect(), Qt::AlignCenter, tr("暂无歌词"));
        return;
    }

    // 上下各显示 7 行
    const int lineH = 64;
    const int fontActive = 28;
    const int fontStep = 2;
    const int maxOffset = 7;
    const qreal midY = h / 2.0;

    for (int offset = -maxOffset; offset <= maxOffset; ++offset) {
        const int idx = m_currentIndex + offset;
        if (idx < 0 || idx >= m_lines.size()) continue;
        const QString t = m_lines.at(idx).text.trimmed();
        if (t.isEmpty()) continue;

        const int fsz = qMax(10, fontActive - qAbs(offset) * fontStep);
        QFont f = font();
        f.setPixelSize(fsz);
        f.setBold(offset == 0);
        p.setFont(f);

        int alpha;
        QColor col;
        if (offset == 0) {
            alpha = 255;
            col = QColor(255, 255, 255, 255);
        } else if (offset < 0) {
            alpha = qBound(40, 180 - qAbs(offset) * 18, 200);
            col = QColor(200, 210, 230, alpha);
        } else {
            alpha = qBound(80, 220 - qAbs(offset) * 18, 230);
            col = QColor(230, 240, 255, alpha);
        }
        p.setPen(col);

        const qreal y = midY + offset * lineH - fsz / 2.0;
        QFontMetrics fm(f);
        QRect rc(0, int(y), w, fm.height());
        p.drawText(rc, Qt::AlignCenter, t);
    }
}

void FullscreenLyricsWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void FullscreenLyricsWindow::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    // 全屏状态下禁止拖动窗口（m_dragOffset 已废弃）
}

void FullscreenLyricsWindow::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    // 全屏状态下禁止拖动窗口
}

void FullscreenLyricsWindow::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_hovered = true;
    m_closeBtn->show();
    m_closeBtn->raise();
    m_closeBtn->move(width() - m_closeBtn->width() - 24, 24);
    update();
}

void FullscreenLyricsWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    // 即使鼠标移到关闭按钮上，leave 也不要急着关掉
    const QPoint gp = QCursor::pos();
    if (QRect(mapToGlobal(QPoint(0, 0)), size()).contains(gp)) {
        return;
    }
    m_hovered = false;
    m_closeBtn->hide();
    update();
}

void FullscreenLyricsWindow::onCloseClicked()
{
    hide();
}

void FullscreenLyricsWindow::enterFullScreenMode()
{
#ifdef Q_OS_WIN
    // 走 Windows API 强制把窗口拉到覆盖整个虚拟屏幕（包含任务栏）
    // 不依赖 Qt::WindowFullScreen（那个在 DWM 下偶尔会失效）
    if (!winId()) {
        // 窗口还没创建 native handle，先 show 一遍让它创建
        setAttribute(Qt::WA_WState_Hidden, false);
    }
    HWND hwnd = (HWND)winId();
    if (!hwnd) {
        // 极端兜底
        show();
        hwnd = (HWND)winId();
    }
    if (hwnd) {
        // 让窗口直接成为 POPUP 顶级（无标题栏 / 无系统菜单 / 不可拖动 / 不可调整）
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME | WS_SYSMENU
                   | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        style |= WS_POPUP | WS_VISIBLE;
        SetWindowLong(hwnd, GWL_STYLE, style);

        // 扩展样式：TOPMOST + TOOLWINDOW（不显示在任务栏 / Alt-Tab）
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        exStyle &= ~WS_EX_APPWINDOW;     // 不在任务栏占位
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

        // 取整屏像素（不是 Qt 的 availableGeometry——那个会扣掉任务栏）
        const int fullW = GetSystemMetrics(SM_CXSCREEN);
        const int fullH = GetSystemMetrics(SM_CYSCREEN);
        // 放到最顶层并占满整屏
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, fullW, fullH,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOZORDER);
    }
#else
    showFullScreen();
#endif
    raise();
    activateWindow();
}

void FullscreenLyricsWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 每次 show 都用 Windows API 强制覆盖整屏
    QTimer::singleShot(0, this, &FullscreenLyricsWindow::enterFullScreenMode);
}

bool FullscreenLyricsWindow::nativeEvent(const QByteArray &eventType,
                                         void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" ||
        eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        switch (msg->message) {
        case WM_NCLBUTTONDOWN:           // 在标题栏上按下鼠标
        case WM_NCLBUTTONDBLCLK:         // 双击标题栏
        case WM_NCRBUTTONDOWN:           // 右键标题栏
            *result = 0;
            return true;                // 吞掉，禁止系统拖动
        case WM_SYSCOMMAND: {
            // 系统命令：移动 / 调整大小 / 最大化 / 最小化
            int cmd = (int)(msg->wParam & 0xFFF0);
            if (cmd == SC_MOVE || cmd == SC_SIZE ||
                cmd == SC_MAXIMIZE || cmd == SC_MINIMIZE ||
                cmd == SC_RESTORE) {
                *result = 0;
                return true;            // 吞掉
            }
            break;
        }
        case WM_NCMOUSELEAVE:            // 鼠标离开非客户区
            *result = 0;
            return true;
        default:
            break;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
