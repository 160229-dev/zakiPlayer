#include "mainwindow.h"

#include "playercontroller.h"
#include "playlistmodel.h"
#include "playlistwidget.h"
#include "visualizer.h"
#include "audiospectrum.h"
#include "settingsmanager.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QMediaPlayer>
#include <QSlider>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif
#include <QFrame>
#include <QStackedWidget>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <QDesktopServices>
#include <QtMath>
#include <cmath>
#include <QFileInfo>
#include <QScreen>
#include <QtDebug>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr int kTitleBarHeight = 36;
constexpr int kBottomBarHeight = 130;
constexpr int kPlaylistWidth = 320;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_coverLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_artistLabel(nullptr)
    , m_positionLabel(nullptr)
    , m_durationLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_playButton(nullptr)
    , m_prevButton(nullptr)
    , m_nextButton(nullptr)
    , m_stopButton(nullptr)
    , m_muteButton(nullptr)
    , m_modeButton(nullptr)
    , m_userSeeking(false)
    , m_updatingPosition(false)
    , m_openFilesButton(nullptr)
    , m_openFolderButton(nullptr)
    , m_clearButton(nullptr)
    , m_positionSlider(nullptr)
    , m_volumeSlider(nullptr)
    , m_visualizer(nullptr)
    , m_playlistView(nullptr)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_player(new PlayerController(this))
    , m_playlistModel(new PlaylistModel(this))
    , m_spectrum(new AudioSpectrum(this))
    , m_playMode(SettingsManager::Sequential)
    , m_positionUpdateTimer(new QTimer(this))
    , m_dragging(false)
    , m_bgRefreshTimer(new QTimer(this))
    , m_bgCacheDirty(true)
{
    setWindowTitle(QStringLiteral("zakiPlayer"));
    setWindowIcon(createTriangleIcon(64));
    setFixedSize(960, 640);
    // 默认居中显示
    if (QScreen *scr = QApplication::primaryScreen()) {
        QRect g = scr->availableGeometry();
        move(g.center().x() - 480, g.center().y() - 320);
    }
    // 启动后强制置顶（用 QTimer 等窗口 HWND 创建完成）
    QTimer::singleShot(0, this, &MainWindow::applyAlwaysOnTop);

    // 半透明背景 - 启用毛玻璃 (Windows 11 亚克力效果)
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    // setWindowFlags 会重建原生窗口，重新置顶
    QTimer::singleShot(0, this, &MainWindow::applyAlwaysOnTop);
    buildUi();
    applyStylesheet();
    wireSignals();
    setupShortcuts();
    setupTray();
    loadSettings();
    startSingleInstanceServer();
    registerGlobalHotkeys();

    // 毛玻璃背景刷新：节流到 50ms
    m_bgRefreshTimer->setSingleShot(true);
    m_bgRefreshTimer->setInterval(80);
    connect(m_bgRefreshTimer, &QTimer::timeout, this, [this]() {
        updateBlurBackground();
    });
    QTimer::singleShot(50, this, &MainWindow::updateBlurBackground);

    // 频谱连接
    m_spectrum->setBandCount(48);
    m_spectrum->attachToPlayer(m_player->mediaPlayer());
    connect(m_spectrum, &AudioSpectrum::spectrumUpdated,
            this, &MainWindow::onSpectrumBands);

    // 接受拖拽
    setAcceptDrops(true);

    m_positionUpdateTimer->setInterval(200);
    connect(m_positionUpdateTimer, &QTimer::timeout, this, [this]() {
        if (!m_userSeeking && m_player->state() == QMediaPlayer::PlayingState) {
            m_positionLabel->setText(formatTime(m_player->position()));
        }
    });
    m_positionUpdateTimer->start();

    updateTitle();
    applyPlayMode(m_playMode);
    onUpdatePlayModeButton();
}

MainWindow::~MainWindow()
{
    unregisterGlobalHotkeys();
    saveSettings();
}

