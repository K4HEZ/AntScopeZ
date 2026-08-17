#include "tdranalysisdialog.h"
#include "ui_tdranalysisdialog.h"
#include "style.h"
#include "settings.h"
#include <qcustomplot.h>

// Same "no real reflection" noise floor CalcTdr() itself uses to zero out
// m_pdTdrImp[]/tdrImpGraph samples (see measurements_tdr.cpp) -- reusing it
// here keeps "did we actually find a reflection" consistent with what the
// TDR math itself already decided was noise.
static const double kNoiseFloor = 0.015;

TDRAnalysisDialog::TDRAnalysisDialog(Measurements* measurements, bool measureSystemMetric,
                                       QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TDRAnalysisDialog),
    m_measurements(measurements),
    m_measureSystemMetric(measureSystemMetric)
{
    ui->setupUi(this);

    QString style = Style::dialog();
    style += Style::label();
    style += Style::lineEdit();
    style += Style::pushButton();
    style += Style::comboBox();
    style += Style::groupBox();
    setStyleSheet(style);

    QString unit = m_measureSystemMetric ? "m" : "ft";
    ui->knownLengthUnitLabel->setText(unit);

    populateCableTypeCombo();
    ui->velocityFactorEdit->setText(QString::number(m_measurements->cableVelFactor(), 'f', 3));

    connect(ui->cableTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TDRAnalysisDialog::onCableTypeChanged);
    connect(ui->velocityFactorEdit, &QLineEdit::textChanged,
            this, &TDRAnalysisDialog::recompute);
    connect(ui->knownLengthEdit, &QLineEdit::textChanged,
            this, &TDRAnalysisDialog::recompute);
    connect(ui->applyVfButton, &QPushButton::clicked,
            this, &TDRAnalysisDialog::onApplyCalculatedVf);

    recompute();
}

TDRAnalysisDialog::~TDRAnalysisDialog()
{
    delete ui;
}

void TDRAnalysisDialog::refresh()
{
    recompute();
}

void TDRAnalysisDialog::populateCableTypeCombo()
{
    m_cables = CableCatalog::load(Settings::programDataPath("cables.txt"));

    ui->cableTypeCombo->blockSignals(true);
    ui->cableTypeCombo->clear();
    ui->cableTypeCombo->addItem(tr("-- Select preset --"));
    for (const CableSpec& c : m_cables)
        ui->cableTypeCombo->addItem(c.name);
    ui->cableTypeCombo->setCurrentIndex(0);
    ui->cableTypeCombo->blockSignals(false);
}

void TDRAnalysisDialog::onCableTypeChanged(int index)
{
    if (index < 1 || index > m_cables.length())
        return; // placeholder, or stale index from a combo rebuild

    ui->velocityFactorEdit->setText(QString::number(m_cables.at(index - 1).velocityFactor, 'f', 3));
    // velocityFactorEdit::textChanged already triggers recompute().
}

