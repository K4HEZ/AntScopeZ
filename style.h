#ifndef STYLE_H
#define STYLE_H

#include <QWidget>
#include <QColor>
#include <QPalette>

// A theme is just a small, named set of colors -- not a full design system,
// and not (yet) user-editable. Every Style::*() stylesheet builder below
// pulls its colors from here instead of hardcoding rgb(...)/hex literals, so
// adding a theme is a new Theme instance plus a branch in Style::theme().
//
// Only the "canvas" -- window/dialog background, primary text, muted text,
// and borders -- is themed at all. Buttons, menus, combo/spin boxes, tables,
// sliders, etc. are deliberately left native/Fusion, painted off
// Style::palette() instead of a per-widget stylesheet, so they render with
// whatever accent the platform/Fusion style actually provides rather than a
// color this app bakes in. This used to also carry a set of "control" colors
// (a hand-picked navy/blue skin, constant across themes) that got layered on
// top of native rendering in most dialogs; that fought with the whole point
// of a Light/Dark canvas swap -- see git history on this file if that skin
// ever needs to come back.
//
// Dark reproduces the app's original (and, until now, only) canvas colors as
// closely as possible. Light is a new, hand-picked counterpart. A future
// "Custom" theme would slot in here by loading a Theme from a config file --
// no theme-editor UI is planned.
struct Theme
{
    QColor windowBackground;   // dialogs, message dialogs, tab pane, tables, lists
    QColor text;               // primary text on the canvas (labels, checkboxes, group box titles...)
    QColor textMuted;          // disabled/hint text
    QColor border;             // generic 1px borders (tables, tab pane, group boxes, combo/progress outlines...)
};

class Style
{
public:
    static void setDarkMode(bool dark);
    static bool isDarkMode();
    static const Theme& theme();

    // A QPalette built from the active Theme, covering the roles native
    // (unstyled) widgets actually use -- Base/AlternateBase for list & table
    // views, Window/Button for tab bars and buttons, etc. Used so that
    // controls we deliberately *don't* give a Style::*() stylesheet to (see
    // style.h's file comment) still track the Light/Dark canvas via native
    // Qt/Fusion painting instead of falling back to some ambient system
    // palette. Selection highlighting (QPalette::Highlight) is left unset,
    // so it comes from Fusion itself rather than this Theme.
    static QPalette palette();

    static QString label();
    static QString pushButton(bool checkable=false);
    static QString lineEdit();
    static QString tabWidget();
    static QString checkBox();
    static QString groupBox();
    static QString spinBox();
    static QString tableWidget();
    static QString headerView();
    static QString radioButton();
    static QString toolButton();
    static QString comboBox();
    static QString progressBar();
    static QString dialog();
    static QString mainWindow();
    static QString slider();
    static QString messageBox();
    static QString listWidget();
    static QString menu();
    static QString colorDialog();

private:
    static bool m_dark;
};

#endif // STYLE_H
