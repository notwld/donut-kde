// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DonutTypes.h"

#include <QList>
#include <QMetaType>
#include <QString>
#include <QUrl>

namespace Donut
{
struct OperationResult {
    enum class Status {
        Success,
        Cancelled,
        Failed,
    };

    Action action = Action::Cancel;
    Status status = Status::Failed;
    QString message; ///< user-visible, never contains file contents
    QList<QUrl> outputs; ///< created archive, extraction folder, copied items...
    int skipped = 0; ///< items intentionally not processed (e.g. sockets, devices)

    bool ok() const
    {
        return status == Status::Success;
    }

    static OperationResult success(Action action, const QString &message, const QList<QUrl> &outputs = {})
    {
        return {action, Status::Success, message, outputs, 0};
    }
    static OperationResult failure(Action action, const QString &message)
    {
        return {action, Status::Failed, message, {}, 0};
    }
    static OperationResult cancelled(Action action, const QString &message = {})
    {
        return {action, Status::Cancelled, message.isEmpty() ? QObject::tr("Cancelled") : message, {}, 0};
    }
};

/// Lets long-running workers report progress and observe cancellation without
/// depending on a particular threading primitive (tests use a trivial implementation).
class OperationControl
{
public:
    virtual ~OperationControl() = default;
    virtual bool isCancelled() const = 0;
    virtual void reportProgress(qint64 done, qint64 total) = 0;
};
}

Q_DECLARE_METATYPE(Donut::OperationResult)
