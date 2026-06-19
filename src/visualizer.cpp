#include "visualizer.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QtMath>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>

Visualizer::Visualizer(QWidget *parent)
    : QWidget(parent)
    , m_barCount(48)
    , m_active(true)
    , m_glowIntensity(0.6)
    , m_glowAnim(nullptr)
    , m_mode(BarMode)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumHeight(80);
    setMouseTracking(true);
    m_displayBands.fill(0.0, m_barCount);

    // 当数据未更新时，缓慢衰减 + 波形历史滚动
    auto *decayTimer = new QTimer(this);
    decayTimer->setInterval(40);
    connect(decayTimer, &QTimer::timeout, this, [this]() {
        if (m_bands.size() != m_displayBands.size()) {
            m_displayBands.fill(0.0, m_barCount);
        }
        for (int i = 0; i < m_displayBands.size(); ++i) {
            const qreal target = m_bands.value(i, 0.0);
            const qreal cur = m_displayBands[i];
            const qreal rate = (target > cur) ? 0.4 : 0.18;
            m_displayBands[i] = cur + (target - cur) * rate;
        }
        // 波形模式：把当前所有 band 的均值压入历史，使用包络跟随器
        if (m_mode == WaveMode) {
            qreal sum = 0;
            qreal peak = 0;
            for (qreal v : m_displayBands) {
                sum += v;
                if (v > peak) peak = v;
            }
            qreal mean = sum / qMax(1, m_displayBands.size());
            // 综合 peak 和 mean，更接近真实音频
            qreal target = peak * 0.6 + mean * 0.4;
            target = qBound(0.0, target, 1.0);
            // 包络跟随器：快上慢下
            if (target > m_envelope) {
                m_envelope = m_envelope * 0.35 + target * 0.65;
            } else {
                m_envelope = m_envelope * 0.88 + target * 0.12;
            }
            m_envelope = qBound(0.0, m_envelope, 1.0);
            m_history.prepend(m_envelope);
            while (m_history.size() > 192) m_history.removeLast();
        }
        update();
    });
    decayTimer->start();
}

QSize Visualizer::sizeHint() const
{
    return QSize(600, 220);
}

QSize Visualizer::minimumSizeHint() const
{
    return QSize(300, 120);
}

void Visualizer::setBands(const QVector<qreal> &bands)
{
    m_bands = bands;
    if (m_bands.size() != m_barCount) {
        m_barCount = qMax(8, m_bands.size());
    }
    update();
}

void Visualizer::setActive(bool active)
{
    m_active = active;
    update();
}

void Visualizer::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    if (m_mode == WaveMode) {
        m_history.clear();
        m_history.resize(96, 0.0);
        m_envelope = 0.0;
    }
    update();
}

void Visualizer::toggleMode()
{
    setMode(m_mode == BarMode ? WaveMode : BarMode);
}

QString Visualizer::modeName(Mode m)
{
    return m == BarMode ? QStringLiteral("条形") : QStringLiteral("波形");
}

void Visualizer::setGlowIntensity(qreal v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v + 1.0, m_glowIntensity + 1.0)) return;
    m_glowIntensity = v;
    update();
}

QColor Visualizer::colorForLevel(qreal level) const
{
    // level: 0.0 ~ 1.0
    // 底部绿 -> 中部黄 -> 顶部红
    level = qBound(0.0, level, 1.0);
    if (level < 0.5) {
        // 绿到黄
        qreal t = level / 0.5;
        QColor c(
            int(80 + (255 - 80) * t),
            int(220 - (220 - 200) * t * 0.1),
            int(120 - 120 * t)
        );
        c.setAlpha(255);
        return c;
    } else {
        // 黄到红
        qreal t = (level - 0.5) / 0.5;
        QColor c(
            255,
            int(220 - 220 * t),
            int(60 * (1.0 - t))
        );
        c.setAlpha(255);
        return c;
    }
}

void Visualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void Visualizer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setAttribute(Qt::WA_TranslucentBackground, false);
    QActionGroup grp(&menu);
    grp.setExclusive(true);
    auto *barAct = menu.addAction(modeName(BarMode));
    barAct->setCheckable(true);
    barAct->setChecked(m_mode == BarMode);
    auto *waveAct = menu.addAction(modeName(WaveMode));
    waveAct->setCheckable(true);
    waveAct->setChecked(m_mode == WaveMode);
    grp.addAction(barAct);
    grp.addAction(waveAct);
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == barAct) setMode(BarMode);
    else if (chosen == waveAct) setMode(WaveMode);
}

void Visualizer::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    toggleMode();
}

void Visualizer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // 背景渐变（弱）
    QLinearGradient bg(0, 0, 0, h);
    bg.setColorAt(0, QColor(255, 255, 255, 8));
    bg.setColorAt(1, QColor(255, 255, 255, 2));
    p.fillRect(rect(), bg);

    if (m_displayBands.isEmpty()) {
        m_displayBands.fill(0.0, m_barCount);
    }

    if (m_mode == BarMode) drawBars(p);
    else drawWave(p);

    // 底部基线
    p.setPen(QPen(QColor(255, 255, 255, 50), 1));
    p.drawLine(0, h - 2, w, h - 2);

    // 模式提示（右下角小字）
    p.setPen(QColor(255, 255, 255, 80));
    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);
    p.drawText(w - 60, h - 6, modeName(m_mode));
}

