// SPDX-License-Identifier: GPL-2.0-or-later
#include "DonutApplication.h"
#include "Autostart.h"
#include "DemoBackend.h"
#include "DonutController.h"
#include "DropTargetBackend.h"
#include "FileActionService.h"
#include "IconProvider.h"
#include "KWinBackend.h"
#include "Logging.h"
#include "OverlayWindow.h"
#include "SettingsManager.h"
#include "ShortcutManager.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QtQml>

using namespace Qt::StringLiterals;

namespace Donut
{
void registerQmlTypes()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    const char *uri = "org.kde.donut.core";
    qmlRegisterUncreatableMetaObject(Donut::staticMetaObject, uri, 1, 0, "Donut", u"Enums only"_s);
    qmlRegisterUncreatableType<DonutController>(uri, 1, 0, "DonutController", u"Created in C++"_s);
    qmlRegisterUncreatableType<SettingsManager>(uri, 1, 0, "SettingsManager", u"Created in C++"_s);
    qmlRegisterUncreatableType<ShortcutManager>(uri, 1, 0, "ShortcutManager", u"Created in C++"_s);
    qmlRegisterUncreatableType<DragBackend>(uri, 1, 0, "DragBackend", u"Created in C++"_s);
}

DonutApplication::DonutApplication(Mode mode, QObject *parent)
    : QObject(parent)
    , m_mode(mode)
{
}

DonutApplication::~DonutApplication()
{
    m_overlay.reset();
    delete m_window;
    delete m_settingsWindow;
}

bool DonutApplication::init()
{
    registerQmlTypes();
    m_engine = std::make_unique<QQmlEngine>();
    m_engine->addImageProvider(u"donuticon"_s, new IconProvider);

    m_settings = new SettingsManager({}, this);
    setDiagnosticLogging(m_settings->diagnosticLogging());
    m_service = new FileActionService(this);
    m_service->setReportJobsToPlasma(m_mode != Mode::Demo);
    m_controller = new DonutController(m_settings, m_service, this);
    m_shortcuts = new ShortcutManager(this);

    switch (m_mode) {
    case Mode::Daemon:
        m_backend = new KWinBackend(this);
        break;
    case Mode::Window:
        m_backend = new DropTargetBackend(this);
        break;
    case Mode::Demo:
        m_backend = new DemoBackend(this);
        break;
    }
    m_controller->setBackend(m_backend);
    m_backend->start();
    qCInfo(lcCore) << "mode:" << m_backend->backendName();

    connect(m_settings, &SettingsManager::saved, this, &DonutApplication::applySettings);

    if (m_mode == Mode::Daemon) {
        m_overlay = std::make_unique<OverlayWindow>(m_engine.get(), m_controller, m_settings);
        if (!m_overlay->isValid()) {
            return false;
        }
        connect(m_shortcuts, &ShortcutManager::activated, this, &DonutApplication::activateAtPointer);
        if (!m_shortcuts->apply(m_settings->shortcut())) {
            qCWarning(lcCore) << m_shortcuts->status();
        }
        connect(m_backend, &DragBackend::availabilityChanged, this, [this] {
            qCInfo(lcBackend) << m_backend->modeDescription();
        });
        return true;
    }

    // Window and demo modes: a regular window that stays open.
    m_controller->setPersistent(true);
    QQmlComponent component(m_engine.get());
    component.loadFromModule("org.kde.donut", "StandaloneWindow");
    QObject *object = component.createWithInitialProperties({
        {u"controller"_s, QVariant::fromValue(m_controller)},
        {u"settings"_s, QVariant::fromValue(m_settings)},
        {u"demo"_s, m_mode == Mode::Demo},
    });
    m_window = qobject_cast<QQuickWindow *>(object);
    if (!m_window) {
        qCCritical(lcUi) << "failed to create window:" << component.errorString();
        delete object;
        return false;
    }
    connect(m_window, &QQuickWindow::closing, qApp, &QCoreApplication::quit);
    connect(m_window, SIGNAL(settingsRequested()), this, SLOT(showSettings()));
    m_controller->openManually({}, true);
    m_controller->notifyOpened();
    m_window->show();
    return true;
}

void DonutApplication::applySettings()
{
    setDiagnosticLogging(m_settings->diagnosticLogging());
    Autostart::setEnabled(m_settings->autostart(), QCoreApplication::applicationFilePath());
    if (m_mode == Mode::Daemon) {
        m_shortcuts->apply(m_settings->shortcut());
        KWinBackend::reconfigureEffect();
    }
}

void DonutApplication::openAtPointer(const QList<QUrl> &urls)
{
    auto open = [this, urls](std::optional<QPointF> pos) {
        QPointF where;
        if (pos) {
            where = *pos;
        } else if (QScreen *s = QGuiApplication::primaryScreen()) {
            // Without the effect a Wayland client cannot know the pointer position.
            where = QRectF(s->geometry()).center();
        }
        m_controller->openManually(where, true, urls);
    };
    m_backend->requestCursorPosition(open);
}

void DonutApplication::activateAtPointer()
{
    if (m_mode != Mode::Daemon) {
        return;
    }
    if (m_controller->menuOpen()) {
        m_controller->dismiss(); // the shortcut toggles
        return;
    }
    openAtPointer({});
}

void DonutApplication::openWithFiles(const QList<QUrl> &urls)
{
    if (m_mode == Mode::Daemon) {
        openAtPointer(urls);
    } else {
        m_controller->setPayloadUrls(urls);
        if (m_window) {
            m_window->raise();
            m_window->requestActivate();
        }
    }
}

void DonutApplication::showSettings()
{
    if (!m_settingsWindow) {
        QQmlComponent component(m_engine.get());
        component.loadFromModule("org.kde.donut", "SettingsPage");
        QObject *object = component.createWithInitialProperties({
            {u"settings"_s, QVariant::fromValue(m_settings)},
            {u"shortcuts"_s, QVariant::fromValue(m_shortcuts)},
            {u"backend"_s, QVariant::fromValue(m_backend)},
        });
        m_settingsWindow = qobject_cast<QQuickWindow *>(object);
        if (!m_settingsWindow) {
            qCCritical(lcUi) << "failed to create settings:" << component.errorString();
            delete object;
            return;
        }
    }
    m_settings->load();
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->requestActivate();
}

void DonutApplication::handleRemoteArguments(const QStringList &arguments, const QString &workingDirectory)
{
    QCommandLineParser parser;
    parser.addOption({u"settings"_s, {}});
    parser.addOption({u"activate"_s, {}});
    parser.addOption({u"open"_s, {}});
    parser.addPositionalArgument(u"files"_s, {});
    parser.parse(arguments);
    if (parser.isSet(u"settings"_s)) {
        showSettings();
        return;
    }
    if (parser.isSet(u"activate"_s)) {
        activateAtPointer();
        return;
    }
    QList<QUrl> urls;
    const QDir cwd(workingDirectory);
    for (const QString &arg : parser.positionalArguments()) {
        urls << QUrl::fromUserInput(arg, cwd.absolutePath(), QUrl::AssumeLocalFile);
    }
    if (!urls.isEmpty()) {
        openWithFiles(urls);
    }
}
}
