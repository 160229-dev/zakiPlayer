#include "lyricsview.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <QPushButton>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QPainterPath>
#include <QPolygon>
#include <QSize>
#include <algorithm>
#include <QtMath>

// 绘制"工字钉"图标：和主窗口标题栏同款（白金属，三段圆盘头 + 梯形针身）
// crossed = 是否在图标上加斜杠（一般 lyric 按钮为 false）
static QIcon makePinIcon(int size, bool crossed = false)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal w = size;
    const qreal h = size;
    const qreal cx = w / 2.0;
    const qreal cy = h / 2.0;

    // 整体旋转 -35°（顺时针），让针尖指向右
    p.translate(cx, cy);
    p.rotate(-35);
    p.translate(-cx, -cy);

    // 三段圆角矩形（工字头），间距拉开，上下两片明显分开
    auto drawBar = [&](qreal y, qreal bw, qreal bh) {
        QRectF r(cx - bw / 2, cy + y, bw, bh);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(245, 248, 252, 255));   // 主体白
        p.drawRoundedRect(r, bh * 0.45, bh * 0.45);
    };
    drawBar(-h * 0.34, w * 0.62, h * 0.10);   // 顶盘
    drawBar(-h * 0.13, w * 0.34, h * 0.07);   // 中横（窄）
    drawBar(+h * 0.06, w * 0.58, h * 0.09);   // 底盘

    // 针身：梯形（根宽 → 尖）
    QPolygonF needle;
    needle << QPointF(cx - w * 0.05, cy + h * 0.15)
           << QPointF(cx + w * 0.05, cy + h * 0.15)
           << QPointF(cx + w * 0.012, cy + h * 0.42)
           << QPointF(cx - w * 0.012, cy + h * 0.42);
    p.setBrush(QColor(220, 226, 235, 255));
    p.drawPolygon(needle);

    // 针身中央一道浅色高光线
    p.setPen(QPen(QColor(255, 255, 255, 200), 0.8));
    p.drawLine(QPointF(cx, cy + h * 0.20),
               QPointF(cx, cy + h * 0.38));

    // 划掉状态：左上 → 右下 一道红线
    if (crossed) {
        const qreal pad = size * 0.08;
        QPen slashPen(QColor(255, 90, 90, 235),
                      qMax(2.0, size * 0.10),
                      Qt::SolidLine, Qt::RoundCap);
        p.setPen(slashPen);
        p.drawLine(QPointF(pad, pad),
                   QPointF(size - pad, size - pad));
    }
    p.end();
    return QIcon(pm);
}

// 绘制"全屏外箭头"图标：四角各一个 L 形外角
static QIcon makeFullscreenIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(255, 255, 255, 230), qMax(1.4, size * 0.10),
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const qreal m = size * 0.18;     // 边距
    const qreal a = size * 0.22;     // 短边长
    auto corner = [&](const QPointF &c, int dx, int dy) {
        // 从角落向外延伸：水平 a，垂直 a
        p.drawLine(QPointF(c.x(), c.y()),
                   QPointF(c.x() + dx * a, c.y()));
        p.drawLine(QPointF(c.x(), c.y()),
                   QPointF(c.x(), c.y() + dy * a));
    };
    corner(QPointF(m, m), +1, +1);
    corner(QPointF(size - m, m), -1, +1);
    corner(QPointF(m, size - m), +1, -1);
    corner(QPointF(size - m, size - m), -1, -1);
    p.end();
    return QIcon(pm);
}

LyricsView::LyricsView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setMouseTracking(true);

    // 固定按钮
    m_pinBtn = new QPushButton(this);
    m_pinBtn->setIcon(makePinIcon(22));
    m_pinBtn->setIconSize(QSize(22, 22));
    m_pinBtn->setFixedSize(34, 28);
    m_pinBtn->setToolTip(tr("固定到桌面"));
    m_pinBtn->setCursor(Qt::PointingHandCursor);
    m_pinBtn->setStyleSheet(
        "QPushButton { background: rgba(40,46,60,200); border: 1px solid rgba(255,255,255,40);"
        " border-radius: 6px; }"
        "QPushButton:hover { background: rgba(70,80,110,230); border-color: rgba(255,255,255,90); }");
    m_pinBtn->hide();
    connect(m_pinBtn, &QPushButton::clicked, this, &LyricsView::onPinClicked);

    // 全屏按钮（和固定按钮同一尺寸）
    m_fsBtn = new QPushButton(this);
    m_fsBtn->setIcon(makeFullscreenIcon(22));
    m_fsBtn->setIconSize(QSize(22, 22));
    m_fsBtn->setFixedSize(34, 28);
    m_fsBtn->setToolTip(tr("全屏歌词"));
    m_fsBtn->setCursor(Qt::PointingHandCursor);
    m_fsBtn->setStyleSheet(
        "QPushButton { background: rgba(40,46,60,200); border: 1px solid rgba(255,255,255,40);"
        " border-radius: 6px; }"
        "QPushButton:hover { background: rgba(70,80,110,230); border-color: rgba(255,255,255,90); }");
    m_fsBtn->hide();
    connect(m_fsBtn, &QPushButton::clicked, this, &LyricsView::onFullscreenClicked);
}

