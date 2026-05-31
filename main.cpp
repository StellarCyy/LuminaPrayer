#include "widget.h"
#include "profilemanager.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QMutex>

// H-08: Structured file logger
static QFile   g_logFile;
static QMutex  g_logMutex;

static void luminaLogHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx)
    const char *level = "UNK";
    switch (type) {
    case QtDebugMsg:    level = "DBG"; break;
    case QtInfoMsg:     level = "INF"; break;
    case QtWarningMsg:  level = "WRN"; break;
    case QtCriticalMsg: level = "ERR"; break;
    case QtFatalMsg:    level = "FTL"; break;
    }

    const QString line = QStringLiteral("%1 [%2] %3\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
             QLatin1String(level),
             msg);

    QMutexLocker lock(&g_logMutex);
    if (g_logFile.isOpen()) {
        g_logFile.write(line.toUtf8());
        g_logFile.flush();
    }

    // Also forward to stderr for debug builds
#ifndef QT_NO_DEBUG_OUTPUT
    fprintf(stderr, "%s", line.toUtf8().constData());
#endif
}

int main(int argc, char *argv[])
{
    #ifdef Q_OS_WIN
        qputenv("QT_MEDIA_BACKEND", "windows");
    #endif

    QApplication a(argc, argv);

    a.setApplicationName("LuminaPrayer");
    a.setApplicationDisplayName("Lumina Prayer");

    // H-08: Install structured file logger
    const QString logPath = QCoreApplication::applicationDirPath() + QStringLiteral("/lumina.log");
    g_logFile.setFileName(logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(luminaLogHandler);
    qInfo() << "=== LuminaPrayer started ===";

  try {

    // Load character profile JSON (safe defaults if missing)
    const QString profilePath = QCoreApplication::applicationDirPath() + "/character.json";
    ProfileManager::instance()->loadFromFile(profilePath);

    Widget w;

    QSystemTrayIcon sysTray(QIcon(ProfileManager::instance()->sprites().icon), &w);

    auto *menu = new QMenu(&w);
    auto showAct = new QAction("从天国降临", menu);
    auto settingsAct = new QAction("设置参数", menu);
    auto exitAct = new QAction("关闭", menu);

    QObject::connect(showAct,&QAction::triggered,[&](){
        w.showFromTray();
    });
    QObject::connect(settingsAct, &QAction::triggered, [&](){
        w.openSettingsDialog();
    });
    QObject::connect(exitAct,&QAction::triggered,[&](){
        QApplication::quit();
    });
    menu->addAction(showAct);
    menu->addAction(settingsAct);
    menu->addAction(exitAct);

    sysTray.setContextMenu(menu);
    sysTray.show();

    w.show();

    const int rc = a.exec();
    qInfo() << "=== LuminaPrayer exiting normally ===";
    g_logFile.flush();
    g_logFile.close();
    return rc;

  } catch (const std::exception &ex) {
    qCritical() << "FATAL uncaught exception:" << ex.what();
    g_logFile.flush();
    g_logFile.close();
    return 1;
  } catch (...) {
    qCritical() << "FATAL unknown exception";
    g_logFile.flush();
    g_logFile.close();
    return 1;
  }
}
