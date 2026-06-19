#include "toast.h"

#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>

ToastWindow::ToastWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool
                   | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 给整体透明度做动画
    auto *eff = new QGraphicsOpacityEffect(this);
    eff->setOpacity(0.0);
    setGraphicsEffect(eff);
}

ToastWindow::~ToastWindow() = default;

void ToastWindow::showAtTop(const QString &title, const QString &subtitle)
{
    m_title = title;
    m_subtitle = subtitle;

    // 计算尺寸：宽上限 520，高度按内容算
    QFont fT = font();
    fT.setPointSize(18);
    fT.setBold(true);
    QFontMetrics fmT(fT);

    QFont fS = font();
    fS.setPointSize(10);
    QFontMetrics fmS(fS);

    const int padX = 28;
    const int padY = 16;
    const int lineGap = 6;

    int w = fmT.horizontalAdvance(m_title) + padX * 2;
    if (!m_subtitle.isEmpty()) {
        w = qMax(w, fmS.horizontalAdvance(m_subtitle) + padX * 2);
    }
    w = qBound(280, w, 520);

    int h = padY * 2;
    h += fmT.height();
    if (!m_subtitle.isEmpty()) {
        h += lineGap + fmS.height();
    }

    setFixedSize(w, h);

    // 顶部居中
    QScreen *scr = QApplication::primaryScreen();
    if (scr) {
        const QRect g = scr->availableGeometry();
        move(g.left() + (g.width() - width()) / 2, g.top() + 80);
    }

    show();
    raise();

    // 淡入 → 保持 → 淡出 → 关闭
    auto *eff = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());

    auto *fadeIn = new QPropertyAnimation(eff, "opacity", this);
    fadeIn->setDuration(220);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    auto *hold = new QPropertyAnimation(eff, "opacity", this);
    hold->setDuration(1500);
    hold->setStartValue(1.0);
    hold->setEndValue(1.0);

    auto *fadeOut = new QPropertyAnimation(eff, "opacity", this);
    fadeOut->setDuration(500);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(fadeIn);
    seq->addAnimation(hold);
    seq->addAnimation(fadeOut);
    connect(seq, &QSequentialAnimationGroup::finished, this, [this]() {
        close();
        deleteLater();
    });
    seq->start();
}

void ToastWindow::showMessage(const QString &title, const QString &subtitle)
{
    auto *w = new ToastWindow;
    w->showAtTop(title, subtitle);
}

void ToastWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 背景：深色半透明圆角（更通透）
    QRectF r(0, 0, width(), height());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 22, 32, 140));
    p.drawRoundedRect(r, 14, 14);

    // 顶部一条微高光
    QLinearGradient hl(0, 0, 0, height());
    hl.setColorAt(0.0, QColor(255, 255, 255, 18));
    hl.setColorAt(0.5, QColor(255, 255, 255, 0));
    p.setBrush(hl);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);

    // 文本
    QFont fT = font();
    fT.setPointSize(18);
    fT.setBold(true);
    p.setFont(fT);
    p.setPen(QColor(255, 255, 255, 250));
    QFontMetrics fmT(fT);
    int y = 16;
    QRect rt(28, y, width() - 56, fmT.height());
    p.drawText(rt, Qt::AlignVCenter | Qt::AlignLeft,
               fmT.elidedText(m_title, Qt::ElideRight, rt.width()));

    if (!m_subtitle.isEmpty()) {
        QFont fS = font();
        fS.setPointSize(10);
        p.setFont(fS);
        p.setPen(QColor(200, 210, 230, 200));
        QFontMetrics fmS(fS);
        int y2 = y + fmT.height() + 6;
        QRect rs(28, y2, width() - 56, fmS.height());
        p.drawText(rs, Qt::AlignVCenter | Qt::AlignLeft,
                   fmS.elidedText(m_subtitle, Qt::ElideRight, rs.width()));
    }
}
