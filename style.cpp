#include "style.h"
#include "settings.h"
#include <QSettings>

int Style::m_activeIndex = 0;

namespace {

// Compiled-in seed data for the 5 fixed slots. Reused for two things: the
// no-ini-yet fallback (per key, in themeAt() below) and Style::
// defaultThemeAt()'s "restore defaults" result -- one source of truth
// instead of two copies that could drift apart.
//
// Dark/Red/Green/Blue have real, individually-chosen chartBackground/marker
// values now (tuned 2026-08-16); Light is still on the original placeholder
// (a plain white chartBackground and semi-transparent QColor(255,0,0,150)
// marker, matching what used to be a single global value shared by every
// theme) pending the same treatment.

Theme lightThemeDefault()
{
    Theme t;
    t.name              = "Light";
    t.windowBackground  = QColor(246, 246, 246);
    t.text              = QColor(30, 30, 30);
    t.textMuted         = QColor(110, 110, 110);
    t.border            = QColor(195, 195, 200);
    t.chartBackground   = QColor(255, 255, 255);
    t.marker            = QColor(255, 0, 0, 150);
    return t;
}

Theme darkThemeDefault()
{
    Theme t;
    t.name              = "Dark";
    t.windowBackground  = QColor(0x2f, 0x2f, 0x2f);
    t.text              = QColor(0xff, 0xff, 0xff);
    t.textMuted         = QColor(0x80, 0x80, 0x80);
    t.border            = QColor(0x80, 0x80, 0x80);
    t.chartBackground   = QColor(0x56, 0x56, 0x56);
    t.marker            = QColor(0xff, 0x00, 0x00);
    return t;
}

Theme redThemeDefault()
{
    Theme t;
    t.name              = "Red";
    t.windowBackground  = QColor(0x40, 0x12, 0x12);
    t.text              = QColor(0xff, 0xf0, 0xf0);
    t.textMuted         = QColor(0x80, 0x60, 0x60);
    t.border            = QColor(0x8e, 0x5a, 0x5f);
    t.chartBackground   = QColor(0xaa, 0x8d, 0x8d);
    t.marker            = QColor(0xff, 0x00, 0x00);
    return t;
}

Theme greenThemeDefault()
{
    Theme t;
    t.name              = "Green";
    t.windowBackground  = QColor(0x00, 0x26, 0x00);
    t.text              = QColor(0xa2, 0xff, 0xa2);
    t.textMuted         = QColor(0x1b, 0x64, 0x26);
    t.border            = QColor(0x00, 0x55, 0x00);
    t.chartBackground   = QColor(0xac, 0xf9, 0xa7);
    t.marker            = QColor(0xff, 0x00, 0x00);
    return t;
}

Theme blueThemeDefault()
{
    Theme t;
    t.name              = "Blue";
    t.windowBackground  = QColor(0x12, 0x12, 0x40);
    t.text              = QColor(0xf0, 0xf0, 0xff);
    t.textMuted         = QColor(0x60, 0x60, 0x80);
    t.border            = QColor(0x5a, 0x5a, 0x80);
    t.chartBackground   = QColor(0x00, 0x76, 0xa1);
    t.marker            = QColor(0xff, 0x00, 0x00);
    return t;
}

QString c(const QColor& color)
{
    return color.name();
}

// theme() is called from hot paths (crosshair/marker repaint on every mouse
// move), so the active theme is cached rather than re-read from the ini on
// every call -- only setActiveThemeIndex()/saveThemeAt() (both rare: a menu
// click, or a future theme editor's Save button) invalidate it.
Theme g_activeThemeCache;
bool g_activeThemeCacheValid = false;

} // namespace

void Style::setActiveThemeIndex(int index)
{
    m_activeIndex = index;
    g_activeThemeCacheValid = false;
}

int Style::activeThemeIndex()
{
    return m_activeIndex;
}

Theme Style::defaultThemeAt(int index)
{
    switch (index) {
    case 0: return lightThemeDefault();
    case 1: return darkThemeDefault();
    case 2: return redThemeDefault();
    case 3: return greenThemeDefault();
    case 4: return blueThemeDefault();
    default: return lightThemeDefault();
    }
}

Theme Style::themeAt(int index)
{
    const Theme def = defaultThemeAt(index);

    QSettings set(Settings::setIniFile(), QSettings::IniFormat);
    set.beginGroup(QString("Theme%1").arg(index));

    Theme t;
    t.name             = set.value("name", def.name).toString();
    t.windowBackground = QColor(set.value("windowBackground", def.windowBackground.name()).toString());
    t.text             = QColor(set.value("text", def.text.name()).toString());
    t.textMuted        = QColor(set.value("textMuted", def.textMuted.name()).toString());
    t.border           = QColor(set.value("border", def.border.name()).toString());
    t.chartBackground  = QColor(set.value("chartBackground", def.chartBackground.name()).toString());
    t.marker           = QColor(set.value("marker", def.marker.name()).toString());

    set.endGroup();
    return t;
}

