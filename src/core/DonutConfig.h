// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * Configuration keys shared by the Donut application and the KWin effect.
 * Both read ~/.config/donutrc; only the application writes it.
 * Header-only and dependency-free on purpose so the effect can include it.
 */
namespace Donut::Config
{
inline constexpr const char *FileName = "donutrc";

inline constexpr const char *GroupGeneral = "General";
inline constexpr const char *KeyEnabled = "Enabled";
inline constexpr const char *KeyAutostart = "Autostart";
inline constexpr const char *KeyDiagnosticLogging = "DiagnosticLogging";

inline constexpr const char *GroupTrigger = "Trigger";
inline constexpr const char *KeyTriggerMode = "Mode"; // "dwell" | "distance" | "shortcut"
inline constexpr const char *KeyThreshold = "Threshold";
inline constexpr const char *KeyDwellMs = "DwellMs";
inline constexpr const char *KeyShortcut = "Shortcut";

inline constexpr const char *GroupAppearance = "Appearance";
inline constexpr const char *KeyOuterRadius = "OuterRadius";
inline constexpr const char *KeyInnerRadius = "InnerRadius";
inline constexpr const char *KeySectorSpacing = "SectorSpacing";
inline constexpr const char *KeyOpenDurationMs = "OpenDurationMs";
inline constexpr const char *KeyShowLabels = "ShowLabels";
inline constexpr const char *KeyGlowIntensity = "GlowIntensity";
inline constexpr const char *KeyPlacement = "Placement"; // "centered" | "offset"
inline constexpr const char *KeyPlacementOffset = "PlacementOffset";
inline constexpr const char *KeyReducedMotion = "ReducedMotion";
inline constexpr const char *KeyBlurBehind = "BlurBehind";

inline constexpr const char *GroupActions = "Actions";
inline constexpr const char *KeyEnabledActions = "EnabledActions";
inline constexpr const char *KeyDestinationMode = "DestinationMode"; // "ask" | "fixed"
inline constexpr const char *KeyFixedDestination = "FixedDestination";

inline constexpr bool DefaultEnabled = true;
inline constexpr const char *DefaultTriggerMode = "dwell";
inline constexpr double DefaultThreshold = 16.0;
inline constexpr int DefaultDwellMs = 450;
inline constexpr const char *DefaultShortcut = "Meta+Shift+D";

inline constexpr double MinThreshold = 4.0;
inline constexpr double MaxThreshold = 96.0;
inline constexpr int MinDwellMs = 150;
inline constexpr int MaxDwellMs = 2000;

/// Name the effect is installed under (library and plugin id).
inline constexpr const char *EffectId = "donutdragwatch";
inline constexpr const char *EffectDBusPath = "/org/kde/Donut/DragWatch";
inline constexpr const char *EffectDBusInterface = "org.kde.Donut.DragWatch";
/// Bumped whenever the D-Bus contract between the effect and the app changes.
inline constexpr int EffectProtocolVersion = 1;
}
