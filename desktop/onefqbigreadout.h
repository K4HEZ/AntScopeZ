#ifndef ONEFQBIGREADOUT_H
#define ONEFQBIGREADOUT_H

#include <QDialog>
#include <QLabel>
#include "analyzerparameters.h"

// Alternate One-Fq floating-window style (OneFqDisplayStyle::BigReadout,
// see onefqwidget.h): a plain, resizable, native-chrome dialog showing
// just SWR as one giant bold number with a small "SWR" caption
// underneath -- a glanceable tuning aid, as opposed to OneFqWidget's
// packed 11-field technical dump. Measurements owns which style is
// currently live and swaps between them on double-click (see
// styleToggleRequested()), feeding both through the same GraphData shape.
//
// Deliberately left with no per-widget stylesheet -- Style::dialog()'s
// comment explains why: the app's global stylesheet already themes plain
// QDialog/QLabel backgrounds and text, so this picks up "app text color
// on app background" for free and stays in sync with theme changes with
// no extra wiring.
class OneFqBigReadout : public QDialog
{
    Q_OBJECT

public:
    explicit OneFqBigReadout(QWidget* parent = nullptr);

    void addData(const GraphData& data);

signals:
    // Emitted from both reject() (Esc) and closeEvent() (title-bar close
    // button) -- QDialog::reject() alone doesn't fire closeEvent(), same
    // split as TdrScanDialog::closing(). Purely a notification: this
    // dialog's own Qt::WA_DeleteOnClose (set by Measurements at
    // construction) handles its teardown, so listeners must not
    // close()/delete it again from this signal -- see
    // Measurements::onOneFqBigReadoutClosing().
    void closing();
    // Double-click anywhere on the dialog -- toggles back to OneFqWidget's
    // Detailed style, see Measurements::toggleOneFqDisplayStyle().
    void styleToggleRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void updateText();
    void fitFontToLabel();

    QLabel* m_valueLabel;
    QLabel* m_captionLabel;
    double m_swr = DBL_MAX;
};

#endif // ONEFQBIGREADOUT_H