void Style::saveThemeAt(int index, const Theme& t)
{
    QSettings set(Settings::setIniFile(), QSettings::IniFormat);
    set.beginGroup(QString("Theme%1").arg(index));

    set.setValue("name", t.name);
    set.setValue("windowBackground", t.windowBackground.name());
    set.setValue("text", t.text.name());
    set.setValue("textMuted", t.textMuted.name());
    set.setValue("border", t.border.name());
    set.setValue("chartBackground", t.chartBackground.name());
    set.setValue("marker", t.marker.name());

    set.endGroup();

    if (index == m_activeIndex)
        g_activeThemeCacheValid = false;
}

Theme Style::theme()
{
    if (!g_activeThemeCacheValid) {
        g_activeThemeCache = themeAt(m_activeIndex);
        g_activeThemeCacheValid = true;
    }
    return g_activeThemeCache;
}

QPalette Style::palette(const Theme& t)
{
    QPalette p;

    const bool canvasIsLight = t.windowBackground.lightness() > 128;

    // QColor::lighter()/darker() scale multiplicatively, which is nearly a
    // no-op on a canvas color this close to black -- e.g. lighter(115) on
    // rgb(18,18,18) (the Dark theme's windowBackground) only reaches
    // (21,21,21). Nudge by a fixed absolute amount instead, so Base/
    // AlternateBase stay visibly distinct from Window regardless of how
    // dark/light the theme's canvas is.
    auto nudge = [canvasIsLight](const QColor& c, int amount) {
        int delta = canvasIsLight ? -amount : amount;
        return QColor(qBound(0, c.red() + delta, 255),
                       qBound(0, c.green() + delta, 255),
                       qBound(0, c.blue() + delta, 255));
    };

    p.setColor(QPalette::Window, t.windowBackground);
    p.setColor(QPalette::WindowText, t.text);
    // Previously identical to Window (zero contrast) -- most visible in a
    // DontUseNativeDialog QFileDialog's "File name" field, which was
    // indistinguishable from the dialog background on the Dark canvas.
    p.setColor(QPalette::Base, nudge(t.windowBackground, 22));
    // A subtle alternating-row tint, distinct from Base too.
    p.setColor(QPalette::AlternateBase, nudge(t.windowBackground, 34));
    p.setColor(QPalette::Text, t.text);
    // Neutral gray, not a branded color -- this is what native (unstyled)
    // buttons/tabs/headers actually render with, so they read as "plain
    // chrome" rather than carrying an app-specific skin.
    p.setColor(QPalette::Button, canvasIsLight
               ? t.windowBackground.darker(112) : t.windowBackground.lighter(160));
    p.setColor(QPalette::ButtonText, t.text);
    // Highlight/HighlightedText deliberately left unset -- Fusion's own
    // accent color shows through for selection highlighting instead of a
    // color this app bakes in.
    p.setColor(QPalette::ToolTipBase, t.windowBackground);
    p.setColor(QPalette::ToolTipText, t.text);
    p.setColor(QPalette::PlaceholderText, t.textMuted);

    p.setColor(QPalette::Disabled, QPalette::Text, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.textMuted);

    return p;
}

QString Style::label(const Theme& t)
{
    QString style;

    style = "QLabel {color: " + c(t.text) + "; }";
    style += "QLabel:disabled {color: " + c(t.textMuted) + "; }";
    return style;
}

QString Style::pushButton(bool checkable)
{
    Q_UNUSED(checkable)
    // Left native: this used to paint every button with a hand-picked navy
    // skin (constant across both themes), layered on top of dialogs that
    // otherwise already track the Light/Dark canvas. Fusion already draws a
    // perfectly good button off Style::palette()'s Button/ButtonText, so
    // dialogs (Settings, Export, Select Device, ...) now match the
    // main-window toolbar buttons, which were already native.
    return QString();
}

QString Style::lineEdit()
{
    // Left native: Fusion fills a QLineEdit from Style::palette()'s
    // Base/Text already, which reads fine on either canvas. The custom
    // rgb(26, 45, 198) fill this used to apply is the original app's
    // pre-theme color, carried over unchanged since well before Light mode
    // existed -- a saturated, attention-grabbing blue that was never
    // reconsidered against a themeable canvas (it's especially jarring on
    // Start/Stop/Points and every other plain input field). The readOnly/
    // disabled rules below it were dead code when this was written: at the
    // time, nothing in the app called QLineEdit::setReadOnly(true), and
    // Fusion already dims disabled fields off the palette's Disabled
    // group. Settings > Cable's Preset mode is the first caller of
    // setReadOnly() (see Settings::updateCableEditability()) -- that gets
    // its own deliberate styling in Style::readOnlyLock() instead of
    // reviving anything here, since plain native read-only (no visual
    // change at all) reads as "still editable" and disabled's dimming
    // reads as "unavailable/broken", neither of which is what "locked to
    // the selected preset's value" should look like.
    return QString();
}

