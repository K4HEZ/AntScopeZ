#ifndef STYLE_H
#define STYLE_H

#include <QWidget>
#include <QColor>
#include <QPalette>
#include <QString>

// A theme is a small, named set of colors -- not a full design system.
// Every Style::*() stylesheet builder below pulls its colors from here
// instead of hardcoding rgb(...)/hex literals.
//
// Only the "canvas" -- window/dialog background, primary text, muted text,
// and borders -- plus the plot's own chart background and marker-line
// color are themed at all. Buttons, menus, combo/spin boxes, tables,
// sliders, etc. are deliberately left native/Fusion, painted off
// Style::palette() instead of a per-widget stylesheet, so they render with
// whatever accent the platform/Fusion style actually provides rather than a
// color this app bakes in. This used to also carry a set of "control" colors
// (a hand-picked navy/blue skin, constant across themes) that got layered on
// top of native rendering in most dialogs; that fought with the whole point
// of a Light/Dark canvas swap -- see git history on this file if that skin
// ever needs to come back.
//
// Exactly 5 fixed slots, indices 0-4, each independently ini-backed
// ("Theme0".."Theme4" groups) and user-renamable/user-editable via
// Settings > Themes (not built yet -- this is groundwork for it). Index 0/1
// ship as the Light/Dark defaults; 2-4 seed from the former "HEZ: Themes"
// draft Red/Green/Blue values. See Style::defaultThemeAt() for the
// compiled-in seed data -- used both as the no-ini-yet fallback and (once
// the editor exists) a "restore defaults" action.
struct Theme
{
    QString name;               // "Light"/"Dark"/"Red"/"Green"/"Blue" by default, freely renamable
    QColor windowBackground;    // dialogs, message dialogs, tab pane, tables, lists
    QColor text;                // primary text on the canvas (labels, checkboxes, group box titles...)
    QColor textMuted;           // disabled/hint text
    QColor border;              // generic 1px borders (tables, tab pane, group boxes, combo/progress outlines...)
    QColor chartBackground;     // QCustomPlot's own background -- was a single ini key shared
                                 // across every theme; now themed like everything else
    QColor marker;              // marker/crosshair line + label color on every plot tab
    QColor base;                 // editable field/control fill -- QLineEdit/QSpinBox/QComboBox's
                                 // Base, and (native, unstyled) QCheckBox/QRadioButton indicator
                                 // fill. Used to be computed on the fly (nudge(windowBackground,
                                 // 22), flipping lighter/darker depending on canvas lightness so
                                 // it stayed visible against a near-black canvas) -- promoted to a
                                 // real, directly-editable color once that auto-computed value and
                                 // Settings > Cable's own separately-hardcoded "always darker"
                                 // editable-field fill (Style::readOnlyLock()) turned out to only
                                 // agree with each other by coincidence, not by construction.
};

class Style
{
public:
    static void setActiveThemeIndex(int index);
    static int activeThemeIndex();
    static Theme theme();   // the active theme (themeAt(activeThemeIndex()), cached)

    // Slot access, independent of which one is currently active -- for a
    // future theme editor (load a slot to edit, restore its defaults).
    static Theme themeAt(int index);           // ini value, per-key fallback to the compiled default
    static Theme defaultThemeAt(int index);     // compiled-in seed only, ignoring the ini
    static void saveThemeAt(int index, const Theme& t); // persists; invalidates the cache if it's the active slot

    // A QPalette built from the given Theme (defaults to the active one),
    // covering the roles native (unstyled) widgets actually use --
    // Base/AlternateBase for list & table views, Window/Button for tab bars
    // and buttons, etc. Used so that controls we deliberately *don't* give a
    // Style::*() stylesheet to (see this file's comment above) still track
    // the active canvas via native Qt/Fusion painting instead of falling
    // back to some ambient system palette. Selection highlighting
    // (QPalette::Highlight) is left unset, so it comes from Fusion itself
    // rather than this Theme. Taking an explicit Theme (rather than always
    // reading the active one) lets a future theme editor preview a
    // candidate theme with the exact same code path real widgets use.
    static QPalette palette(const Theme& t = theme());

    static QString label(const Theme& t = theme());
    static QString pushButton(bool checkable=false);
    static QString lineEdit();
    static QString tabWidget();
    static QString checkBox();
    static QString groupBox(const Theme& t = theme());
    static QString spinBox();
    static QString tableWidget();
    static QString headerView();
    static QString radioButton();
    static QString toolButton();
    static QString comboBox();
    static QString progressBar();
    static QString dialog();
    // A control disabled because it's locked to a currently-selected
    // value (e.g. Settings > Cable's Preset mode) needs to read
    // differently from one disabled because it's simply not applicable
    // right now -- the former is still showing real, current information
    // and shouldn't look "broken" the way Qt's automatic disabled-palette
    // dimming does. QLineEdit's own :read-only state gets this
    // automatically (QSpinBox rides along on plain :disabled, since
    // nothing currently gives it a "not applicable" case to distinguish
    // from); QComboBox/QRadioButton have no read-only concept, so callers
    // toggle the "readOnlyLock" dynamic property (setProperty() +
    // style()->unpolish()/polish() to force a repaint) on just the
    // specific instances that mean "locked", leaving plain setEnabled(false)
    // elsewhere to keep Qt's normal dimmed look for "not applicable".
    // QCheckBox gets only the dimmed-but-legible text treatment (no
    // meaningful fill of its own to flatten).
    static QString readOnlyLock(const Theme& t = theme());
    static QString mainWindow();
    static QString slider();
    static QString messageBox();
    static QString listWidget();
    static QString menu();
    static QString colorDialog();

    // Every Style::*() fragment above that is a plain type/property selector
    // with no per-instance runtime data (i.e. everything except the swatch-
    // button/theme-preview one-off colors composed inline at their call
    // sites), concatenated once. This -- not any individual fragment -- is
    // what should actually be handed to qApp->setStyleSheet(), and only from
    // the two places that legitimately own "the current global stylesheet":
    // startup (main.cpp) and MainWindow::changeColorTheme(). No dialog
    // should call setStyleSheet() on itself for theme-derived styling;
    // QApplication::setStyleSheet() *replaces* rather than merges, so a
    // second, uncoordinated call anywhere else silently drops whatever this
    // established. See git history around this comment if that reasoning
    // needs re-deriving.
    static QString globalStyleSheet();

private:
    static int m_activeIndex;
};

#endif // STYLE_H
