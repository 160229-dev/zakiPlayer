#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <QWidget>
#include <QVector>
#include <QPropertyAnimation>

class Visualizer : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal glowIntensity READ glowIntensity WRITE setGlowIntensity)
public:
    enum Mode { BarMode, WaveMode };
    Q_ENUM(Mode)

    explicit Visualizer(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void setBands(const QVector<qreal> &bands);
    void setActive(bool active);
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }
    void toggleMode();

    qreal glowIntensity() const { return m_glowIntensity; }
    void setGlowIntensity(qreal v);

    static QString modeName(Mode m);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QColor colorForLevel(qreal level) const;
    void drawBars(QPainter &p);
    void drawWave(QPainter &p);

    QVector<qreal> m_bands;
    QVector<qreal> m_displayBands;
    QVector<qreal> m_history;   // for waveform: 滚动历史
    qreal m_envelope = 0.0;     // 包络跟随器
    int m_barCount;
    bool m_active;
    qreal m_glowIntensity;
    QPropertyAnimation *m_glowAnim;
    Mode m_mode;
};

#endif // VISUALIZER_H