QString Style::tabWidget()
{
    // Left native: Fusion draws a perfectly serviceable tab bar off
    // Style::palette()'s Window/Button, no per-state color rules needed here.
    return QString();
}

QString Style::checkBox()
{
    // Left native: the platform/Fusion indicator off Style::palette() is
    // fine, no need for custom checkbox glyph images anymore (removed --
    // see git history for the former checked.png/unchecked.png).
    return QString();
}

QString Style::groupBox(const Theme& t)
{
    QString style;

    style = "QGroupBox:title {color: " + c(t.text) + "; padding: 3px 0; subcontrol-origin: 1ex;} ";
    style += "QGroupBox {border: 2px solid " + c(t.border) + "; border-radius: 5px;} ";
    return style;
}

QString Style::spinBox()
{
    // Left native -- see Style::lineEdit(); same rgb(26, 45, 198) fill, same
    // fix.
    return QString();
}

QString Style::tableWidget()
{
    // Left fully native -- see Style::checkBox() for why the checkbox-column
    // glyphs went native too.
    return QString();
}

QString Style::listWidget()
{
    // Left native -- see Style::tableWidget().
    return QString();
}

QString Style::headerView()
{
    // Left native -- see Style::tableWidget().
    return QString();
}

QString Style::radioButton()
{
    // Left native -- see Style::checkBox().
    return QString();
}

QString Style::toolButton()
{
    // Left native. (The chart-background swatch button sets its own literal
    // background color directly, independent of this -- see
    // Settings::showColorDialog() -- so it's unaffected either way.)
    return QString();
}

QString Style::comboBox()
{
    // Otherwise left native -- Fusion's combo box already reads Button/Base
    // correctly from Style::palette(). Padding on both the closed box and
    // its dropdown list items is added explicitly though: Fusion's default
    // is tight enough (at some DPI/font combinations) that adjacent
    // dropdown item text visually crowds/touches its neighbors.
    return "QComboBox{padding: 2px 6px;} "
           "QComboBox QAbstractItemView::item{padding: 4px 6px;} ";
}

QString Style::progressBar()
{
    // Left native -- Fusion fills the chunk from QPalette::Highlight.
    return QString();
}

QString Style::slider()
{
    // Left native.
    return QString();
}

QString Style::dialog()
{
    const Theme t = theme();
    return "QDialog{background-color: " + c(t.windowBackground) + ";} ";
}

QString Style::readOnlyLock(const Theme& t)
{
    // Text stays fully legible (t.text, not t.textMuted) -- this is real,
    // current information, not a hint/placeholder. Flattening the fill to
    // the dialog's own background (rather than the input's normal Base
    // fill) is what actually reads as "locked" instead of "just like every
    // other editable field". See the declaration in style.h for why two
    // separate selectors are needed (QLineEdit really is read-only;
    // QComboBox/QRadioButton fake it via the "readOnlyLock" property since
    // neither has a native read-only state).
    QString bg = c(t.windowBackground);
    QString fg = c(t.text);
    return "QLineEdit:read-only {background-color: " + bg + "; color: " + fg + ";} "
           "*[readOnlyLock=\"true\"]:disabled {background-color: " + bg + "; color: " + fg + ";} ";
}

QString Style::mainWindow()
{
    const Theme t = theme();
    // QMainWindow{} alone has no visible effect -- its central widget (the
    // promoted CentralWidget class) fully occludes it edge-to-edge, so that
    // needs its own rule, matched by class name.
    return "QMainWindow{background-color: " + c(t.windowBackground) + ";} "
           "CentralWidget{background-color: " + c(t.windowBackground) + ";} ";
}

QString Style::messageBox()
{
    // Only the canvas: QMessageBox doesn't pick up Style::palette()'s
    // Window/WindowText on its own the way a QDialog subclass does, so it
    // still needs an explicit background/label rule. Its buttons used to get
    // a hand-picked navy skin here too -- left native now, same reasoning as
    // Style::pushButton().
    const Theme t = theme();
    QString style = R"(
    QMessageBox {
        background-color: )" + c(t.windowBackground) + R"(;
    }

    QMessageBox QLabel {
        color: )" + c(t.text) + R"(;
    }
    )";
    return style;
}

QString Style::colorDialog()
{
    // Only the canvas -- see Style::messageBox(). QLineEdit/QSpinBox/
    // QPushButton are left native now instead of the hand-picked navy skin.
    const Theme t = theme();
    QString style = R"(
    QColorDialog {
        background-color: )" + c(t.windowBackground) + R"(;
    }

    QColorDialog QLabel {
        color: )" + c(t.text) + R"(;
    }
    )";
    return style;
}

QString Style::menu()
{
    // Left native: Fusion draws QMenu off Style::palette()'s Window/
    // WindowText/Highlight already, no hand-picked navy skin needed.
    return QString();
}
