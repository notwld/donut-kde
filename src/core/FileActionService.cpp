// SPDX-License-Identifier: GPL-2.0-or-later
#include "FileActionService.h"
#include "ActionValidator.h"
#include "ArchiveOperations.h"
#include "FileNaming.h"
#include "Logging.h"

#include <KIO/CopyJob>

#include <QPromise>
#include <QtConcurrentRun>

using namespace Qt::StringLiterals;

namespace Donut
{
namespace
{
constexpr int ProgressRange = 1000;

class PromiseControl : public OperationControl
{
public:
    explicit PromiseControl(QPromise<OperationResult> &promise)
        : m_promise(promise)
    {
        m_promise.setProgressRange(0, ProgressRange);
    }

    bool isCancelled() const override
    {
        return m_promise.isCanceled();
    }

    void reportProgress(qint64 done, qint64 total) override
    {
        const int value = total > 0 ? int(std::min<qint64>(done * ProgressRange / total, ProgressRange)) : 0;
        if (value != m_last) { // throttle: at most ProgressRange updates
            m_last = value;
            m_promise.setProgressValue(value);
        }
    }

private:
    QPromise<OperationResult> &m_promise;
    int m_last = -1;
};

QString itemsText(int count)
{
    return count == 1 ? QObject::tr("1 item") : QObject::tr("%1 items").arg(count);
}
}

FileActionService::FileActionService(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Donut::OperationResult>();
}

FileActionService::~FileActionService()
{
    if (m_job) {
        m_job->kill(KJob::Quietly);
    }
    if (m_watcher) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
    }
}

bool FileActionService::isBusy() const
{
    return m_busy;
}

double FileActionService::progress() const
{
    return m_progress;
}

void FileActionService::setReportJobsToPlasma(bool enabled)
{
    m_reportToPlasma = enabled;
}

bool FileActionService::start(const Request &request)
{
    if (m_busy) {
        qCWarning(lcActions) << "refusing to start" << actionId(request.action) << "- another operation is running";
        return false;
    }
    m_busy = true;
    m_action = request.action;
    Q_EMIT busyChanged();
    setProgress(-1.0);

    qCInfo(lcActions) << "starting" << actionId(request.action) << "on" << request.sources.size() << "item(s)";

    auto failLater = [this](const OperationResult &result) {
        QMetaObject::invokeMethod(this, [this, result] { finish(result); }, Qt::QueuedConnection);
    };

    if (request.action == Action::Cancel) {
        failLater(OperationResult::cancelled(Action::Cancel));
        return true;
    }
    if (const Validation v = validateAction(request.action, request.sources); !v.ok) {
        failLater(OperationResult::failure(request.action, v.reason));
        return true;
    }
    if (const Validation v = validateDestination(request.action, request.sources, request.destination); !v.ok) {
        failLater(OperationResult::failure(request.action, v.reason));
        return true;
    }

    if (request.action == Action::Copy || request.action == Action::Move) {
        startKioJob(request);
    } else {
        startArchiveJob(request);
    }
    return true;
}

