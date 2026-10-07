#ifndef TUNINGPANEL_H
#define TUNINGPANEL_H

#include <QWidget>
#include <QList>
#include <QLabel>
#include "analyzerparameters.h"
#include "itubands.h"

class BandIndicator;

// Tuning mode's chart-area tab: a large SWR readout centered in the tab,
// with the band indicator along the bottom. The parameter grid is a separate
// widget (parametersWidget()) that MainWindow docks in the left column where
// Cursor Details sits in the other modes. Shows each sample as it arrives
// (no averaging -- the antenna changes while it's being adjusted).
class TuningPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TuningPanel(QWidget* parent = nullptr);

    // Heading plus the readings; MainWindow puts it in the left column.
    QWidget* parametersWidget() const { return m_params; }
    void setBands(const QList<BandPreset>& bands);
    void setFrequencyKHz(double khz);
    void clear();
    void addData(const GraphData& data);

    // Green up to 2.0, amber up to 3.0, red above.
    static QColor swrColor(double swr);

signals:
    void frequencyPicked(double khz);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void fitFontToLabel();

    QLabel* m_swrLabel;
    QList<QLabel*> m_values; // parameter grid, in setValues() order
    QWidget* m_params;
    bool m_fitting = false;
    QLabel* m_freqLabel;
    BandIndicator* m_band;
};

#endif // TUNINGPANEL_H
