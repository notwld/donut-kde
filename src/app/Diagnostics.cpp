// SPDX-License-Identifier: GPL-2.0-or-later
#include "Diagnostics.h"
#include "Autostart.h"
#include "DonutConfig.h"
#include "SettingsManager.h"
#include "donut_version.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Donut
{
namespace
{
QString env(const char *name)
{
    const QString v = qEnvironmentVariable(name);
    return v.isEmpty() ? u"(unset)"_s : v;
}

QDBusMessage call(const QString &service, const QString &path, const QString &iface, const QString &method, const QVariantList &args = {})
{
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, iface, method);
    msg.setArguments(args);
    return QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
}

QString runningKWinVersion()
{
    const QDBusMessage reply = call(u"org.kde.KWin"_s, u"/KWin"_s, u"org.kde.KWin"_s, u"supportInformation"_s);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    const QString info = reply.arguments().constFirst().toString();
    for (QStringView line : QStringView(info).split(u'\n')) {
        if (line.startsWith(u"KWin version:")) {
            return line.mid(13).trimmed().toString();
        }
    }
    return {};
}

QStringList installedEffectCopies()
{
    QStringList dirs = QCoreApplication::libraryPaths();
    dirs << QDir::homePath() + u"/.local/lib/plugins"_s << QDir::homePath() + u"/.local/lib/qt6/plugins"_s << u"/usr/lib/qt6/plugins"_s;
    dirs.removeDuplicates();
    QStringList found;
    for (const QString &d : std::as_const(dirs)) {
        const QString path = d + u"/kwin/effects/plugins/"_s + QString::fromLatin1(Config::EffectId) + u".so"_s;
        if (QFileInfo::exists(path)) {
            found << path;
        }
    }
    return found;
}
}

QString diagnosticsReport(const SettingsManager &settings)
{
    QStringList out;
    auto line = [&out](const QString &key, const QString &value) {
        out << u"%1 %2"_s.arg(key + u':', -28).arg(value);
    };

    out << u"Donut diagnostics"_s << u"================="_s;
    line(u"Donut version"_s, QStringLiteral(DONUT_VERSION_STRING));
    line(u"Qt runtime"_s, QString::fromLatin1(qVersion()));
    line(u"Qt platform plugin"_s, QGuiApplication::platformName());
    line(u"XDG_SESSION_TYPE"_s, env("XDG_SESSION_TYPE"));
    line(u"XDG_CURRENT_DESKTOP"_s, env("XDG_CURRENT_DESKTOP"));
    line(u"WAYLAND_DISPLAY"_s, env("WAYLAND_DISPLAY"));

    const bool kwinOnBus = QDBusConnection::sessionBus().interface()->isServiceRegistered(u"org.kde.KWin"_s);
    const QString kwinVersion = kwinOnBus ? runningKWinVersion() : QString();
    line(u"KWin on session bus"_s, kwinOnBus ? u"yes"_s : u"no"_s);
    line(u"Running KWin version"_s, kwinVersion.isEmpty() ? u"unknown"_s : kwinVersion);

    const QStringList copies = installedEffectCopies();
    line(u"Effect files found"_s, copies.isEmpty() ? u"none"_s : copies.join(u", "_s));

    bool effectLoaded = false;
    if (kwinOnBus) {
        const QDBusMessage loaded = call(u"org.kde.KWin"_s, u"/Effects"_s, u"org.kde.kwin.Effects"_s, u"isEffectLoaded"_s, {QString::fromLatin1(Config::EffectId)});
        effectLoaded = loaded.type() == QDBusMessage::ReplyMessage && loaded.arguments().value(0).toBool();
    }
    line(u"Effect loaded in KWin"_s, effectLoaded ? u"yes"_s : u"no"_s);

    QString builtFor;
    if (effectLoaded) {
        const QString path = QString::fromLatin1(Config::EffectDBusPath);
        const QString iface = QString::fromLatin1(Config::EffectDBusInterface);
        const QDBusMessage proto = call(u"org.kde.KWin"_s, path, iface, u"protocolVersion"_s);
        const QDBusMessage built = call(u"org.kde.KWin"_s, path, iface, u"kwinVersion"_s);
        const QDBusMessage mode = call(u"org.kde.KWin"_s, path, iface, u"triggerMode"_s);
        builtFor = built.arguments().value(0).toString();
        line(u"Effect protocol"_s, u"%1 (app expects %2)"_s.arg(proto.arguments().value(0).toString()).arg(Config::EffectProtocolVersion));
        line(u"Effect built for KWin"_s, builtFor);
        line(u"Effect trigger mode"_s, mode.arguments().value(0).toString());
    }

    QString mode;
    if (effectLoaded && QGuiApplication::platformName() == u"wayland"_s) {
        mode = u"Native compositor integration (KWin effect + layer-shell drop target)"_s;
    } else if (QGuiApplication::platformName() == u"wayland"_s) {
        mode = u"Effect missing: only manual activation, `donut --window` (drop-target fallback) and `donut --demo` work"_s;
    } else {
        mode = u"Not a Wayland session: automatic drag detection is unavailable; use `donut --window` or `donut --demo`"_s;
    }
    line(u"Effective mode"_s, mode);
    if (!builtFor.isEmpty() && !kwinVersion.isEmpty() && builtFor != kwinVersion) {
        out << u"WARNING: effect was built for KWin %1 but KWin %2 is running. Rebuild it."_s.arg(builtFor, kwinVersion);
    }

    out << QString() << u"Settings (%1)"_s.arg(settings.configFilePath());
    line(u"  Enabled"_s, settings.enabled() ? u"yes"_s : u"no"_s);
    line(u"  Trigger mode"_s, settings.triggerMode());
    line(u"  Activation threshold"_s, u"%1 px"_s.arg(settings.activationThreshold()));
    line(u"  Dwell"_s, u"%1 ms"_s.arg(settings.dwellMs()));
    line(u"  Shortcut"_s, settings.shortcut());
    line(u"  Enabled actions"_s, settings.enabledActions().join(u", "_s));
    line(u"  Animation factor"_s, QString::number(settings.effectiveAnimationFactor()));
    line(u"  Autostart entry"_s, Autostart::isEnabled() ? Autostart::entryPath() : u"none"_s);
    out << QString() << u"Verbose logs: QT_LOGGING_RULES=\"donut.*.debug=true\" donut"_s
        << u"Effect logs:  journalctl --user -b | grep donut.kwin"_s;
    return out.join(u'\n') + u'\n';
}
}