TDRAnalysisDialog::Peak TDRAnalysisDialog::findPeak() const
{
    Peak p;
    if (m_measurements == nullptr || m_measurements->isEmpty())
        return p;

    // Same most-recent-measurement selection as
    // MarkerComparisonDialog::qFactorAt() -- see measurements.h's
    // cableVelFactor() comment and last()'s own comment for why this has
    // to be 0, not getMeasurementLength()-1.
    int mostRecent = 0;
    measurement* mm;
    switch (m_measurements->getFarEndMeasurement()) {
    case 1: mm = m_measurements->getMeasurementSub(mostRecent); break;
    case 2: mm = m_measurements->getMeasurementAdd(mostRecent); break;
    default: mm = m_measurements->last(); break;
    }
    if (mm == nullptr)
        return p;

    QCPDataMap& impMap = m_measureSystemMetric ? mm->tdrImpGraph : mm->tdrImpGraphFeet;
    QList<double> keys = impMap.keys(); // ascending distance order
    if (keys.isEmpty())
        return p;

    double bestKey = keys.at(0);
    double bestAmp = impMap.value(bestKey).value;
    for (int i = 1; i < keys.length(); ++i) {
        double amp = impMap.value(keys.at(i)).value;
        if (qAbs(amp) > qAbs(bestAmp)) {
            bestAmp = amp;
            bestKey = keys.at(i);
        }
    }

    // The stored key is a distance computed with whatever velocity factor
    // was active when redrawTDR() last ran (Measurements::cableVelFactor(),
    // fed from Settings > Cable). Distance is linear in velocity factor
    // (see m_tdrResolution's formula in CalcTdr()), so rescaling to this
    // dialog's own local velocity factor is exact and doesn't need
    // re-running the FFT -- only re-plotting would.
    double globalVf = m_measurements->cableVelFactor();
    double localVf = ui->velocityFactorEdit->text().toDouble();
    double ratio = (globalVf > 0 && localVf > 0) ? (localVf / globalVf) : 1.0;

    p.found = true;
    p.distance = bestKey * ratio;
    p.amplitude = bestAmp;

    // Peak sitting at (or past) the last few percent of this scan's
    // unambiguous range -- the real reflection may be farther out than
    // this scan can resolve. rangeNoteLabel surfaces this in recompute().
    double lastKey = keys.last();
    p.nearRangeEdge = (lastKey > 0 && bestKey >= 0.95 * lastKey);

    return p;
}

void TDRAnalysisDialog::recompute()
{
    Peak p = findPeak();
    ui->rangeNoteLabel->setText(QString());

    if (!p.found) {
        ui->cableLengthLabel->setText("--");
        ui->reflectionLabel->setText(tr("-- (run a scan on the TDR tab first)"));
        ui->calculatedVfLabel->setText("--");
        ui->applyVfButton->setEnabled(false);
        return;
    }

    QString unit = m_measureSystemMetric ? "m" : "ft";

    if (qAbs(p.amplitude) < kNoiseFloor) {
        ui->cableLengthLabel->setText(tr("n/a (no reflection above noise floor)"));
        ui->reflectionLabel->setText(tr("None detected"));
    } else {
        ui->cableLengthLabel->setText(QString("%1 %2").arg(p.distance, 0, 'f', 2).arg(unit));
        ui->reflectionLabel->setText(p.amplitude > 0
                                       ? tr("Open / high impedance")
                                       : tr("Short / near 0 %1").arg(QChar(0x03A9))); // Omega
        if (p.nearRangeEdge) {
            ui->rangeNoteLabel->setText(tr("Peak is near the edge of this scan's range -- "
                                            "the real reflection may be farther away than this "
                                            "scan can resolve. More sweep points raises the range."));
        }
    }

    // -- Reverse-solve: velocity factor from a known length --
    bool knownLengthValid = false;
    double knownLength = ui->knownLengthEdit->text().toDouble(&knownLengthValid);
    double localVf = ui->velocityFactorEdit->text().toDouble();
    if (knownLengthValid && knownLength > 0 && p.distance > 0 && localVf > 0
            && qAbs(p.amplitude) >= kNoiseFloor) {
        m_lastCalculatedVf = localVf * (knownLength / p.distance);
        ui->calculatedVfLabel->setText(QString::number(m_lastCalculatedVf, 'f', 3));
        ui->applyVfButton->setEnabled(true);
    } else {
        m_lastCalculatedVf = 0;
        ui->calculatedVfLabel->setText("--");
        ui->applyVfButton->setEnabled(false);
    }
}

void TDRAnalysisDialog::onApplyCalculatedVf()
{
    if (m_lastCalculatedVf <= 0)
        return;
    ui->cableTypeCombo->setCurrentIndex(0); // a reverse-solved VF won't match any preset
    ui->velocityFactorEdit->setText(QString::number(m_lastCalculatedVf, 'f', 3));
    // velocityFactorEdit::textChanged already triggers recompute().
    emit applyVelocityFactorAsCustom(m_lastCalculatedVf);
}
