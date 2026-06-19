#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QPoint>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QShortcut>
#include <QStackedWidget>
#include <QMediaPlayer>
#include <QStringList>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QEvent>
#include <QTimer>
#include <QPixmap>
#include <QLocalServer>

#include "settingsmanager.h"
#include "lyricsview.h"
#include "pinnedlyrics.h"
#include "fullscreenlyrics.h"
#include "toast.h"

class QVideoWidget;
class QSlider;
class QPushButton;
class QToolButton;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QTimer;
class QResizeEvent;

class PlayerController;
class PlaylistModel;
class PlaylistWidget;
class Visualizer;
class AudioSpectrum;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setStartupPlaylist(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    // 全局热键
    void registerGlobalHotkeys();
    void unregisterGlobalHotkeys();

private slots:
    void onPlayClicked();
    void onPrevClicked();
    void onNextClicked();
    void onStopClicked();
    void onMuteClicked();
    void onModeClicked();
    void onOpenFilesClicked();
    void onOpenFolderClicked();
    void onClearPlaylistClicked();
    void onRemoveSelectedClicked();
    void onPositionSliderPressed();
    void onPositionSliderReleased();
    void onPositionSliderMoved(int value);
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onVolumeChanged(int vol);
    void onMutedChanged(bool muted);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onEndOfMedia();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onErrorOccurred(const QString &msg);
    void onSpectrumBands(const QVector<qreal> &bands);
    void onCoverArtChanged();
    void onCurrentMediaChanged();
    void onPlaylistPlayRequested(int index);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onSeekForward();
    void onSeekBackward();
    void onVolumeUp();
    void onVolumeDown();
    void onUpdatePlayModeButton();
    void onEqualizerToggle(bool active);
    void onShowContextMenu(const QPoint &pos);
    void onLyricsPinRequested();
    void onLyricsFullscreenRequested();

private:
    void buildUi();
    void applyStylesheet();
    void wireSignals();
    void updateTitle();
    QString formatTime(qint64 ms) const;
    void playAt(int index);
    void handleDroppedUrls(const QList<QUrl> &urls);
    void setupShortcuts();
    void setupTray();
    void startSingleInstanceServer();
    void onIpcNewConnection();
    void loadSettings();
private:
    void applyAlwaysOnTop();
    void saveSettings();
    void applyPlayMode(SettingsManager::PlayMode mode);
    void decideNextAfterEnd();
    void onTogglePinTop();
    void updatePinTopIcon();
    static QIcon createPinTopIcon(int size, bool crossed);

    // 全局热键 ID（与 Windows RegisterHotKey 的 id 一一对应）
    enum GlobalHotkeyId {
        HK_PlayPause = 0xB001,
        HK_Prev      = 0xB002,
        HK_Next      = 0xB003,
        HK_Stop      = 0xB004,
        HK_ShowHide  = 0xB005,
        HK_SummonWin = 0xB006,  // 召唤主窗口 Ctrl+Alt+W
    };
    bool m_globalHotkeysRegistered = false;
    void updateCoverDisplay();
    void loadLyricsForCurrent();
    void setUiEnabled(bool enabled);
    void appendToPlaylist(const QList<QUrl> &urls);
    QMenu* buildPlaylistContextMenu(int clickedRow);
    QMenu* buildModeMenu();
    QIcon createTriangleIcon(int size = 64) const;

    // UI
    QWidget *m_centralWidget;
    QLabel *m_coverLabel;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    QLabel *m_positionLabel;
    QLabel *m_durationLabel;
    QLabel *m_statusLabel;

    // 控件
    QPushButton *m_playButton;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QPushButton *m_stopButton;
    QPushButton *m_muteButton;
    QToolButton *m_modeButton;
    QPushButton *m_openFilesButton;
    QPushButton *m_openFolderButton;
    QPushButton *m_clearButton;

    QSlider *m_positionSlider;
    QSlider *m_volumeSlider;

    Visualizer *m_visualizer;
    LyricsView *m_lyricsView;
    PlaylistWidget *m_playlistView;
    PinnedLyricsWindow *m_pinnedLyrics = nullptr;
    FullscreenLyricsWindow *m_fullscreenLyrics = nullptr;
    QPushButton *m_pinTopBtn = nullptr;
    bool m_pinnedTop = true;

    QSystemTrayIcon *m_trayIcon;
    QLocalServer *m_localServer;
    QMenu *m_trayMenu;

    // 数据
    PlayerController *m_player;
    PlaylistModel *m_playlistModel;
    AudioSpectrum *m_spectrum;
    SettingsManager::PlayMode m_playMode;
    QTimer *m_positionUpdateTimer;

    // 状态
    bool m_userSeeking;
    bool m_updatingPosition;
    bool m_dragging;
    QPoint m_dragPos;
    QSize m_normalSize;

    // 毛玻璃背景缓存
    QPixmap m_bgCache;
    QTimer *m_bgRefreshTimer;
    bool m_bgCacheDirty;

    void updateBlurBackground();
    void scheduleBackgroundRefresh();
};

#endif // MAINWINDOW_H
