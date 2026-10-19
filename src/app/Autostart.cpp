// SPDX-License-Identifier: GPL-2.0-or-later
#include "Autostart.h"
#include "Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Donut::Autostart
{
QString entryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/autostart/org.kde.donut.desktop"_s;
}

bool isEnabled()
{
    return QFile::exists(entryPath());
}

bool setEnabled(bool enabled, const QString &executable)
{
    const QString path = entryPath();
    if (!enabled) {
        return !QFile::exists(path) || QFile::remove(path);
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcCore) << "cannot write autostart entry" << path;
        return false;
    }
    // Quote the path per the Desktop Entry spec in case it contains spaces.
    const QString exec = executable.contains(u' ') ? u"\"%1\""_s.arg(executable) : executable;
    const QString content = u"[Desktop Entry]\n"
                            "Type=Application\n"
                            "Name=Donut\n"
                            "Comment=Radial drag action menu\n"
                            "Exec=%1\n"
                            "Icon=org.kde.donut\n"
                            "X-KDE-autostart-phase=2\n"
                            "NoDisplay=true\n"_s.arg(exec);
    f.write(content.toUtf8());
    return f.error() == QFileDevice::NoError;
}
}
