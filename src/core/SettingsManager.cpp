// SPDX-License-Identifier: GPL-2.0-or-later
#include "SettingsManager.h"
#include "DonutConfig.h"
#include "Logging.h"

#include <KConfigGroup>

#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Donut
{
namespace
{
const QStringList &defaultActions()
{
    static const QStringList actions = {u"copy"_s, u"move"_s, u"zip"_s, u"extract"_s};
    return actions;
}
}

SettingsManager::SettingsManager(KSharedConfig::Ptr config, QObject *parent)
    : QObject(parent)
    , m_config(config ? config : KSharedConfig::openConfig(QString::fromLatin1(Config::FileName), KConfig::SimpleConfig))
{
    resetToDefaults();
    load();
}

template<typename T>
void SettingsManager::assign(T &member, const T &value)
{
    if (member == value) {
        return;
    }
    member = value;
    setDirty(true);
    Q_EMIT changed();
}

void SettingsManager::setDirty(bool dirty)
{
    if (m_dirty != dirty) {
        m_dirty = dirty;
        Q_EMIT dirtyChanged();
    }
}

// clang-format off
bool SettingsManager::enabled() const { return m_enabled; }
void SettingsManager::setEnabled(bool v) { assign(m_enabled, v); }
int SettingsManager::activationThreshold() const { return m_threshold; }
void SettingsManager::setActivationThreshold(int v) { assign(m_threshold, std::clamp(v, int(Config::MinThreshold), int(Config::MaxThreshold))); }
QString SettingsManager::triggerMode() const { return m_triggerMode; }
void SettingsManager::setTriggerMode(const QString &v) { assign(m_triggerMode, triggerModeId(triggerModeFromId(v))); }
TriggerMode SettingsManager::triggerModeEnum() const { return triggerModeFromId(m_triggerMode); }
int SettingsManager::dwellMs() const { return m_dwellMs; }
void SettingsManager::setDwellMs(int v) { assign(m_dwellMs, std::clamp(v, Config::MinDwellMs, Config::MaxDwellMs)); }
int SettingsManager::outerRadius() const { return m_outerRadius; }
void SettingsManager::setOuterRadius(int v) { assign(m_outerRadius, std::clamp(v, 120, 240)); }
int SettingsManager::innerRadius() const { return m_innerRadius; }
void SettingsManager::setInnerRadius(int v) { assign(m_innerRadius, std::clamp(v, 36, 70)); }
double SettingsManager::sectorSpacing() const { return m_sectorSpacing; }
void SettingsManager::setSectorSpacing(double v) { assign(m_sectorSpacing, std::clamp(v, 0.0, 10.0)); }
int SettingsManager::openDurationMs() const { return m_openDurationMs; }
void SettingsManager::setOpenDurationMs(int v) { assign(m_openDurationMs, std::clamp(v, 0, 600)); }
bool SettingsManager::showLabels() const { return m_showLabels; }
void SettingsManager::setShowLabels(bool v) { assign(m_showLabels, v); }
double SettingsManager::glowIntensity() const { return m_glowIntensity; }
void SettingsManager::setGlowIntensity(double v) { assign(m_glowIntensity, std::clamp(v, 0.0, 1.0)); }
QString SettingsManager::placement() const { return m_placement; }
void SettingsManager::setPlacement(const QString &v) { assign(m_placement, v == u"offset"_s ? v : u"centered"_s); }
int SettingsManager::placementOffset() const { return m_placementOffset; }
void SettingsManager::setPlacementOffset(int v) { assign(m_placementOffset, std::clamp(v, 0, 200)); }
bool SettingsManager::reducedMotion() const { return m_reducedMotion; }
void SettingsManager::setReducedMotion(bool v) { assign(m_reducedMotion, v); }
bool SettingsManager::blurBehind() const { return m_blurBehind; }
void SettingsManager::setBlurBehind(bool v) { assign(m_blurBehind, v); }
QStringList SettingsManager::enabledActions() const { return m_enabledActions; }
QString SettingsManager::shortcut() const { return m_shortcut; }
void SettingsManager::setShortcut(const QString &v) { assign(m_shortcut, v.trimmed()); }
bool SettingsManager::autostart() const { return m_autostart; }
void SettingsManager::setAutostart(bool v) { assign(m_autostart, v); }
bool SettingsManager::diagnosticLogging() const { return m_diagnosticLogging; }
void SettingsManager::setDiagnosticLogging(bool v) { assign(m_diagnosticLogging, v); }
QString SettingsManager::destinationMode() const { return m_destinationMode; }
void SettingsManager::setDestinationMode(const QString &v) { assign(m_destinationMode, v == u"fixed"_s ? v : u"ask"_s); }
QUrl SettingsManager::fixedDestination() const { return m_fixedDestination; }
void SettingsManager::setFixedDestination(const QUrl &v) { assign(m_fixedDestination, v); }
bool SettingsManager::isDirty() const { return m_dirty; }
// clang-format on

void SettingsManager::setEnabledActions(const QStringList &v)
{
    // Keep canonical order and drop unknown ids; Cancel is implicit.
    QStringList clean;
    for (const QString &id : defaultActions()) {
        if (v.contains(id)) {
            clean.append(id);
        }
    }
    assign(m_enabledActions, clean);
}

bool SettingsManager::isActionEnabled(const QString &id) const
{
    return id == u"cancel"_s || m_enabledActions.contains(id);
}

void SettingsManager::setActionEnabled(const QString &id, bool enabled)
{
    QStringList list = m_enabledActions;
    list.removeAll(id);
    if (enabled) {
        list.append(id);
    }
    setEnabledActions(list);
}

QList<Action> SettingsManager::ringActions() const
{
    QList<Action> actions;
    for (const QString &id : m_enabledActions) {
        if (auto a = actionFromId(id)) {
            actions.append(*a);
        }
    }
    actions.append(Action::Cancel);
    return actions;
}

ActivationTracker::Config SettingsManager::trackerConfig() const
{
    ActivationTracker::Config c;
    switch (triggerModeEnum()) {
    case TriggerMode::Dwell:
        c.mode = ActivationTracker::Mode::Dwell;
        break;
    case TriggerMode::Distance:
        c.mode = ActivationTracker::Mode::Distance;
        break;
    case TriggerMode::ShortcutOnly:
        c.mode = ActivationTracker::Mode::Manual;
        break;
    }
    c.threshold = m_threshold;
    c.dwellMs = m_dwellMs;
    return c;
}

double SettingsManager::effectiveAnimationFactor() const
{
    if (m_reducedMotion) {
        return 0.0;
    }
    const KConfigGroup kde = KSharedConfig::openConfig(u"kdeglobals"_s)->group(u"KDE"_s);
    return std::clamp(kde.readEntry("AnimationDurationFactor", 1.0), 0.0, 4.0);
}

void SettingsManager::resetToDefaults()
{
    m_enabled = Config::DefaultEnabled;
    m_threshold = int(Config::DefaultThreshold);
    m_triggerMode = QString::fromLatin1(Config::DefaultTriggerMode);
    m_dwellMs = Config::DefaultDwellMs;
    m_outerRadius = 170;
    m_innerRadius = 50;
    m_sectorSpacing = 2.0;
    m_openDurationMs = 200;
    m_showLabels = true;
    m_glowIntensity = 0.7;
    m_placement = u"centered"_s;
    m_placementOffset = 0;
    m_reducedMotion = false;
    m_blurBehind = true;
    m_enabledActions = defaultActions();
    m_shortcut = QString::fromLatin1(Config::DefaultShortcut);
    m_autostart = false;
    m_diagnosticLogging = false;
    m_destinationMode = u"ask"_s;
    m_fixedDestination = {};
    setDirty(true);
    Q_EMIT changed();
}

void SettingsManager::load()
{
    m_config->reparseConfiguration();
    const KConfigGroup general = m_config->group(QString::fromLatin1(Config::GroupGeneral));
    const KConfigGroup trigger = m_config->group(QString::fromLatin1(Config::GroupTrigger));
    const KConfigGroup look = m_config->group(QString::fromLatin1(Config::GroupAppearance));
    const KConfigGroup actions = m_config->group(QString::fromLatin1(Config::GroupActions));

    // Route through the setters so stored values are clamped/sanitised too.
    setEnabled(general.readEntry(Config::KeyEnabled, m_enabled));
    setAutostart(general.readEntry(Config::KeyAutostart, m_autostart));
    setDiagnosticLogging(general.readEntry(Config::KeyDiagnosticLogging, m_diagnosticLogging));
    setTriggerMode(trigger.readEntry(Config::KeyTriggerMode, m_triggerMode));
    setActivationThreshold(trigger.readEntry(Config::KeyThreshold, m_threshold));
    setDwellMs(trigger.readEntry(Config::KeyDwellMs, m_dwellMs));
    setShortcut(trigger.readEntry(Config::KeyShortcut, m_shortcut));
    setOuterRadius(look.readEntry(Config::KeyOuterRadius, m_outerRadius));
    setInnerRadius(look.readEntry(Config::KeyInnerRadius, m_innerRadius));
    setSectorSpacing(look.readEntry(Config::KeySectorSpacing, m_sectorSpacing));
    setOpenDurationMs(look.readEntry(Config::KeyOpenDurationMs, m_openDurationMs));
    setShowLabels(look.readEntry(Config::KeyShowLabels, m_showLabels));
    setGlowIntensity(look.readEntry(Config::KeyGlowIntensity, m_glowIntensity));
    setPlacement(look.readEntry(Config::KeyPlacement, m_placement));
    setPlacementOffset(look.readEntry(Config::KeyPlacementOffset, m_placementOffset));
    setReducedMotion(look.readEntry(Config::KeyReducedMotion, m_reducedMotion));
    setBlurBehind(look.readEntry(Config::KeyBlurBehind, m_blurBehind));
    setEnabledActions(actions.readEntry(Config::KeyEnabledActions, m_enabledActions));
    setDestinationMode(actions.readEntry(Config::KeyDestinationMode, m_destinationMode));
    setFixedDestination(QUrl(actions.readEntry(Config::KeyFixedDestination, m_fixedDestination.toString())));
    setDirty(false);
}

bool SettingsManager::save()
{
    KConfigGroup general = m_config->group(QString::fromLatin1(Config::GroupGeneral));
    KConfigGroup trigger = m_config->group(QString::fromLatin1(Config::GroupTrigger));
    KConfigGroup look = m_config->group(QString::fromLatin1(Config::GroupAppearance));
    KConfigGroup actions = m_config->group(QString::fromLatin1(Config::GroupActions));

    general.writeEntry(Config::KeyEnabled, m_enabled);
    general.writeEntry(Config::KeyAutostart, m_autostart);
    general.writeEntry(Config::KeyDiagnosticLogging, m_diagnosticLogging);
    trigger.writeEntry(Config::KeyTriggerMode, m_triggerMode);
    trigger.writeEntry(Config::KeyThreshold, m_threshold);
    trigger.writeEntry(Config::KeyDwellMs, m_dwellMs);
    trigger.writeEntry(Config::KeyShortcut, m_shortcut);
    look.writeEntry(Config::KeyOuterRadius, m_outerRadius);
    look.writeEntry(Config::KeyInnerRadius, m_innerRadius);
    look.writeEntry(Config::KeySectorSpacing, m_sectorSpacing);
    look.writeEntry(Config::KeyOpenDurationMs, m_openDurationMs);
    look.writeEntry(Config::KeyShowLabels, m_showLabels);
    look.writeEntry(Config::KeyGlowIntensity, m_glowIntensity);
    look.writeEntry(Config::KeyPlacement, m_placement);
    look.writeEntry(Config::KeyPlacementOffset, m_placementOffset);
    look.writeEntry(Config::KeyReducedMotion, m_reducedMotion);
    look.writeEntry(Config::KeyBlurBehind, m_blurBehind);
    actions.writeEntry(Config::KeyEnabledActions, m_enabledActions);
    actions.writeEntry(Config::KeyDestinationMode, m_destinationMode);
    actions.writeEntry(Config::KeyFixedDestination, m_fixedDestination.toString());

    if (!m_config->sync()) {
        qCWarning(lcCore) << "failed to write" << configFilePath();
        return false;
    }
    setDirty(false);
    qCInfo(lcCore) << "settings saved";
    Q_EMIT saved();
    return true;
}

QString SettingsManager::configFilePath() const
{
    const QString name = m_config->name();
    if (name.startsWith(u'/')) {
        return name;
    }
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' + name;
}
}