void FileActionService::startKioJob(const Request &request)
{
    const KIO::JobFlags flags = m_reportToPlasma ? KIO::DefaultFlags : KIO::HideProgressInfo;
    KIO::CopyJob *job = request.action == Action::Copy ? KIO::copy(request.sources, request.destination, flags)
                                                       : KIO::move(request.sources, request.destination, flags);
    // No interactive delegate: Donut may run without QApplication, and conflicts
    // are resolved by auto-renaming. Nothing is ever overwritten.
    job->setUiDelegate(nullptr);
    job->setAutoRename(true);
    m_job = job;

    auto outputs = std::make_shared<QList<QUrl>>();
    auto renamed = std::make_shared<int>(0);
    connect(job, &KIO::CopyJob::copyingDone, this, [outputs](KIO::Job *, const QUrl &, const QUrl &to) {
        outputs->append(to);
    });
    connect(job, &KIO::CopyJob::copyingLinkDone, this, [outputs](KIO::Job *, const QUrl &, const QString &, const QUrl &to) {
        outputs->append(to);
    });
    connect(job, &KIO::CopyJob::renamed, this, [renamed](KIO::Job *, const QUrl &, const QUrl &) {
        ++*renamed;
    });
    connect(job, &KJob::percentChanged, this, [this](KJob *, unsigned long percent) {
        setProgress(double(percent) / 100.0);
    });
    const Action action = request.action;
    const int count = int(request.sources.size());
    connect(job, &KJob::result, this, [this, action, count, outputs, renamed](KJob *j) {
        if (j->error() == KJob::KilledJobError) {
            finish(OperationResult::cancelled(action));
        } else if (j->error()) {
            finish(OperationResult::failure(action, j->errorString()));
        } else {
            QString message = action == Action::Copy ? tr("Copied %1").arg(itemsText(count)) : tr("Moved %1").arg(itemsText(count));
            if (*renamed > 0) {
                message += u" · "_s + tr("%n renamed to avoid overwriting", nullptr, *renamed);
            }
            finish(OperationResult::success(action, message, *outputs));
        }
    });
}

void FileActionService::startArchiveJob(const Request &request)
{
    QStringList paths;
    for (const QUrl &u : request.sources) {
        paths.append(u.toLocalFile());
    }
    const QString destination = request.destination.toLocalFile();
    const Action action = request.action;
    const QString archiveName = FileNaming::defaultArchiveName(request.sources);

    m_watcher = new QFutureWatcher<OperationResult>(this);
    connect(m_watcher, &QFutureWatcherBase::progressValueChanged, this, [this](int value) {
        setProgress(double(value) / ProgressRange);
    });
    connect(m_watcher, &QFutureWatcherBase::finished, this, [this, action] {
        const QFuture<OperationResult> future = m_watcher->future();
        // Results reported after cancel() are discarded by QFuture, so treat that as cancelled.
        const OperationResult result =
            (future.isCanceled() || future.resultCount() == 0) ? OperationResult::cancelled(action) : future.result();
        m_watcher->deleteLater();
        m_watcher = nullptr;
        finish(result);
    });

    m_watcher->setFuture(QtConcurrent::run([action, paths, destination, archiveName](QPromise<OperationResult> &promise) {
        PromiseControl control(promise);
        if (action == Action::Zip) {
            promise.addResult(ArchiveOperations::createZip(paths, destination, archiveName, &control));
            return;
        }
        // Extract each archive into its own folder; stop at the first failure.
        OperationResult combined = OperationResult::success(action, {});
        for (const QString &archive : paths) {
            OperationResult r = ArchiveOperations::extractZip(archive, destination, &control);
            if (!r.ok()) {
                promise.addResult(r);
                return;
            }
            combined.outputs += r.outputs;
            combined.message = r.message;
        }
        if (paths.size() > 1) {
            combined.message = QObject::tr("Extracted %1 archives").arg(paths.size());
        }
        promise.addResult(combined);
    }));
}

void FileActionService::cancel()
{
    if (m_job) {
        qCInfo(lcActions) << "cancelling KIO job";
        m_job->kill(KJob::EmitResult);
    } else if (m_watcher) {
        qCInfo(lcActions) << "cancelling archive job";
        m_watcher->cancel();
    }
}

void FileActionService::finish(const OperationResult &result)
{
    m_job = nullptr;
    m_busy = false;
    setProgress(result.ok() ? 1.0 : -1.0);
    qCInfo(lcActions) << "finished" << actionId(result.action) << "status" << int(result.status);
    Q_EMIT busyChanged();
    Q_EMIT finished(result);
}

void FileActionService::setProgress(double progress)
{
    if (!qFuzzyCompare(progress + 2.0, m_progress + 2.0)) {
        m_progress = progress;
        Q_EMIT progressChanged();
    }
}
}
