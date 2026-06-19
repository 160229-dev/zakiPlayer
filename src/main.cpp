#include "mainwindow.h"
#include "settingsmanager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QLocale>
#include <QLocalSocket>

static const char *kIpcServerName = "zakiplayer-singleton-v1";

// 尝试连接已运行的实例；连接成功就发出"show"指令并返回 true
static bool tryForwardToExistingInstance(const QStringList &paths)
{
    QLocalSocket sock;
    sock.connectToServer(QString::fromLatin1(kIpcServerName));
    if (!sock.waitForConnected(250)) {
        return false;
    }
    QByteArray msg = "show\n";
    if (!paths.isEmpty()) {
        msg += paths.join(QChar('\u0001')).toUtf8();
        msg += '\n';
    }
    sock.write(msg);
    sock.flush();
    sock.waitForBytesWritten(500);
    sock.disconnectFromServer();
    return true;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("ZakiPlayer"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zakiplayer.local"));
    QCoreApplication::setApplicationName(QStringLiteral("zakiPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // 解析命令行：接受若干文件/文件夹作为启动播放列表
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("zakiPlayer - A cross-platform multimedia player"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Media files or directories to play");
    parser.process(app);

    const QStringList paths = parser.positionalArguments();

    // 单实例：第二次启动时通知第一次的实例显示窗口并退出
    if (tryForwardToExistingInstance(paths)) {
        return 0;
    }

    MainWindow w;
    w.show();

    if (!paths.isEmpty()) {
        w.setStartupPlaylist(paths);
    }

    return app.exec();
}
