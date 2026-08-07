#include "style.h"

bool Style::m_dark = true;

namespace {

Theme darkTheme()
{
    Theme t;
    t.windowBackground = QColor(18, 18, 18);
    t.text             = QColor(255, 255, 255);
    t.textMuted         = QColor(128, 128, 128);
    t.border            = QColor(90, 90, 95);
    return t;
}

Theme lightTheme()
{
    Theme t;
    t.windowBackground = QColor(246, 246, 246);
    t.text             = QColor(30, 30, 30);
    t.textMuted         = QColor(110, 110, 110);
    t.border            = QColor(195, 195, 200);
    return t;
}

QString c(const QColor& color)
{
    return color.name();
}

} // namespace

void Style::setDarkMode(bool dark)
{
    m_dark = dark;
}

bool Style::isDarkMode()
{
    return m_dark;
}

QPalette Style::palette()
{
    const Theme& t = theme();
    QPalette p;

    const bool canvasIsLight = t.windowBackground.lightness() > 128;

    p.setColor(QPalette::Window, t.windowBackground);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.windowBackground);
    // A subtle alternating-row tint -- nudge toward white on a light canvas,
    // toward black on a dark one, whichever direction "toward" is.
    p.setColor(QPalette::AlternateBase, canvasIsLight
               ? t.windowBackground.darker(104) : t.windowBackground.lighter(115));
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

const Theme& Style::theme()
{
    static const Theme dark = darkTheme();
    static const Theme light = lightTheme();
    return m_dark ? dark : light;
}

QString Style::label()
{
    const Theme& t = theme();
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
    // disabled rules below it were dead code besides: nothing in the app
    // ever calls QLineEdit::setReadOnly(true), and Fusion already dims
    // disabled fields off the palette's Disabled group.
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
    // fine, no need for the custom checked.png/unchecked.png glyphs anymore.
    return QString();
}

QString Style::groupBox()
{
    const Theme& t = theme();
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
    // Left native -- Fusion's combo box already reads Button/Base correctly
    // from Style::palette().
    return QString();
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
    const Theme& t = theme();
    return "QDialog{background-color: " + c(t.windowBackground) + ";} ";
}

QString Style::mainWindow()
{
    const Theme& t = theme();
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
    const Theme& t = theme();
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
    const Theme& t = theme();
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