void MainWindow::buildUi()
{
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");
    setCentralWidget(m_centralWidget);

    auto *root = new QVBoxLayout(m_centralWidget);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ===== 顶部自定义标题栏 =====
    QWidget *titleBar = new QWidget(m_centralWidget);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(kTitleBarHeight);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(14, 0, 8, 0);
    titleLayout->setSpacing(8);

    QLabel *appLabel = new QLabel(QStringLiteral("zakiPlayer"), titleBar);
    appLabel->setObjectName("appLabel");
    titleLayout->addWidget(appLabel);
    titleLayout->addStretch(1);

    // 窗口控制按钮
    QPushButton *minBtn = new QPushButton(titleBar);
    minBtn->setObjectName("minButton");
    minBtn->setFixedSize(36, 28);
    minBtn->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarMinButton));
    minBtn->setIconSize(QSize(14, 14));
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);

    // 置顶切换按钮
    m_pinTopBtn = new QPushButton(titleBar);
    m_pinTopBtn->setObjectName("pinTopButton");
    m_pinTopBtn->setFixedSize(36, 28);
    m_pinTopBtn->setIconSize(QSize(16, 16));
    m_pinTopBtn->setCursor(Qt::PointingHandCursor);
    m_pinTopBtn->setToolTip(tr("取消置顶"));
    connect(m_pinTopBtn, &QPushButton::clicked, this, &MainWindow::onTogglePinTop);

    QPushButton *closeBtn = new QPushButton(titleBar);
    closeBtn->setObjectName("closeButton");
    closeBtn->setFixedSize(36, 28);
    closeBtn->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeBtn->setIconSize(QSize(14, 14));
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    titleLayout->addWidget(m_pinTopBtn);
    titleLayout->addWidget(minBtn);
    titleLayout->addWidget(closeBtn);

    // 初始化置顶按钮图标（默认置顶，无斜线）
    updatePinTopIcon();

    root->addWidget(titleBar);

    // ===== 主体 =====
    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(12, 6, 12, 6);
    mainLayout->setSpacing(12);

    // 左侧控制面板
    QFrame *leftPanel = new QFrame;
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(320);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(14, 12, 14, 12);
    leftLayout->setSpacing(10);

    // 封面 + 标题区
    QHBoxLayout *coverTitleLayout = new QHBoxLayout;
    coverTitleLayout->setSpacing(12);

    m_coverLabel = new QLabel;
    m_coverLabel->setObjectName("coverLabel");
    m_coverLabel->setFixedSize(96, 96);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet(
        "background: rgba(255,255,255,8);"
        "border-radius: 12px;"
        "color: rgba(255,255,255,120);"
        "font-size: 32px;");
    m_coverLabel->setText(QStringLiteral("\u266B")); // ♫
    coverTitleLayout->addWidget(m_coverLabel);

    QVBoxLayout *titleLayout2 = new QVBoxLayout;
    titleLayout2->setSpacing(4);
    m_titleLabel = new QLabel(tr("未在播放"));
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setWordWrap(true);
    QFont tf = m_titleLabel->font();
    tf.setPointSize(12);
    tf.setBold(true);
    m_titleLabel->setFont(tf);

    m_artistLabel = new QLabel(tr("将文件拖入窗口即可开始"));
    m_artistLabel->setObjectName("artistLabel");
    m_artistLabel->setWordWrap(true);

    titleLayout2->addWidget(m_titleLabel);
    titleLayout2->addWidget(m_artistLabel);
    titleLayout2->addStretch(1);
    coverTitleLayout->addLayout(titleLayout2, 1);

    leftLayout->addLayout(coverTitleLayout);

    // 进度条
    QHBoxLayout *posLayout = new QHBoxLayout;
    posLayout->setSpacing(8);
    m_positionSlider = new QSlider(Qt::Horizontal);
    m_positionSlider->setObjectName("positionSlider");
    m_positionSlider->setRange(0, 0);
    m_positionSlider->setSingleStep(1000);
    m_positionSlider->setPageStep(10000);
    m_positionLabel = new QLabel("00:00");
    m_positionLabel->setObjectName("timeLabel");
    m_positionLabel->setMinimumWidth(48);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_durationLabel = new QLabel("00:00");
    m_durationLabel->setObjectName("timeLabel");
    m_durationLabel->setMinimumWidth(48);
    m_durationLabel->setAlignment(Qt::AlignCenter);
    posLayout->addWidget(m_positionLabel);
    posLayout->addWidget(m_positionSlider, 1);
    posLayout->addWidget(m_durationLabel);
    leftLayout->addLayout(posLayout);

    // 控制按钮
    QHBoxLayout *ctrlLayout = new QHBoxLayout;
    ctrlLayout->setSpacing(8);

    m_prevButton = new QPushButton;
    m_prevButton->setObjectName("ctrlButton");
    m_prevButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaSkipBackward));
    m_prevButton->setIconSize(QSize(20, 20));
    m_prevButton->setFixedSize(46, 38);
    m_prevButton->setToolTip(tr("上一首"));

    m_playButton = new QPushButton;
    m_playButton->setObjectName("playButton");
    m_playButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaPlay));
    m_playButton->setIconSize(QSize(26, 26));
    m_playButton->setFixedSize(58, 44);
    m_playButton->setToolTip(tr("播放/暂停 (空格)"));

    m_nextButton = new QPushButton;
    m_nextButton->setObjectName("ctrlButton");
    m_nextButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaSkipForward));
    m_nextButton->setIconSize(QSize(20, 20));
    m_nextButton->setFixedSize(46, 38);
    m_nextButton->setToolTip(tr("下一首"));

    m_stopButton = new QPushButton;
    m_stopButton->setObjectName("ctrlButton");
    m_stopButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaStop));
    m_stopButton->setIconSize(QSize(20, 20));
    m_stopButton->setFixedSize(46, 38);
    m_stopButton->setToolTip(tr("停止"));

    m_modeButton = new QToolButton;
    m_modeButton->setObjectName("modeButton");
    m_modeButton->setText(tr("顺序"));
    m_modeButton->setFixedSize(60, 38);
    m_modeButton->setToolTip(tr("播放模式"));
    m_modeButton->setPopupMode(QToolButton::InstantPopup);
    m_modeButton->setMenu(buildModeMenu());

    ctrlLayout->addWidget(m_prevButton);
    ctrlLayout->addWidget(m_playButton);
    ctrlLayout->addWidget(m_nextButton);
    ctrlLayout->addWidget(m_stopButton);
    ctrlLayout->addWidget(m_modeButton);
    ctrlLayout->addStretch(1);
    leftLayout->addLayout(ctrlLayout);

    // 音量
    QHBoxLayout *volLayout = new QHBoxLayout;
    volLayout->setSpacing(8);
    m_muteButton = new QPushButton;
    m_muteButton->setObjectName("ctrlButton");
    m_muteButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaVolume));
    m_muteButton->setIconSize(QSize(20, 20));
    m_muteButton->setFixedSize(38, 32);
    m_muteButton->setToolTip(tr("静音 (M)"));

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setObjectName("volumeSlider");
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(SettingsManager::instance()->volume());
    m_volumeSlider->setFixedHeight(20);
    volLayout->addWidget(m_muteButton);
    volLayout->addWidget(m_volumeSlider, 1);
    leftLayout->addLayout(volLayout);

    // 文件操作
    QHBoxLayout *fileOpLayout = new QHBoxLayout;
    fileOpLayout->setSpacing(8);
    m_openFilesButton = new QPushButton(tr("添加文件"));
    m_openFilesButton->setObjectName("smallButton");
    m_openFolderButton = new QPushButton(tr("添加文件夹"));
    m_openFolderButton->setObjectName("smallButton");
    m_clearButton = new QPushButton(tr("清空"));
    m_clearButton->setObjectName("smallButton");
    fileOpLayout->addWidget(m_openFilesButton);
    fileOpLayout->addWidget(m_openFolderButton);
    fileOpLayout->addWidget(m_clearButton);
    leftLayout->addLayout(fileOpLayout);

    // 歌词显示区
    m_lyricsView = new LyricsView;
    m_lyricsView->setObjectName("lyricsView");
    m_lyricsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_lyricsView->setMinimumHeight(160);
    leftLayout->addWidget(m_lyricsView, 1);

    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setText(QStringLiteral("zakiPlayer - 准备就绪"));
    leftLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(leftPanel);

    // ===== 中央：放大版可视化 =====
    QFrame *visualizerFrame = new QFrame;
    visualizerFrame->setObjectName("visualizerFrame");
    QVBoxLayout *vfLayout = new QVBoxLayout(visualizerFrame);
    vfLayout->setContentsMargins(8, 8, 8, 8);
    vfLayout->setSpacing(6);

    // 标题行（放在 visualizer 上方）
    QHBoxLayout *vTopLayout = new QHBoxLayout;
    QLabel *vTitle = new QLabel(tr("可视化"));
    vTitle->setObjectName("sectionTitle");
    QPushButton *modeSwitchBtn = new QPushButton(tr("条形"));
    modeSwitchBtn->setObjectName("smallButton");
    modeSwitchBtn->setFixedHeight(24);
    vTopLayout->addWidget(vTitle);
    vTopLayout->addStretch(1);
    vTopLayout->addWidget(modeSwitchBtn);
    vfLayout->addLayout(vTopLayout);

    m_visualizer = new Visualizer;
    m_visualizer->setObjectName("visualizer");
    m_visualizer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_visualizer->setMinimumSize(420, 240);
    vfLayout->addWidget(m_visualizer, 1);

    connect(modeSwitchBtn, &QPushButton::clicked, this, [this, modeSwitchBtn]() {
        m_visualizer->toggleMode();
        modeSwitchBtn->setText(Visualizer::modeName(m_visualizer->mode()));
    });
    // 双击 visualizer 也可切换
    m_visualizer->setToolTip(tr("双击或右键切换 条形 / 波形"));

    mainLayout->addWidget(visualizerFrame, 1);

    // ===== 右侧：播放列表 =====
    QFrame *playlistFrame = new QFrame;
    playlistFrame->setObjectName("playlistFrame");
    playlistFrame->setFixedWidth(kPlaylistWidth);
    QVBoxLayout *plLayout = new QVBoxLayout(playlistFrame);
    plLayout->setContentsMargins(8, 8, 8, 8);
    plLayout->setSpacing(6);

    QHBoxLayout *plHeader = new QHBoxLayout;
    QLabel *plTitle = new QLabel(tr("播放列表"));
    plTitle->setObjectName("sectionTitle");
    plHeader->addWidget(plTitle);
    plHeader->addStretch(1);
    plLayout->addLayout(plHeader);

    m_playlistView = new PlaylistWidget;
    m_playlistView->setObjectName("playlistView");
    m_playlistView->setModel(m_playlistModel);
    plLayout->addWidget(m_playlistView, 1);

    mainLayout->addWidget(playlistFrame);

    root->addLayout(mainLayout, 1);

    // 阴影
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(0, 0, 0, 200));
    setGraphicsEffect(shadow);
}

QMenu* MainWindow::buildModeMenu()
{
    auto *menu = new QMenu(this);
    auto *grp = new QActionGroup(this);
    auto addMode = [this, menu, grp](SettingsManager::PlayMode mode, const QString &text) {
        auto *a = new QAction(text, this);
        a->setCheckable(true);
        a->setData(static_cast<int>(mode));
        grp->addAction(a);
        menu->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode]() {
            applyPlayMode(mode);
            onUpdatePlayModeButton();
        });
    };
    addMode(SettingsManager::Sequential, tr("顺序播放"));
    addMode(SettingsManager::RepeatAll, tr("列表循环"));
    addMode(SettingsManager::RepeatOne, tr("单曲循环"));
    addMode(SettingsManager::Shuffle, tr("随机播放"));
    return menu;
}