QVector<LyricsView::Line> LyricsView::parseLrcFile(const QString &lrcPath)
{
    QVector<Line> result;
    QFile f(lrcPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    // LRC 标签格式：[mm:ss.xx] 或 [mm:ss.xxx]
    static const QRegularExpression re(R"(\[(\d{1,2}):(\d{1,2})(?:[.:](\d{1,3}))?\])");
    while (!in.atEnd()) {
        const QString line = in.readLine();
        // 一行可能含多个时间戳
        QRegularExpressionMatchIterator it = re.globalMatch(line);
        QString text;
        int lastEnd = 0;
        bool found = false;
        while (it.hasNext()) {
            auto m = it.next();
            const qint64 mm = m.captured(1).toLongLong();
            const qint64 ss = m.captured(2).toLongLong();
            const QString frac = m.captured(3);
            qint64 ms = mm * 60 * 1000 + ss * 1000;
            if (!frac.isEmpty()) {
                // 小数部分按位数补齐
                if (frac.size() == 1) ms += frac.toLongLong() * 100;
                else if (frac.size() == 2) ms += frac.toLongLong() * 10;
                else if (frac.size() >= 3) ms += frac.left(3).toLongLong();
            }
            Line ln;
            ln.timeMs = ms;
            text = line.mid(m.capturedEnd());
            ln.text = text;
            result.append(ln);
            lastEnd = m.capturedEnd();
            found = true;
            Q_UNUSED(lastEnd);
        }
        if (!found) continue;
    }
    std::sort(result.begin(), result.end(),
              [](const Line &a, const Line &b) { return a.timeMs < b.timeMs; });
    return result;
}

void LyricsView::setLines(const QVector<Line> &lines)
{
    m_lines = lines;
    m_currentIndex = -1;
    setHasLyrics(!lines.isEmpty());
    update();
}

void LyricsView::clearLines()
{
    m_lines.clear();
    m_currentIndex = -1;
    m_positionMs = 0;
    setHasLyrics(false);
    update();
}

void LyricsView::setHasLyrics(bool has)
{
    m_hasLyrics = has;
    // 没歌词时把按钮也藏起来
    if (!m_hasLyrics) {
        if (m_pinBtn) m_pinBtn->hide();
        if (m_fsBtn) m_fsBtn->hide();
    }
}

void LyricsView::setPosition(qint64 ms)
{
    m_positionMs = ms;
    int idx = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i).timeMs <= ms) {
            idx = i;
        } else {
            break;
        }
    }
    if (idx != m_currentIndex) {
        m_currentIndex = idx;
        update();
    }
}

void LyricsView::syncPosition(qint64 ms)
{
    m_positionMs = ms;
    int idx = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i).timeMs <= ms) {
            idx = i;
        } else {
            break;
        }
    }
    if (idx != m_currentIndex) {
        m_currentIndex = idx;
    }
    update();
}

void LyricsView::updateButtonGeometry()
{
    if (!m_pinBtn || !m_fsBtn) return;
    const int r = 4;
    m_fsBtn->move(width() - m_fsBtn->width() - r, r);
    m_pinBtn->move(width() - m_pinBtn->width() - m_fsBtn->width() - r - 4, r);
}

void LyricsView::onPinClicked()
{
    emit pinRequested();
}

void LyricsView::onFullscreenClicked()
{
    emit fullscreenRequested();
}

void LyricsView::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_hovered = true;
    if (m_hasLyrics) {
        if (m_pinBtn) m_pinBtn->show();
        if (m_fsBtn) m_fsBtn->show();
    }
    updateButtonGeometry();
}

void LyricsView::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hovered = false;
    if (m_pinBtn) m_pinBtn->hide();
    if (m_fsBtn) m_fsBtn->hide();
}

void LyricsView::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_hovered) {
        m_hovered = true;
        if (m_hasLyrics) {
            if (m_pinBtn) m_pinBtn->show();
            if (m_fsBtn) m_fsBtn->show();
        }
        updateButtonGeometry();
    }
}

void LyricsView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    updateButtonGeometry();
}

void LyricsView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();

    if (m_lines.isEmpty()) {
        p.setPen(QColor(255, 255, 255, 90));
        QFont f = p.font();
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, tr("暂无歌词"));
        return;
    }

    // 当前行号：未到第一行时显示为空或第一行预亮
    const int cur = qBound(0, m_currentIndex, m_lines.size() - 1);

    // 行高
    const int lineH = 28;          // 基础行高
    const int fontActive = 18;      // 当前行字号
    const int fontStep = 2;         // 距当前行每远一行的字号衰减

    // 计算中心 Y（当前行水平居中）
    const qreal midY = h / 2.0;

    // 上下各显示的"最大行数"
    const int maxOffset = 5;

    for (int offset = -maxOffset; offset <= maxOffset; ++offset) {
        const int idx = cur + offset;
        if (idx < 0 || idx >= m_lines.size()) continue;

        const Line &line = m_lines.at(idx);
        if (line.text.trimmed().isEmpty()) continue;

        // 字号：当前行最大，越远越小
        const int fontSize = fontActive - qAbs(offset) * fontStep;
        const int fsz = qMax(10, fontSize);

        // 透明度：当前行最亮
        const int alpha = 255 - qAbs(offset) * 38;
        const int a = qBound(50, alpha, 255);

        // 颜色：当前行纯白，其它偏冷
        QColor col;
        if (offset == 0) {
            col = QColor(255, 255, 255, 255);
        } else {
            col = QColor(220, 230, 245, a);
        }

        QFont f = p.font();
        f.setPixelSize(fsz);
        f.setBold(offset == 0);
        p.setFont(f);
        p.setPen(col);

        // 行的 y 坐标
        const qreal y = midY + offset * lineH - fsz / 2.0;

        // 居中绘制
        QFontMetrics fm(f);
        QRect textRect(0, int(y), w, fm.height());
        p.drawText(textRect, Qt::AlignCenter, line.text);
    }
}
