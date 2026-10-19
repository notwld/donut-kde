    // SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ActivationTracker.h"
#include "DonutTypes.h"

#include <KSharedConfig>

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace Donut
{
/*
 * All user settings, persisted in ~/.config/donutrc via KConfig.
 *
 * Properties are edited in memory by the settings page and written with
 * save(). The KWin effect reads the [General] and [Trigger] groups of the
 * same file when it is reconfigured.
 */
class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(int activationThreshold READ activationThreshold WRITE setActivationThreshold NOTIFY changed)
    Q_PROPERTY(QString triggerMode READ triggerMode WRITE setTriggerMode NOTIFY changed)
    Q_PROPERTY(int dwellMs READ dwellMs WRITE setDwellMs NOTIFY changed)
    Q_PROPERTY(int outerRadius READ outerRadius WRITE setOuterRadius NOTIFY changed)
    Q_PROPERTY(int innerRadius READ innerRadius WRITE setInnerRadius NOTIFY changed)
    Q_PROPERTY(double sectorSpacing READ sectorSpacing WRITE setSectorSpacing NOTIFY changed)
    Q_PROPERTY(int openDurationMs READ openDurationMs WRITE setOpenDurationMs NOTIFY changed)
    Q_PROPERTY(bool showLabels READ showLabels WRITE setShowLabels NOTIFY changed)
    Q_PROPERTY(double glowIntensity READ glowIntensity WRITE setGlowIntensity NOTIFY changed)
    Q_PROPERTY(QString placement READ placement WRITE setPlacement NOTIFY changed)
    Q_PROPERTY(int placementOffset READ placementOffset WRITE setPlacementOffset NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY changed)
    Q_PROPERTY(bool blurBehind READ blurBehind WRITE setBlurBehind NOTIFY changed)
    Q_PROPERTY(QStringList enabledActions READ enabledActions WRITE setEnabledActions NOTIFY changed)
    Q_PROPERTY(QString shortcut READ shortcut WRITE setShortcut NOTIFY changed)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY changed)
    Q_PROPERTY(bool diagnosticLogging READ diagnosticLogging WRITE setDiagnosticLogging NOTIFY changed)
    Q_PROPERTY(QString destinationMode READ destinationMode WRITE setDestinationMode NOTIFY changed)
    Q_PROPERTY(QUrl fixedDestination READ fixedDestination WRITE setFixedDestination NOTIFY changed)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(double effectiveAnimationFactor READ effectiveAnimationFactor NOTIFY changed)

public:
    /// Uses ~/.config/donutrc unless a config is injected (tests).
    explicit SettingsManager(KSharedConfig::Ptr config = {}, QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool v);
    int activationThreshold() const;
    void setActivationThreshold(int v);
    QString triggerMode() const;
    void setTriggerMode(const QString &v);
    TriggerMode triggerModeEnum() const;
    int dwellMs() const;
    void setDwellMs(int v);
    int outerRadius() const;
    void setOuterRadius(int v);
    int innerRadius() const;
    void setInnerRadius(int v);
    double sectorSpacing() const;
    void setSectorSpacing(double v);
    int openDurationMs() const;
    void setOpenDurationMs(int v);
    bool showLabels() const;
    void setShowLabels(bool v);
    double glowIntensity() const;
    void setGlowIntensity(double v);
    QString placement() const;
    void setPlacement(const QString &v);
    int placementOffset() const;
    void setPlacementOffset(int v);
    bool reducedMotion() const;
    void setReducedMotion(bool v);
    bool blurBehind() const;
    void setBlurBehind(bool v);
    QStringList enabledActions() const;
    void setEnabledActions(const QStringList &v);
    Q_INVOKABLE bool isActionEnabled(const QString &id) const;
    Q_INVOKABLE void setActionEnabled(const QString &id, bool enabled);
    QString shortcut() const;
    void setShortcut(const QString &v);
    bool autostart() const;
    void setAutostart(bool v);
    bool diagnosticLogging() const;
    void setDiagnosticLogging(bool v);
    QString destinationMode() const;
    void setDestinationMode(const QString &v);
    QUrl fixedDestination() const;
    void setFixedDestination(const QUrl &v);

    /// Actions shown on the ring, in clockwise order. Cancel is always present.
    QList<Action> ringActions() const;
    ActivationTracker::Config trackerConfig() const;

    /// Plasma's global animation speed (kdeglobals AnimationDurationFactor),
    /// forced to 0 when reducedMotion is enabled.
    double effectiveAnimationFactor() const;

    bool isDirty() const;
    Q_INVOKABLE void load();
    Q_INVOKABLE bool save();
    Q_INVOKABLE void resetToDefaults();

    QString configFilePath() const;

Q_SIGNALS:
    void changed();
    void dirtyChanged();
    /// Emitted after save() wrote the file.
    void saved();

private:
    template<typename T>
    void assign(T &member, const T &value);
    void setDirty(bool dirty);

    KSharedConfig::Ptr m_config;
    bool m_dirty = false;

    bool m_enabled;
    int m_threshold;
    QString m_triggerMode;
    int m_dwellMs;
    int m_outerRadius;
    int m_innerRadius;
    double m_sectorSpacing;
    int m_openDurationMs;
    bool m_showLabels;
    double m_glowIntensity;
    QString m_placement;
    int m_placementOffset;
    bool m_reducedMotion;
    bool m_blurBehind;
    QStringList m_enabledActions;
    QString m_shortcut;
    bool m_autostart;
    bool m_diagnosticLogging;
    QString m_destinationMode;
    QUrl m_fixedDestination;
};
}