QMenu* MainWindow::buildPlaylistContextMenu(int clickedRow)
{
    auto *menu = new QMenu(this);
    if (clickedRow < 0) {
        // 右键空白区：只有清空
        auto *clearAct = menu->addAction(tr("清空列表"));
        return menu;
    }
    // 右键某个曲子：播放 / 删除 / 打开位置
    auto *playAct = menu->addAction(tr("播放"));
    auto *removeAct = menu->addAction(tr("从列表中删除"));
    if (m_playlistModel->currentIndex() == clickedRow
        && m_player->state() == QMediaPlayer::PlayingState) {
        playAct->setEnabled(false);
        playAct->setText(tr("播放（正在播放）"));
    }
    menu->addSeparator();
    auto *openLocationAct = menu->addAction(tr("打开文件所在位置"));
    return menu;
}

void MainWindow::applyStylesheet()
{
    QString style = R"(
        QMainWindow {
            background: transparent;
        }
        #centralWidget {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 rgba(28, 32, 48, 140),
                        stop:0.5 rgba(20, 24, 36, 160),
                        stop:1 rgba(16, 18, 28, 170));
            border-radius: 14px;
            border: 1px solid rgba(255, 255, 255, 22);
        }
        #titleBar {
            background: rgba(0, 0, 0, 80);
            border-top-left-radius: 14px;
            border-top-right-radius: 14px;
            border-bottom: 1px solid rgba(255, 255, 255, 16);
        }
        #appLabel {
            color: rgba(255, 255, 255, 200);
            font-size: 13px;
            font-weight: 600;
            letter-spacing: 1px;
        }
        #minButton, #pinTopButton, #closeButton {
            background: transparent;
            border: none;
            border-radius: 6px;
        }
        #minButton:hover, #pinTopButton:hover {
            background: rgba(255, 255, 255, 30);
        }
        #closeButton:hover {
            background: rgba(232, 17, 35, 220);
        }
        #leftPanel {
            background: rgba(20, 22, 30, 180);
            border: 1px solid rgba(255, 255, 255, 14);
            border-radius: 14px;
        }
        #playlistFrame {
            background: rgba(20, 22, 30, 180);
            border: 1px solid rgba(255, 255, 255, 14);
            border-radius: 14px;
        }
        #visualizerFrame {
            background: rgba(10, 12, 20, 120);
            border: 1px solid rgba(255, 255, 255, 14);
            border-radius: 14px;
        }
        #titleLabel {
            color: rgba(255, 255, 255, 230);
        }
        #artistLabel {
            color: rgba(255, 255, 255, 150);
        }
        #sectionTitle {
            color: rgba(255, 255, 255, 200);
            font-size: 13px;
            font-weight: 600;
            padding: 4px;
        }
        #timeLabel, #statusLabel {
            color: rgba(255, 255, 255, 150);
            font-size: 11px;
        }
        #statusLabel {
            padding: 4px;
        }
        #lyricsView {
            background: rgba(0, 0, 0, 40);
            border-radius: 8px;
            border: 1px solid rgba(255, 255, 255, 16);
        }
        QPushButton {
            color: rgba(255, 255, 255, 220);
            background: rgba(255, 255, 255, 14);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 10px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background: rgba(120, 180, 255, 50);
            border: 1px solid rgba(140, 200, 255, 180);
            color: white;
        }
        QPushButton:pressed {
            background: rgba(120, 180, 255, 100);
        }
        #ctrlButton {
            border-radius: 10px;
        }
        #playButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 rgba(120, 200, 255, 230),
                        stop:1 rgba(70, 130, 240, 230));
            color: white;
            border: 1px solid rgba(140, 200, 255, 200);
            border-radius: 12px;
            font-weight: bold;
        }
        #playButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 rgba(150, 210, 255, 250),
                        stop:1 rgba(100, 160, 255, 250));
            border: 1px solid rgba(180, 220, 255, 240);
        }
        #smallButton {
            padding: 4px 10px;
            font-size: 12px;
            border-radius: 8px;
        }
        #modeButton {
            background: rgba(255, 255, 255, 14);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 10px;
            color: rgba(255, 255, 255, 220);
            padding: 0 6px;
        }
        #modeButton:hover {
            background: rgba(120, 180, 255, 50);
        }
        #modeButton::menu-indicator {
            image: none;
            width: 0; height: 0;
        }
        QToolButton QMenu {
            background: rgba(28, 30, 40, 240);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 8px;
            padding: 4px;
        }
        QToolButton QMenu::item {
            color: rgba(255, 255, 255, 220);
            padding: 6px 24px;
            border-radius: 6px;
        }
        QToolButton QMenu::item:selected {
            background: rgba(120, 180, 255, 80);
        }
        QSlider::groove:horizontal {
            background: rgba(255, 255, 255, 22);
            height: 6px;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 rgba(120, 200, 255, 200),
                        stop:1 rgba(70, 130, 240, 220));
            border-radius: 3px;
        }
        QSlider::add-page:horizontal {
            background: rgba(255, 255, 255, 14);
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: white;
            border: 2px solid rgba(120, 180, 255, 200);
            width: 14px;
            height: 14px;
            margin: -6px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: rgba(180, 220, 255, 255);
        }
        #volumeSlider::groove:horizontal {
            height: 4px;
        }
        #playlistView {
            background: rgba(0, 0, 0, 60);
            border: 1px solid rgba(255, 255, 255, 12);
            border-radius: 10px;
            color: rgba(255, 255, 255, 220);
            outline: none;
        }
        #playlistView::item {
            padding: 8px 10px;
            border-radius: 6px;
            margin: 1px 4px;
        }
        #playlistView::item:selected {
            background: rgba(120, 180, 255, 100);
            color: white;
        }
        #playlistView::item:hover {
            background: rgba(255, 255, 255, 16);
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px;
        }
        QScrollBar::handle:vertical {
            background: rgba(255, 255, 255, 80);
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(255, 255, 255, 140);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QMenu {
            background: rgba(28, 30, 40, 240);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 8px;
            padding: 6px;
            color: rgba(255, 255, 255, 220);
        }
        QMenu::item {
            padding: 6px 24px;
            border-radius: 6px;
        }
        QMenu::item:selected {
            background: rgba(120, 180, 255, 80);
        }
        QMenu::separator {
            height: 1px;
            background: rgba(255, 255, 255, 30);
            margin: 4px 8px;
        }
    )";
    setStyleSheet(style);
    qApp->setStyleSheet(style);
}

