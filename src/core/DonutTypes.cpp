// SPDX-License-Identifier: GPL-2.0-or-later
#include "DonutTypes.h"

#include <QMetaEnum>

using namespace Qt::StringLiterals;

namespace Donut
{
QString actionId(Action action)
{
    switch (action) {
    case Action::Copy:
        return u"copy"_s;
    case Action::Move:
        return u"move"_s;
    case Action::Zip:
        return u"zip"_s;
    case Action::Extract:
        return u"extract"_s;
    case Action::Cancel:
        return u"cancel"_s;
    }
    return {};
}

QString actionLabel(Action action)
{
    switch (action) {
    case Action::Copy:
        return QObject::tr("Copy");
    case Action::Move:
        return QObject::tr("Move");
    case Action::Zip:
        return QObject::tr("Zip");
    case Action::Extract:
        return QObject::tr("Extract");
    case Action::Cancel:
        return QObject::tr("Cancel");
    }
    return {};
}

QString actionIconName(Action action)
{
    switch (action) {
    case Action::Copy:
        return u"edit-copy"_s;
    case Action::Move:
        return u"edit-move"_s;
    case Action::Zip:
        return u"archive-insert"_s;
    case Action::Extract:
        return u"archive-extract"_s;
    case Action::Cancel:
        return u"dialog-cancel"_s;
    }
    return {};
}

std::optional<Action> actionFromId(QStringView id)
{
    for (Action a : allActions()) {
        if (actionId(a) == id) {
            return a;
        }
    }
    return std::nullopt;
}

QList<Action> allActions()
{
    return {Action::Copy, Action::Move, Action::Zip, Action::Extract, Action::Cancel};
}

QString stateName(State state)
{
    return QString::fromLatin1(QMetaEnum::fromType<State>().valueToKey(int(state)));
}

QString triggerModeId(TriggerMode mode)
{
    switch (mode) {
    case TriggerMode::Dwell:
        return u"dwell"_s;
    case TriggerMode::Distance:
        return u"distance"_s;
    case TriggerMode::ShortcutOnly:
        return u"shortcut"_s;
    }
    return u"dwell"_s;
}

TriggerMode triggerModeFromId(QStringView id, TriggerMode fallback)
{
    for (TriggerMode m : {TriggerMode::Dwell, TriggerMode::Distance, TriggerMode::ShortcutOnly}) {
        if (triggerModeId(m) == id) {
            return m;
        }
    }
    return fallback;
}
}
