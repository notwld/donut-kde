// SPDX-License-Identifier: GPL-2.0-or-later
#include "DonutApplication.h"
#include "Diagnostics.h"
#include "SettingsManager.h"
#include "donut_version.h"

#include <KDBusService>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QQmlEngineExtensionPlugin>
#include <QQuickWindow>

#include <cstdio>

Q_IMPORT_QML_PLUGIN(org_kde_donutPlugin)

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    // Translucent overlay surfaces need an alpha channel.
    QQuickWindow::setDefaultAlphaBuffer(true);

    // QApplication (not QGuiApplication) so KDE's native file dialogs work.
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"donut"_s);
    QApplication::setApplicationDisplayName(u"Donut"_s);
    QApplication::setApplicationVersion(QStringLiteral(DONUT_VERSION_STRING));
    QApplication::setOrganizationDomain(u"kde.org"_s);
    QApplication::setDesktopFileName(u"org.kde.donut"_s);
    QApplication::setWindowIcon(QIcon::fromTheme(u"org.kde.donut"_s, QIcon(u":/icons/org.kde.donut.svg"_s)));
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Radial drag action menu for KDE Plasma"_s);
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption windowOpt(u"window"_s, u"Open the drop-target window (works without the KWin effect)."_s);
    const QCommandLineOption demoOpt(u"demo"_s, u"Open the manual demo window. Actions are simulated; files are never touched."_s);
    const QCommandLineOption settingsOpt(u"settings"_s, u"Show the settings page."_s);
    const QCommandLineOption activateOpt(u"activate"_s, u"Open the donut at the pointer (same as the global shortcut)."_s);
    const QCommandLineOption openOpt(u"open"_s, u"Open the donut at the pointer with the given files."_s);
    const QCommandLineOption diagnosticsOpt(u"diagnostics"_s, u"Print environment, backend and integration status, then exit."_s);
    parser.addOptions({windowOpt, demoOpt, settingsOpt, activateOpt, openOpt, diagnosticsOpt});
    parser.addPositionalArgument(u"files"_s, u"Files or folders to act on."_s, u"[files...]"_s);
    parser.process(app);

    if (parser.isSet(diagnosticsOpt)) {
        Donut::SettingsManager settings;
        std::fputs(qPrintable(Donut::diagnosticsReport(settings)), stdout);
        return 0;
    }

    QList<QUrl> files;
    for (const QString &arg : parser.positionalArguments()) {
        files << QUrl::fromUserInput(arg, QDir::currentPath(), QUrl::AssumeLocalFile);
    }

    using Mode = Donut::DonutApplication::Mode;
    const Mode mode = parser.isSet(demoOpt) ? Mode::Demo : parser.isSet(windowOpt) ? Mode::Window : Mode::Daemon;

    std::unique_ptr<KDBusService> service;
    if (mode == Mode::Daemon) {
        // Single instance: a second `donut --open ...` forwards its arguments here and exits.
        service = std::make_unique<KDBusService>(KDBusService::Unique);
    }

    Donut::DonutApplication donut(mode);
    if (!donut.init()) {
        return 1;
    }
    if (service) {
        QObject::connect(service.get(), &KDBusService::activateRequested, &donut, &Donut::DonutApplication::handleRemoteArguments);
    }

    if (parser.isSet(settingsOpt)) {
        donut.showSettings();
    } else if (parser.isSet(activateOpt)) {
        donut.activateAtPointer();
    } else if (!files.isEmpty()) {
        donut.openWithFiles(files);
    }
    return app.exec();
}