void MainWindow::wireSignals()
{
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_prevButton, &QPushButton::clicked, this, &MainWindow::onPrevClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::onNextClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_muteButton, &QPushButton::clicked, this, &MainWindow::onMuteClicked);
    connect(m_openFilesButton, &QPushButton::clicked, this, &MainWindow::onOpenFilesClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        m_playlistModel->clear();
        m_player->stop();
        m_player->setMedia(QUrl());
        m_titleLabel->setText(tr("未在播放"));
        m_artistLabel->setText(tr("将文件拖入窗口即可开始"));
        m_positionLabel->setText(QStringLiteral("00:00"));
        m_durationLabel->setText(QStringLiteral("00:00"));
        m_positionSlider->setValue(0);
        m_statusLabel->setText(QString());
        m_coverLabel->setPixmap(QPixmap());
        m_coverLabel->setText(QStringLiteral("\u266B"));
    });

    connect(m_positionSlider, &QSlider::sliderPressed, this, &MainWindow::onPositionSliderPressed);
    connect(m_positionSlider, &QSlider::sliderReleased, this, &MainWindow::onPositionSliderReleased);
    connect(m_positionSlider, &QSlider::sliderMoved, this, &MainWindow::onPositionSliderMoved);
    connect(m_positionSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_userSeeking) {
            m_positionLabel->setText(formatTime(value));
        } else if (!m_updatingPosition) {
            m_player->seek(value);
        }
    });

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_player->setVolume(v);
    });

    connect(m_player, &PlayerController::positionChanged,
            this, &MainWindow::onPositionChanged);
    connect(m_player, &PlayerController::durationChanged,
            this, &MainWindow::onDurationChanged);
    connect(m_player, &PlayerController::volumeChanged,
            this, &MainWindow::onVolumeChanged);
    connect(m_player, &PlayerController::mutedChanged,
            this, &MainWindow::onMutedChanged);
    connect(m_player, &PlayerController::stateChanged,
            this, &MainWindow::onPlaybackStateChanged);
    connect(m_player, &PlayerController::endOfMedia,
            this, &MainWindow::onEndOfMedia);
    connect(m_player, &PlayerController::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_player, &PlayerController::coverArtChanged,
            this, &MainWindow::onCoverArtChanged);
    connect(m_player, &PlayerController::currentMediaChanged,
            this, &MainWindow::onCurrentMediaChanged);

    connect(m_playlistView, &PlaylistWidget::playRequested,
            this, &MainWindow::onPlaylistPlayRequested);
    m_playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistView, &QWidget::customContextMenuRequested,
            this, &MainWindow::onShowContextMenu);

    // 歌词视图：固定 / 全屏
    if (m_lyricsView) {
        connect(m_lyricsView, &LyricsView::pinRequested,
                this, &MainWindow::onLyricsPinRequested);
        connect(m_lyricsView, &LyricsView::fullscreenRequested,
                this, &MainWindow::onLyricsFullscreenRequested);
    }
}

void MainWindow::setupShortcuts()
{
    // 这些 QShortcut 将全局快捷键挂在 MainWindow 上
    auto *playPause = new QShortcut(QKeySequence(Qt::Key_Space), this);
    playPause->setContext(Qt::ApplicationShortcut);
    connect(playPause, &QShortcut::activated, this, &MainWindow::onPlayClicked);

    auto *seekFwd = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(seekFwd, &QShortcut::activated, this, &MainWindow::onSeekForward);

    auto *seekBwd = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(seekBwd, &QShortcut::activated, this, &MainWindow::onSeekBackward);

    auto *volUp = new QShortcut(QKeySequence(Qt::Key_Up), this);
    connect(volUp, &QShortcut::activated, this, &MainWindow::onVolumeUp);

    auto *volDown = new QShortcut(QKeySequence(Qt::Key_Down), this);
    connect(volDown, &QShortcut::activated, this, &MainWindow::onVolumeDown);

    auto *mute = new QShortcut(QKeySequence("M"), this);
    connect(mute, &QShortcut::activated, this, &MainWindow::onMuteClicked);

    auto *next = new QShortcut(QKeySequence("N"), this);
    connect(next, &QShortcut::activated, this, &MainWindow::onNextClicked);

    auto *prev = new QShortcut(QKeySequence("P"), this);
    connect(prev, &QShortcut::activated, this, &MainWindow::onPrevClicked);
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(createTriangleIcon(32));
    m_trayIcon->setToolTip(QStringLiteral("zakiPlayer"));
    m_trayMenu = new QMenu(this);
    auto *showAct = m_trayMenu->addAction(tr("显示主界面"));
    auto *playAct = m_trayMenu->addAction(tr("播放/暂停"));
    auto *quitAct = m_trayMenu->addAction(tr("退出"));
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayActivated);
    connect(showAct, &QAction::triggered, this, [this]() { show(); raise(); activateWindow(); });
    connect(playAct, &QAction::triggered, this, &MainWindow::onPlayClicked);
    connect(quitAct, &QAction::triggered, this, [this]() { qApp->quit(); });
    m_trayIcon->show();
}

QIcon MainWindow::createTriangleIcon(int size) const
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 等边三角形顶点（指向右）
    const qreal cx = size / 2.0;
    const qreal cy = size / 2.0;
    const qreal r = size * 0.38;
    const QPointF V0(cx + r * 0.866, cy);              // 右侧尖端
    const QPointF V1(cx - r * 0.433, cy - r * 0.75);   // 左上
    const QPointF V2(cx - r * 0.433, cy + r * 0.75);   // 左下

    QPainterPath path;
    path.moveTo(V0);
    path.lineTo(V1);
    path.lineTo(V2);
    path.closeSubpath();

    // RoundCap + RoundJoin：直接描边三角即可获得圆角
    const qreal penW = qMax(2.4, size * 0.10);
    p.setPen(QPen(QColor(255, 255, 255, 235), penW,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    p.end();
    return QIcon(pm);
}

// 启动本地 IPC 服务，监听"是否已有实例在跑"
void MainWindow::startSingleInstanceServer()
{
    m_localServer = new QLocalServer(this);
    // 服务名固定即可；上次异常退出可能残留 socket 文件，先清掉
    QLocalServer::removeServer(QStringLiteral("zakiplayer-singleton-v1"));
    if (!m_localServer->listen(QStringLiteral("zakiplayer-singleton-v1"))) {
        qWarning("IPC: listen failed: %s", qPrintable(m_localServer->errorString()));
        return;
    }
    connect(m_localServer, &QLocalServer::newConnection,
            this, &MainWindow::onIpcNewConnection);
}

void MainWindow::onIpcNewConnection()
{
    while (m_localServer && m_localServer->hasPendingConnections()) {
        QLocalSocket *client = m_localServer->nextPendingConnection();
        if (!client) continue;
        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            const QByteArray data = client->readAll();
            const QString msg = QString::fromUtf8(data).trimmed();
            // 协议：第一行是 "show" / "hide" / "quit"，可选第二行以 \n 分隔的路径列表
            const QStringList lines = msg.split(QChar('\n'));
            const QString cmd = lines.value(0).trimmed();
            QStringList paths;
            if (lines.size() > 1) {
                const QString p = lines.value(1).trimmed();
                if (!p.isEmpty()) paths = p.split(QChar('\u0001'), Qt::SkipEmptyParts);
            }
            if (cmd == QLatin1String("quit")) {
                QApplication::quit();
                return;
            }
            // show 或带路径：把窗口拉到前面
            if (isMinimized()) showNormal();
            if (!isVisible()) show();
            raise();
            activateWindow();
            setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            if (!paths.isEmpty()) {
                setStartupPlaylist(paths);
            }
        });
        connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    }
}

void MainWindow::loadSettings()
{
    auto *s = SettingsManager::instance();
    // 窗口尺寸固定为 960×640，不还原
    QByteArray state = s->windowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
    m_volumeSlider->setValue(s->volume());
    m_playMode = s->playMode();

    // 加载上次播放列表
    QStringList paths = s->playlist();
    if (!paths.isEmpty()) {
        m_playlistModel->fromStringList(paths);
        int idx = s->lastIndex();
        if (idx >= 0 && idx < m_playlistModel->count()) {
            m_playlistModel->setCurrentIndex(idx);
        }
    }
}

