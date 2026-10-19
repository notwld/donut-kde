// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "DonutTypes.h"
#include "OperationResult.h"

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QUrl>

class KJob;

namespace Donut
{
/*
 * Runs one file operation at a time, always off the GUI thread:
 *  - Copy / Move: KIO::CopyJob (async, KDE semantics, cross-filesystem moves
 *    copy first and delete the source only after each item succeeded,
 *    existing names are auto-renamed, nothing is overwritten).
 *  - Zip / Extract: ArchiveOperations on the global thread pool.
 */
class FileActionService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    struct Request {
        Action action = Action::Cancel;
        QList<QUrl> sources;
        QUrl destination; ///< target folder
    };

    explicit FileActionService(QObject *parent = nullptr);
    ~FileActionService() override;

    /// Starts the request. Returns false (and emits nothing) if busy.
    /// Validation failures are reported through finished().
    bool start(const Request &request);
    void cancel();

    bool isBusy() const;
    /// 0..1, or -1 when indeterminate.
    double progress() const;

    /// Register KIO jobs with Plasma's job tracker so long copies show up in
    /// the notification area. Off by default (tests, demo mode).
    void setReportJobsToPlasma(bool enabled);

Q_SIGNALS:
    void busyChanged();
    void progressChanged();
    void finished(const Donut::OperationResult &result);

private:
    void startKioJob(const Request &request);
    void startArchiveJob(const Request &request);
    void finish(const OperationResult &result);
    void setProgress(double progress);

    bool m_busy = false;
    double m_progress = -1.0;
    bool m_reportToPlasma = false;
    Action m_action = Action::Cancel;
    QPointer<KJob> m_job;
    QFutureWatcher<OperationResult> *m_watcher = nullptr;
};
}