void Visualizer::drawBars(QPainter &p)
{
    const int w = width();
    const int h = height();
    const int n = m_displayBands.size();
    if (n <= 0) return;

    const qreal gap = 3.0;
    const qreal totalGap = gap * (n - 1);
    const qreal barWidth = qMax(2.0, (w - totalGap) / n);
    const qreal baseY = h - 12.0;

    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        qreal level = m_displayBands.at(i);
        level = qPow(level, 0.85);
        const qreal barH = qMax(2.0, level * (h - 24));
        const qreal x = i * (barWidth + gap);

        QRectF r(x, baseY - barH, barWidth, barH);
        QLinearGradient grad(r.bottomLeft(), r.topLeft());
        QColor top = colorForLevel(level);
        QColor bottom(80, 200, 120, 220);
        grad.setColorAt(0.0, bottom);
        grad.setColorAt(1.0, top);
        p.setBrush(grad);

        qreal radius = qMin(barWidth / 2.0, 3.0);
        p.drawRoundedRect(r, radius, radius);

        if (m_glowIntensity > 0.05 && level > 0.05) {
            QColor glow = top;
            glow.setAlpha(int(180 * m_glowIntensity));
            p.setBrush(glow);
            QRectF cap(x, r.top(), barWidth, qMin(barWidth, 4.0));
            p.drawRoundedRect(cap, radius, radius);
        }
    }
}

void Visualizer::drawWave(QPainter &p)
{
    const int w = width();
    const int h = height();
    if (m_history.isEmpty() || w <= 0 || h <= 0) return;

    const qreal midY = h / 2.0;
    const int n = m_history.size();
    const qreal step = qreal(w) / qMax(1, n - 1);
    const qreal maxAmp = h * 0.46;

    // 背景网格
    p.setPen(QPen(QColor(255, 255, 255, 18), 1));
    for (int i = 1; i < 8; ++i) {
        const qreal x = w * i / 8.0;
        p.drawLine(QPointF(x, 0), QPointF(x, h));
    }
    for (int i = 1; i < 4; ++i) {
        const qreal y = h * i / 4.0;
        p.drawLine(QPointF(0, y), QPointF(w, y));
    }

    // 中线
    p.setPen(QPen(QColor(255, 255, 255, 60), 1, Qt::DashLine));
    p.drawLine(QPointF(0, midY), QPointF(w, midY));

    // 构造上下波形
    QVector<QPointF> upper(n), lower(n);
    for (int i = 0; i < n; ++i) {
        const qreal x = i * step;
        const qreal amp = m_history.at(i) * maxAmp;
        upper[i] = QPointF(x, midY - amp);
        lower[i] = QPointF(x, midY + amp);
    }

    // 填充主体（渐变）
    QPainterPath fillPath;
    fillPath.moveTo(upper.first());
    for (int i = 1; i < n; ++i) fillPath.lineTo(upper[i]);
    for (int i = n - 1; i >= 0; --i) fillPath.lineTo(lower[i]);
    fillPath.closeSubpath();

    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0, QColor(255, 90, 110, 220));
    grad.setColorAt(0.3, QColor(255, 200, 80, 230));
    grad.setColorAt(0.65, QColor(120, 230, 130, 230));
    grad.setColorAt(1.0, QColor(60, 180, 220, 220));
    p.fillPath(fillPath, grad);

    // 描边主线（白色高亮）
    p.setPen(QPen(QColor(255, 255, 255, 220), 1.4));
    QPainterPath upperPath, lowerPath;
    upperPath.moveTo(upper.first());
    lowerPath.moveTo(lower.first());
    for (int i = 1; i < n; ++i) {
        upperPath.lineTo(upper[i]);
        lowerPath.lineTo(lower[i]);
    }
    p.drawPath(upperPath);
    p.drawPath(lowerPath);

    // 顶端/底端小竖线（柱化效果）
    p.setPen(QPen(QColor(255, 255, 255, 110), 1));
    for (int i = 0; i < n; ++i) {
        const qreal x = upper[i].x();
        p.drawLine(QPointF(x, upper[i].y()), QPointF(x, lower[i].y()));
    }

    // 当前位置指示（最左端的高亮）
    if (n > 0) {
        const QPointF pTop = upper.first();
        const QPointF pBot = lower.first();
        QRadialGradient hl(pTop.x(), midY, qMax(8.0, (pBot.y() - pTop.y()) * 0.5));
        hl.setColorAt(0.0, QColor(255, 255, 255, 160));
        hl.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(hl);
        p.drawEllipse(QPointF(pTop.x(), midY), 6, qMax(4.0, (pBot.y() - pTop.y()) * 0.5));
    }
}