void MainWindow::saveSettings()
{
    auto *s = SettingsManager::instance();
    s->setWindowState(saveState());
    s->setVolume(m_volumeSlider->value());
    s->setMuted(m_player->muted());
    s->setPlayMode(m_playMode);
    s->setPlaylist(m_playlistModel->toStringList());
    s->setLastIndex(m_playlistModel->currentIndex());
    s->save();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_trayIcon && m_trayIcon->isVisible()) {
        // 最小化到托盘
        hide();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_normalSize = size();
    // 尺寸变化后刷新毛玻璃背景
    scheduleBackgroundRefresh();
    // 重新置顶（防其它应用把我们从 TOPMOST 挤掉）
    applyAlwaysOnTop();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    // 窗口移动后背景不再与屏幕对齐，延迟刷新
    scheduleBackgroundRefresh();
    // 重新置顶
    applyAlwaysOnTop();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 每次窗口从隐藏到显示，强制置顶（API 级别）
    applyAlwaysOnTop();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *top = childAt(event->pos());
        // 拖动窗口：仅在标题栏区域
        if (event->pos().y() < kTitleBarHeight) {
            m_dragging = true;
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
            return;
        }
        Q_UNUSED(top);
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
    QMainWindow::mouseReleaseEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    Q_UNUSED(event);
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        handleDroppedUrls(event->mimeData()->urls());
        event->acceptProposedAction();
    }
}

void MainWindow::handleDroppedUrls(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) return;
    bool first = (m_playlistModel->count() == 0);
    appendToPlaylist(urls);
    if (first && m_playlistModel->count() > 0) {
        onPlaylistPlayRequested(0);
    } else {
        // 默认播放刚加入的第一项
        int start = m_playlistModel->count() - urls.size();
        if (start >= 0) {
            onPlaylistPlayRequested(start);
        }
    }
}

void MainWindow::appendToPlaylist(const QList<QUrl> &urls)
{
    QList<QUrl> valid;
    for (const QUrl &u : urls) {
        if (!u.isLocalFile()) continue;
        QString path = u.toLocalFile();
        QFileInfo info(path);
        if (info.isDir()) {
            QDir dir(path);
            QStringList entries = dir.entryList(
                {"*.mp3", "*.wav", "*.flac", "*.aac", "*.m4a", "*.ogg", "*.opus", "*.wma",
                 "*.aiff", "*.ape", "*.mp4", "*.avi", "*.mkv", "*.mov", "*.wmv", "*.flv",
                 "*.webm", "*.mpg", "*.mpeg", "*.m4v", "*.3gp", "*.ts"},
                QDir::Files, QDir::Name);
            for (const QString &e : entries) {
                valid.append(QUrl::fromLocalFile(dir.absoluteFilePath(e)));
            }
        } else {
            valid.append(u);
        }
    }
    m_playlistModel->addUrls(valid);
}

void MainWindow::onPlayClicked()
{
    if (m_player->state() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else if (m_playlistModel->currentIndex() >= 0 || m_player->mediaPlayer()->source().isValid()) {
        m_player->play();
    } else if (m_playlistModel->count() > 0) {
        onPlaylistPlayRequested(0);
    }
}

void MainWindow::onPrevClicked()
{
    int idx = m_playlistModel->currentIndex();
    if (idx > 0) {
        onPlaylistPlayRequested(idx - 1);
    } else if (m_playlistModel->count() > 0) {
        onPlaylistPlayRequested(m_playlistModel->count() - 1);
    }
}

void MainWindow::onNextClicked()
{
    int idx = m_playlistModel->currentIndex();
    if (m_playlistModel->count() == 0) return;
    if (idx + 1 < m_playlistModel->count()) {
        onPlaylistPlayRequested(idx + 1);
    } else {
        onPlaylistPlayRequested(0);
    }
}

void MainWindow::onStopClicked()
{
    m_player->stop();
}

void MainWindow::onMuteClicked()
{
    m_player->setMuted(!m_player->muted());
}

void MainWindow::onModeClicked()
{
    // 通过菜单处理
}

void MainWindow::onOpenFilesClicked()
{
    auto *s = SettingsManager::instance();
    QString path = s->lastOpenPath();
    QStringList files = QFileDialog::getOpenFileNames(this, tr("选择媒体文件"),
                                                     path,
                                                     tr("媒体文件 (*.mp3 *.wav *.flac *.aac *.m4a "
                                                        "*.ogg *.opus *.wma *.aiff *.ape "
                                                        "*.mp4 *.avi *.mkv *.mov *.wmv *.flv "
                                                        "*.webm *.mpg *.mpeg *.m4v *.3gp *.ts);;"
                                                        "所有文件 (*.*)"));
    if (files.isEmpty()) return;
    QList<QUrl> urls;
    for (const QString &f : files) {
        urls << QUrl::fromLocalFile(f);
    }
    s->setLastOpenPath(QFileInfo(files.first()).absolutePath());
    bool wasEmpty = (m_playlistModel->count() == 0);
    appendToPlaylist(urls);
    if (wasEmpty) onPlaylistPlayRequested(0);
}

void MainWindow::onOpenFolderClicked()
{
    auto *s = SettingsManager::instance();
    QString path = s->lastOpenPath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择文件夹"), path);
    if (dir.isEmpty()) return;
    s->setLastOpenPath(dir);
    QUrl u = QUrl::fromLocalFile(dir);
    bool wasEmpty = (m_playlistModel->count() == 0);
    m_playlistModel->addUrls({ u });
    if (wasEmpty && m_playlistModel->count() > 0) onPlaylistPlayRequested(0);
}

void MainWindow::onClearPlaylistClicked()
{
    m_playlistModel->clear();
    m_player->stop();
    m_player->setMedia(QUrl());
    m_titleLabel->setText(tr("未在播放"));
    m_artistLabel->setText(tr("将文件拖入窗口即可开始"));
    m_positionLabel->setText(QStringLiteral("00:00"));
    m_durationLabel->setText(QStringLiteral("00:00"));
    m_positionSlider->setValue(0);
    m_statusLabel->setText(QString());
    m_coverLabel->setPixmap(QPixmap());
    m_coverLabel->setText(QStringLiteral("\u266B"));
}

void MainWindow::onRemoveSelectedClicked()
{
    QModelIndexList sel = m_playlistView->selectionModel()->selectedRows();
    std::sort(sel.begin(), sel.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() > b.row();
    });
    for (const QModelIndex &idx : sel) {
        m_playlistModel->removeAt(idx.row());
    }
}

void MainWindow::onPositionSliderPressed()
{
    m_userSeeking = true;
}

void MainWindow::onPositionSliderReleased()
{
    m_userSeeking = false;
    // 注意：valueChanged 已经在 release 时触发了 seek 逻辑（m_userSeeking=false）
    // 这里仅保证最终值被设置一次，避免遗漏（处理只点击滑块的情况）
    m_player->seek(m_positionSlider->value());
}

void MainWindow::onPositionSliderMoved(int value)
{
    m_positionLabel->setText(formatTime(value));
}

void MainWindow::onPositionChanged(qint64 pos)
{
    if (m_userSeeking) return;
    m_updatingPosition = true;
    if (m_positionSlider->maximum() != m_player->duration()) {
        m_positionSlider->setMaximum(m_player->duration());
    }
    m_positionSlider->setValue(pos);
    m_positionLabel->setText(formatTime(pos));
    m_updatingPosition = false;
    if (m_lyricsView) m_lyricsView->setPosition(pos);
    if (m_pinnedLyrics && m_pinnedLyrics->isVisible()) {
        m_pinnedLyrics->updateLines(m_lyricsView->lines(), m_lyricsView->currentIndex());
    }
    if (m_fullscreenLyrics && m_fullscreenLyrics->isVisible()) {
        m_fullscreenLyrics->updateLines(m_lyricsView->lines(), m_lyricsView->currentIndex());
    }
}

void MainWindow::onDurationChanged(qint64 dur)
{
    m_positionSlider->setRange(0, dur);
    m_durationLabel->setText(formatTime(dur));
}

void MainWindow::onVolumeChanged(int vol)
{
    if (m_volumeSlider->value() != vol) {
        m_volumeSlider->setValue(vol);
    }
}

