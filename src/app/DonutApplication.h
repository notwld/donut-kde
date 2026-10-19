// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <memory>

class QQmlEngine;
class QQuickWindow;

namespace Donut
{
class DonutController;
class DragBackend;
class FileActionService;
class OverlayWindow;
class SettingsManager;
class ShortcutManager;

class DonutApplication : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        Daemon, ///< KWin backend + layer-shell overlay + global shortcut
        Window, ///< drop-target fallback window
        Demo, ///< dry-run demo window
    };

    explicit DonutApplication(Mode mode, QObject *parent = nullptr);
    ~DonutApplication() override;

    bool init();
    /// Opens the donut with these files at the pointer (daemon) or in the window.
    void openWithFiles(const QList<QUrl> &urls);
    /// Opens the empty donut at the pointer for dropping files (shortcut).
    void activateAtPointer();

public Q_SLOTS:
    void showSettings();

public:
    /// Arguments forwarded by a second `donut ...` invocation (KDBusService).
    void handleRemoteArguments(const QStringList &arguments, const QString &workingDirectory);

private:
    void applySettings();
    void openAtPointer(const QList<QUrl> &urls);

    Mode m_mode;
    std::unique_ptr<QQmlEngine> m_engine;
    SettingsManager *m_settings = nullptr;
    FileActionService *m_service = nullptr;
    DonutController *m_controller = nullptr;
    DragBackend *m_backend = nullptr;
    ShortcutManager *m_shortcuts = nullptr;
    std::unique_ptr<OverlayWindow> m_overlay;
    QPointer<QQuickWindow> m_window;
    QPointer<QQuickWindow> m_settingsWindow;
};

/// Registers Donut's C++ types for QML (also used by the QML tests).
void registerQmlTypes();
}
