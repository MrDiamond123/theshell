#include "mainwindow.h"
#include "background.h"
#include "loginsplash.h"
#include "segfaultdialog.h"
#include "globalfilter.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QProcess>
#include <QThread>
#include <QAbstractNativeEventFilter>
#include <QUrl>
#include <QMediaPlayer>
#include <QDebug>
#include <QSettings>
#include <QInputDialog>
#include <signal.h>
#include <unistd.h>

MainWindow* MainWin = NULL;

void catch_sigsegv(int signal) {
    qDebug() << "SEGFAULT! Quitting now!";
    if (MainWin != NULL) {
        MainWin->close();
        delete MainWin;
    }
    SegfaultDialog* dialog = new SegfaultDialog();
    dialog->exec();
    std::abort();
    //QApplication::exit(1);
}

int main(int argc, char *argv[])
{
    struct sigaction segvact;
    segvact.sa_handler = catch_sigsegv;
    sigemptyset(&segvact.sa_mask);
    segvact.sa_flags = 0;
    sigaction(SIGSEGV, &segvact, 0);

    QApplication a(argc, argv);

    bool showSplash = true;
    bool autoStart = true;

    QStringList args = a.arguments();
    args.removeFirst();
    for (QString arg : a.arguments()) {
        if (arg == "--help" || arg == "-h") {
            qDebug() << "theShell";
            qDebug() << "Usage: theshell [OPTIONS]";
            qDebug() << "  -s, --no-splash-screen       Don't show the splash screen";
            qDebug() << "  -a, --no-autostart           Don't autostart executables";
            qDebug() << "  -h, --help                   Show this help output";
            return 0;
        } else if (arg == "-s" || arg == "--no-splash-screen") {
            showSplash = false;
        } else if (arg == "-a" || arg == "--no-autostart") {
            autoStart = false;
        }
    }

    a.setOrganizationName("theSuite");
    a.setOrganizationDomain("");
    a.setApplicationName("theShell");

    new GlobalFilter(&a);

    QFile lockfile(QDir::home().absolutePath() + "/.theshell.lck");
    if (lockfile.exists()) {
        if (QMessageBox::warning(0, "theShell already running", "theShell seems to already be running. "
                                                               "Do you wish to start theShell anyway?",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            return 0;
        }
    }

    lockfile.open(QFile::WriteOnly);
    lockfile.write(QByteArray());
    lockfile.close();

    QSettings settings;

    QString windowManager = settings.value("startup/WindowManagerCommand", "").toString();
    if (windowManager == "") {
        windowManager = "kwin_x11";
        settings.setValue("startup/WindowManagerCommand", windowManager);
    }

    if (autoStart) {
        QStringList autostartApps = settings.value("startup/autostart", "").toString().split(",");
        for (QString app : autostartApps) {
            QProcess::startDetached(app);
        }
    }

    while (!QProcess::startDetached(windowManager)) {
        windowManager = QInputDialog::getText(0, "Window Manager couldn't start",
                              "The window manager \"" + windowManager + "\" could not start. \n\n"
                              "Enter the name or path of a window manager to attempt to start a different window"
                              "manager, or hit 'Cancel' to start theShell without a window manager.");
        if (windowManager == "") {
            break;
        }
    }


    if (showSplash) {
        LoginSplash* splash = new LoginSplash();
        splash->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        splash->showFullScreen();
    }

    QProcess* ksuperkey = new QProcess();
    ksuperkey->start("ksuperkey -d -e \"Super_L=Alt_L|F5;Alt_R|F5\"");

    QRect screenGeometry = QApplication::desktop()->screenGeometry();
    MainWin = new MainWindow();
    Background b(MainWin);
    b.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint);
    b.setGeometry(screenGeometry);
    b.showFullScreen();
    b.setAttribute(Qt::WA_X11NetWmWindowTypeDesktop, true);

    MainWin->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    MainWin->setGeometry(screenGeometry.x() - 1, screenGeometry.y(), screenGeometry.width() + 1, MainWin->height());
    MainWin->show();

    QThread::sleep(1);

    return a.exec();
}

void playSound(QUrl location, bool uncompressed = false) {
    if (uncompressed) {
        QSoundEffect* sound = new QSoundEffect();
        sound->setSource(location);
        sound->play();
    } else {
        QMediaPlayer* sound = new QMediaPlayer();
        sound->setMedia(location);
        sound->play();
    }
}

QIcon getIconFromTheme(QString name, QColor textColor) {
    int averageCol = (textColor.red() + textColor.green() + textColor.blue()) / 3;

    if (averageCol <= 127) {
        return QIcon(":/icons/dark/images/dark/" + name);
    } else {
        return QIcon(":/icons/light/images/light/" + name);
    }
}

void EndSession(EndSessionWait::shutdownType type) {
    EndSessionWait* w = new EndSessionWait(type);
    w->showFullScreen();
    QApplication::setOverrideCursor(Qt::BlankCursor);
}