void MainWindow::onMutedChanged(bool muted)
{
    m_muteButton->setIcon(qApp->style()->standardIcon(
        muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
}

void MainWindow::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        m_playButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaPause));
    } else {
        m_playButton->setIcon(qApp->style()->standardIcon(QStyle::SP_MediaPlay));
    }
}

void MainWindow::onEndOfMedia()
{
    decideNextAfterEnd();
}

void MainWindow::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    Q_UNUSED(status);
}

void MainWindow::onErrorOccurred(const QString &msg)
{
    m_statusLabel->setText(tr("错误: %1").arg(msg));
    if (m_trayIcon) {
        m_trayIcon->showMessage(tr("zakiPlayer"), tr("播放错误: %1").arg(msg),
                                QSystemTrayIcon::Critical);
    }
}

void MainWindow::onSpectrumBands(const QVector<qreal> &bands)
{
    if (m_visualizer) {
        m_visualizer->setBands(bands);
    }
}

void MainWindow::onCoverArtChanged()
{
    updateCoverDisplay();
}

void MainWindow::onCurrentMediaChanged()
{
    m_titleLabel->setText(m_player->currentTitle());
    m_artistLabel->setText(m_player->currentArtist());
    updateTitle();
    updateCoverDisplay();
    loadLyricsForCurrent();

    // 窗口关闭或最小化时，弹出顶部 toast 通知
    if (!isVisible() || isMinimized()) {
        const QString t = m_player->currentTitle();
        const QString a = m_player->currentArtist();
        if (!t.isEmpty()) {
            ToastWindow::showMessage(t, a);
        }
    }
}

void MainWindow::loadLyricsForCurrent()
{
    if (!m_lyricsView) return;
    // 找当前播放 URL 对应的本地文件，再找同名 .lrc
    const QUrl u = m_player->currentSource();
    const QString path = u.toLocalFile();
    if (path.isEmpty()) {
        m_lyricsView->clearLines();
        return;
    }
    // 尝试同目录下的同名 .lrc
    QFileInfo fi(path);
    QString lrc = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".lrc");
    QVector<LyricsView::Line> lines;
    if (QFileInfo::exists(lrc)) {
        lines = LyricsView::parseLrcFile(lrc);
    }
    if (lines.isEmpty()) {
        m_lyricsView->clearLines();
    } else {
        m_lyricsView->setLines(lines);
    }
    // 同步给悬浮窗
    if (m_pinnedLyrics && m_pinnedLyrics->isVisible()) {
        m_pinnedLyrics->updateLines(m_lyricsView->lines(), m_lyricsView->currentIndex());
        m_pinnedLyrics->setSource(m_titleLabel->text());
    }
    if (m_fullscreenLyrics && m_fullscreenLyrics->isVisible()) {
        m_fullscreenLyrics->updateLines(m_lyricsView->lines(), m_lyricsView->currentIndex());
        m_fullscreenLyrics->setSource(m_titleLabel->text());
    }
}

void MainWindow::updateCoverDisplay()
{
    QPixmap cover = m_player->coverArt();
    if (!cover.isNull()) {
        QPixmap round = cover.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_coverLabel->setPixmap(round);
        m_coverLabel->setText(QString());
    } else {
        // 先清 pixmap，再 setText，避免 setPixmap(null) 把文字擦掉
        m_coverLabel->setPixmap(QPixmap());
        m_coverLabel->setText(QStringLiteral("\u266B"));
    }
}

void MainWindow::onPlaylistPlayRequested(int index)
{
    if (index < 0 || index >= m_playlistModel->count()) return;
    m_playlistModel->setCurrentIndex(index);
    QUrl url = m_playlistModel->urlAt(index);
    if (url.isValid()) {
        m_player->setMedia(url);
        m_player->play();
    }
    m_playlistView->setCurrentIndex(m_playlistModel->index(index));
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) hide(); else {
            setWindowState(windowState() & ~Qt::WindowMaximized);
            show(); raise(); activateWindow();
            applyAlwaysOnTop();
        }
    }
}

void MainWindow::onSeekForward()
{
    m_player->seekRelative(10000);
}

void MainWindow::onSeekBackward()
{
    m_player->seekRelative(-10000);
}

void MainWindow::onVolumeUp()
{
    m_player->setVolume(m_player->volume() + 5);
}

void MainWindow::onVolumeDown()
{
    m_player->setVolume(m_player->volume() - 5);
}

void MainWindow::onUpdatePlayModeButton()
{
    QString text;
    switch (m_playMode) {
    case SettingsManager::Sequential: text = tr("顺序"); break;
    case SettingsManager::RepeatOne: text = tr("单曲"); break;
    case SettingsManager::RepeatAll: text = tr("循环"); break;
    case SettingsManager::Shuffle:   text = tr("随机"); break;
    }
    m_modeButton->setText(text);
}

void MainWindow::onEqualizerToggle(bool active)
{
    m_spectrum->setActive(active);
}

void MainWindow::onShowContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_playlistView->indexAt(pos);
    const int clickedRow = idx.isValid() ? idx.row() : -1;
    auto *menu = buildPlaylistContextMenu(clickedRow);
    QAction *act = menu->exec(m_playlistView->viewport()->mapToGlobal(pos));
    if (!act) { delete menu; return; }
    const QString txt = act->text();
    if (txt == tr("播放") || txt.startsWith(tr("播放"))) {
        if (clickedRow >= 0) onPlaylistPlayRequested(clickedRow);
    } else if (txt == tr("从列表中删除")) {
        if (clickedRow >= 0) m_playlistModel->removeAt(clickedRow);
    } else if (txt == tr("清空列表")) {
        m_playlistModel->clear();
        m_player->stop();
        m_player->setMedia(QUrl());
        m_titleLabel->setText(tr("未在播放"));
        m_artistLabel->setText(tr("将文件拖入窗口即可开始"));
        m_positionLabel->setText(QStringLiteral("00:00"));
        m_durationLabel->setText(QStringLiteral("00:00"));
        m_positionSlider->setValue(0);
        m_statusLabel->setText(QString());
        m_coverLabel->setPixmap(QPixmap());
        m_coverLabel->setText(QStringLiteral("\u266B"));
        if (m_lyricsView) m_lyricsView->clearLines();
    } else if (txt == tr("打开文件所在位置")) {
        if (clickedRow >= 0) {
            const QUrl u = m_playlistModel->urlAt(clickedRow);
            const QString path = u.toLocalFile();
            if (QFileInfo(path).exists()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        }
    }
    delete menu;
}

void MainWindow::onLyricsPinRequested()
{
    if (!m_lyricsView || !m_lyricsView->hasLines()) return;
    if (!m_pinnedLyrics) {
        m_pinnedLyrics = new PinnedLyricsWindow;
    }
    m_pinnedLyrics->setSource(m_titleLabel->text());
    if (m_lyricsView) {
        m_pinnedLyrics->updateLines(m_lyricsView->lines(), m_lyricsView->currentIndex());
    }
    m_pinnedLyrics->show();
    m_pinnedLyrics->raise();
}

void MainWindow::onLyricsFullscreenRequested()
{
    if (!m_lyricsView || !m_lyricsView->hasLines()) return;
    // 每次都重建一个干净的全屏窗口，避免复用导致状态被记住（最大化、WS_CAPTION 等）
    if (m_fullscreenLyrics) {
        m_fullscreenLyrics->deleteLater();
        m_fullscreenLyrics = nullptr;
    }
    m_fullscreenLyrics = new FullscreenLyricsWindow;
    m_fullscreenLyrics->setSource(m_titleLabel->text());
    if (m_lyricsView) {
        m_fullscreenLyrics->updateLines(m_lyricsView->lines(), m_lyricsView->currentIndex());
    }
    // enterFullScreenMode 内部用 Windows API 强制覆盖整屏
    //   - WS_POPUP：无标题栏 / 无边框 / 不可拖动 / 不可调整
    //   - WS_EX_TOPMOST：永远在最上层
    //   - WS_EX_TOOLWINDOW：不在任务栏 / Alt-Tab
    m_fullscreenLyrics->show();           // 先让 native window 创建出来
    m_fullscreenLyrics->enterFullScreenMode();
}

void MainWindow::playAt(int index)
{
    onPlaylistPlayRequested(index);
}

void MainWindow::applyPlayMode(SettingsManager::PlayMode mode)
{
    m_playMode = mode;
    SettingsManager::instance()->setPlayMode(mode);
    onUpdatePlayModeButton();
}

void MainWindow::decideNextAfterEnd()
{
    int next = -1;
    switch (m_playMode) {
    case SettingsManager::Sequential:
        next = m_playlistModel->nextIndexSequential();
        break;
    case SettingsManager::RepeatOne:
        next = m_playlistModel->nextIndexRepeatOne();
        // 单曲循环：从头再播
        m_player->seek(0);
        m_player->play();
        return;
    case SettingsManager::RepeatAll:
        next = m_playlistModel->nextIndexRepeatAll();
        break;
    case SettingsManager::Shuffle:
        next = m_playlistModel->nextIndexShuffle();
        break;
    }
    if (next >= 0) {
        onPlaylistPlayRequested(next);
    } else {
        m_player->stop();
    }
}

void MainWindow::setUiEnabled(bool enabled)
{
    m_playButton->setEnabled(enabled);
    m_prevButton->setEnabled(enabled);
    m_nextButton->setEnabled(enabled);
    m_stopButton->setEnabled(enabled);
    m_positionSlider->setEnabled(enabled);
    m_volumeSlider->setEnabled(enabled);
}

void MainWindow::updateTitle()
{
    QString t = m_player->currentTitle();
    if (t.isEmpty()) {
        setWindowTitle(QStringLiteral("zakiPlayer"));
        m_statusLabel->setText(QStringLiteral("zakiPlayer - 准备就绪"));
    } else {
        setWindowTitle(QStringLiteral("zakiPlayer - %1").arg(t));
        m_statusLabel->setText(QStringLiteral("正在播放: %1 - %2").arg(t, m_player->currentArtist()));
    }
}

QString MainWindow::formatTime(qint64 ms) const
{
    if (ms < 0) ms = 0;
    qint64 total = ms / 1000;
    qint64 h = total / 3600;
    qint64 m = (total % 3600) / 60;
    qint64 s = total % 60;
    if (h > 0) {
        return QString::asprintf("%lld:%02lld:%02lld", h, m, s);
    }
    return QString::asprintf("%02lld:%02lld", m, s);
}

void MainWindow::setStartupPlaylist(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    QList<QUrl> urls;
    for (const QString &p : paths) {
        urls << QUrl::fromLocalFile(p);
    }
    appendToPlaylist(urls);
    if (m_playlistModel && m_playlistModel->count() > 0) {
        onPlaylistPlayRequested(0);
    }
}

// =============================================================
// 全局热键（Windows RegisterHotKey，无焦点也能用）
// =============================================================
void MainWindow::registerGlobalHotkeys()
{
#ifdef Q_OS_WIN
    if (m_globalHotkeysRegistered) return;
    // Ctrl+Alt+Space  播放/暂停
    RegisterHotKey((HWND)winId(), HK_PlayPause, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_SPACE);
    // Ctrl+Alt+Left   上一首
    RegisterHotKey((HWND)winId(), HK_Prev,      MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_LEFT);
    // Ctrl+Alt+Right  下一首
    RegisterHotKey((HWND)winId(), HK_Next,      MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_RIGHT);
    // Ctrl+Alt+Up     停止
    RegisterHotKey((HWND)winId(), HK_Stop,      MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_UP);
    // Ctrl+Alt+Down   显示/隐藏窗口
    RegisterHotKey((HWND)winId(), HK_ShowHide,  MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_DOWN);
    // Ctrl+Alt+W      召唤窗口（任何时候都能用，强制显示并聚焦）
    RegisterHotKey((HWND)winId(), HK_SummonWin, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W');
    m_globalHotkeysRegistered = true;
#endif
}

void MainWindow::unregisterGlobalHotkeys()
{
#ifdef Q_OS_WIN
    if (!m_globalHotkeysRegistered) return;
    UnregisterHotKey((HWND)winId(), HK_PlayPause);
    UnregisterHotKey((HWND)winId(), HK_Prev);
    UnregisterHotKey((HWND)winId(), HK_Next);
    UnregisterHotKey((HWND)winId(), HK_Stop);
    UnregisterHotKey((HWND)winId(), HK_ShowHide);
    UnregisterHotKey((HWND)winId(), HK_SummonWin);
    m_globalHotkeysRegistered = false;
#endif
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "WM_HOTKEY") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg && msg->message == WM_HOTKEY) {
            const int id = (int)msg->wParam;
            switch (id) {
            case HK_PlayPause:
                onPlayClicked();
                break;
            case HK_Prev:
                onPrevClicked();
                break;
            case HK_Next:
                onNextClicked();
                break;
            case HK_Stop:
                onStopClicked();
                break;
            case HK_ShowHide:
                if (isVisible() && !isMinimized()) {
                    if (m_trayIcon && m_trayIcon->isVisible()) {
                        hide();
                    } else {
                        showMinimized();
                    }
                } else {
                    // 清除可能被记住的 Maximized 状态，避免召唤后变成全屏
                    setWindowState(windowState() & ~Qt::WindowMaximized);
                    showNormal();
                    raise();
                    activateWindow();
                    applyAlwaysOnTop();
                }
                break;
            case HK_SummonWin:
                // 强制把窗口拉到前台：无论是否可见/最小化/关闭
                // 关键：先清掉 Maximized 状态，再 showNormal，否则可能被记住的最大化状态导致召唤后变成全屏
                setWindowState(windowState() & ~Qt::WindowMaximized);
                showNormal();
                raise();
                activateWindow();
                applyAlwaysOnTop();
                break;
            default:
                break;
            }
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

// =================================================================
//  毛玻璃 (Acrylic / Frosted Glass) 背景
// =================================================================

static QImage boxBlur(const QImage &src, int radius)
{
    if (radius <= 0 || src.isNull()) return src.copy();
    QImage tmp = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = tmp.width();
    const int h = tmp.height();

    // 水平方向盒式模糊 (in-place 不可，使用额外缓冲)
    QImage buf = tmp;
    const int diameter = radius * 2 + 1;
    QVector<quint32> sums(w * 4, 0);
    for (int y = 0; y < h; ++y) {
        const quint32 *srcLine = reinterpret_cast<const quint32 *>(tmp.constScanLine(y));
        quint32 *dstLine = reinterpret_cast<quint32 *>(buf.scanLine(y));
        // 初始化累加器
        for (int c = 0; c < 4; ++c) sums[c] = 0;
        for (int x = -radius; x <= radius; ++x) {
            int sx = qBound(0, x, w - 1);
            quint32 px = srcLine[sx];
            sums[0] += (px >> 24) & 0xFF;
            sums[1] += (px >> 16) & 0xFF;
            sums[2] += (px >> 8) & 0xFF;
            sums[3] += px & 0xFF;
        }
        for (int x = 0; x < w; ++x) {
            quint32 out = ((sums[0] / diameter) << 24)
                        | ((sums[1] / diameter) << 16)
                        | ((sums[2] / diameter) << 8)
                        |  (sums[3] / diameter);
            dstLine[x] = out;
            // 滑动窗口
            int xOut = x - radius;
            int xIn = x + radius + 1;
            quint32 pxOut = srcLine[qBound(0, xOut, w - 1)];
            quint32 pxIn  = srcLine[qBound(0, xIn, w - 1)];
            for (int c = 0; c < 4; ++c) {
                quint32 mask = 0xFFu << (c * 8);
                quint32 shift = c * 8;
                sums[c] = sums[c] - ((pxOut & mask) >> shift) + ((pxIn & mask) >> shift);
            }
        }
    }
    // 垂直方向
    QImage out = buf;
    QVector<quint32> vsums(h * 4, 0);
    for (int x = 0; x < w; ++x) {
        const quint32 *srcColStart = reinterpret_cast<const quint32 *>(buf.constBits());
        for (int c = 0; c < 4; ++c) vsums[c] = 0;
        for (int y = -radius; y <= radius; ++y) {
            int sy = qBound(0, y, h - 1);
            quint32 px = reinterpret_cast<const quint32 *>(buf.constScanLine(sy))[x];
            vsums[0] += (px >> 24) & 0xFF;
            vsums[1] += (px >> 16) & 0xFF;
            vsums[2] += (px >> 8) & 0xFF;
            vsums[3] += px & 0xFF;
        }
        for (int y = 0; y < h; ++y) {
            quint32 *dstLine = reinterpret_cast<quint32 *>(out.scanLine(y));
            quint32 val = ((vsums[0] / diameter) << 24)
                        | ((vsums[1] / diameter) << 16)
                        | ((vsums[2] / diameter) << 8)
                        |  (vsums[3] / diameter);
            dstLine[x] = val;
            int yOut = y - radius;
            int yIn = y + radius + 1;
            quint32 pxOut = reinterpret_cast<const quint32 *>(buf.constScanLine(qBound(0, yOut, h - 1)))[x];
            quint32 pxIn  = reinterpret_cast<const quint32 *>(buf.constScanLine(qBound(0, yIn, h - 1)))[x];
            for (int c = 0; c < 4; ++c) {
                quint32 mask = 0xFFu << (c * 8);
                quint32 shift = c * 8;
                vsums[c] = vsums[c] - ((pxOut & mask) >> shift) + ((pxIn & mask) >> shift);
            }
        }
    }
    return out;
}

void MainWindow::scheduleBackgroundRefresh()
{
    if (!m_bgRefreshTimer->isActive()) {
        m_bgRefreshTimer->start();
    }
}

void MainWindow::updateBlurBackground()
{
    if (!isVisible()) return;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect g = frameGeometry();
    if (g.width() < 2 || g.height() < 2) return;

    // 在多屏环境下取窗口所在屏幕
    QScreen *targetScreen = QGuiApplication::screenAt(QPoint(g.center()));
    if (!targetScreen) targetScreen = screen;

    QPixmap pm = targetScreen->grabWindow(0, g.x(), g.y(), g.width(), g.height());
    if (pm.isNull()) return;

    QImage img = pm.toImage();
    // 通过降采样 + 升采样来近似模糊（性能好、效果自然）
    const int downscaleFactor = 6;
    int sw = qMax(1, img.width() / downscaleFactor);
    int sh = qMax(1, img.height() / downscaleFactor);
    QImage small = img.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage blurred = boxBlur(small, 3);
    QImage final = blurred.scaled(img.width(), img.height(),
                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_bgCache = QPixmap::fromImage(final);
    m_bgCacheDirty = false;

    update();
}

void MainWindow::applyAlwaysOnTop()
{
#ifdef Q_OS_WIN
    if (!m_pinnedTop) return;  // 用户主动取消置顶
    // Windows API 级别强制置顶
    //  - SetWindowPos + HWND_TOPMOST  比 Qt::WindowStaysOnTopHint 更"硬"
    //  - SWP_NOMOVE | SWP_NOSIZE 不动当前位置和大小
    //  - SWP_NOACTIVATE 不抢焦点
    HWND hwnd = (HWND)winId();
    if (hwnd) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#endif
}

void MainWindow::onTogglePinTop()
{
    m_pinnedTop = !m_pinnedTop;
    if (m_pinnedTop) {
        // 重新置顶
        applyAlwaysOnTop();
    } else {
#ifdef Q_OS_WIN
        // 取消置顶：把窗口从 TOPMOST 拿下来，放回普通 Z-order 末尾
        HWND hwnd = (HWND)winId();
        if (hwnd) {
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
#endif
    }
    updatePinTopIcon();
}

void MainWindow::updatePinTopIcon()
{
    if (!m_pinTopBtn) return;
    m_pinTopBtn->setIcon(createPinTopIcon(64, !m_pinnedTop));
    m_pinTopBtn->setToolTip(m_pinnedTop ? tr("取消置顶") : tr("置顶"));
}

QIcon MainWindow::createPinTopIcon(int size, bool crossed)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal cx = size / 2.0;
    const qreal cy = size / 2.0;

    // 画一个 45° 倾斜的"工字钉"
    p.save();
    p.translate(cx, cy);
    p.rotate(-35);  // 顺时针针尖指向右
    p.translate(-cx, -cy);

    // 工字头（三段）—— 比歌词按钮那个稍微紧凑些
    auto drawRoundedBar = [&](qreal y, qreal w, qreal h) {
        QRectF r(cx - w / 2, cy + y, w, h);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(245, 248, 252, 255));  // 主体白
        p.drawRoundedRect(r, h * 0.45, h * 0.45);
    };
    drawRoundedBar(-size * 0.40, size * 0.62, size * 0.10);  // 顶盘
    drawRoundedBar(-size * 0.18, size * 0.34, size * 0.07);  // 中横
    drawRoundedBar(-size * 0.02, size * 0.58, size * 0.09);  // 底盘

    // 针身：梯形（根宽 → 尖锐）
    QPolygonF needle;
    needle << QPointF(cx - size * 0.05, cy + size * 0.07)
           << QPointF(cx + size * 0.05, cy + size * 0.07)
           << QPointF(cx + size * 0.012, cy + size * 0.45)
           << QPointF(cx - size * 0.012, cy + size * 0.45);
    p.setBrush(QColor(220, 226, 235, 255));
    p.drawPolygon(needle);
    p.restore();

    // 取消置顶状态：在整个图标上画一条 45° 斜线
    if (crossed) {
        const qreal pad = size * 0.08;
        QPen slashPen(QColor(255, 90, 90, 235),
                      qMax(2.0, size * 0.10),
                      Qt::SolidLine, Qt::RoundCap);
        p.setPen(slashPen);
        // 左下 → 右上
        p.drawLine(QPointF(pad, size - pad),
                   QPointF(size - pad, pad));
    }
    p.end();
    return QIcon(pm);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_bgCache.isNull()) {
        // 还没抓取到屏幕，先画一个深色背景防止穿透
        QPainter p(this);
        p.fillRect(rect(), QColor(20, 24, 36, 240));
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 圆角裁剪
    const qreal radius = 14.0;
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), radius, radius);
    p.setClipPath(path);

    // 绘制模糊后的屏幕内容
    p.drawPixmap(0, 0, m_bgCache);

    // Win11 风格叠加：渐变 + 噪点
    QLinearGradient grad(0, 0, width(), height());
    grad.setColorAt(0.0, QColor(32, 38, 56, 175));
    grad.setColorAt(0.5, QColor(20, 24, 38, 195));
    grad.setColorAt(1.0, QColor(14, 18, 28, 210));
    p.fillPath(path, grad);

    // 顶部高光（增加磨砂立体感）
    QLinearGradient topHL(0, 0, 0, height() * 0.5);
    topHL.setColorAt(0.0, QColor(255, 255, 255, 38));
    topHL.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillPath(path, topHL);

    p.end();
}
