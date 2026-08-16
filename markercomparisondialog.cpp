#include "markercomparisondialog.h"
#include "ui_markercomparisondialog.h"
#include "style.h"
#include <markerspopup.h>
#include <qcustomplot.h>

MarkerComparisonDialog::MarkerComparisonDialog(Markers* markers, Measurements* measurements,
                                                 bool measureSystemMetric, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MarkerComparisonDialog),
    m_markers(markers),
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

    ui->antennaTypeCombo->addItem(tr("1/4-wave vertical"), QuarterWaveVertical);
    ui->antennaTypeCombo->addItem(tr("1/2-wave dipole"), HalfWaveDipole);
    ui->antennaTypeCombo->addItem(tr("Full-wave (loop/wire)"), FullWave);

    ui->antennaLengthUnitLabel->setText(m_measureSystemMetric ? "m" : "ft");

    // Trim-estimate display unit -- independent of the length field's unit
    // above (see markercomparisondialog.h). Defaults to whichever of
    // inches/cm matches the app's global Metric/Imperial setting, but is
    // freely changeable from here without touching that setting.
    ui->trimUnitsCombo->addItem(tr("Feet"), Feet);
    ui->trimUnitsCombo->addItem(tr("Inches"), Inches);
    ui->trimUnitsCombo->addItem(tr("Meters"), Meters);
    ui->trimUnitsCombo->addItem(tr("Cm"), Cm);
    ui->trimUnitsCombo->setCurrentIndex(ui->trimUnitsCombo->findData(m_measureSystemMetric ? (int)Cm : (int)Inches));

    populateMarkerCombos();

    connect(ui->currentMarkerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkerComparisonDialog::recompute);
    connect(ui->targetMarkerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkerComparisonDialog::recompute);
    connect(ui->antennaTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkerComparisonDialog::recompute);
    connect(ui->antennaLengthEdit, &QLineEdit::textChanged,
            this, &MarkerComparisonDialog::recompute);
    connect(ui->trimUnitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkerComparisonDialog::recompute);

    recompute();
}

MarkerComparisonDialog::~MarkerComparisonDialog()
{
    delete ui;
}

// Called on show and hooked to the analyzer's measurementComplete() signal
// (see MainWindow::on_actionMarkerComparison_triggered()) so a marker added/
// removed elsewhere, or a fresh sweep during Continuous scan, is reflected
// here without the user having to reopen the dialog.
void MarkerComparisonDialog::refresh()
{
    populateMarkerCombos();
    recompute();
}

void MarkerComparisonDialog::populateMarkerCombos()
{
    int prevCurrent = ui->currentMarkerCombo->currentData().toInt();
    int prevTarget = ui->targetMarkerCombo->currentData().toInt();

    ui->currentMarkerCombo->blockSignals(true);
    ui->targetMarkerCombo->blockSignals(true);

    ui->currentMarkerCombo->clear();
    ui->targetMarkerCombo->clear();

    int count = m_markers->getMarkersCount();
    for (int i = 1; i <= count; ++i) {
        marker m = m_markers->getMarker(i - 1); // getMarker() is 0-based
        QString label = QString("%1: %2 kHz").arg(i).arg(m.frequency, 0, 'f', 3);
        ui->currentMarkerCombo->addItem(label, i);
        ui->targetMarkerCombo->addItem(label, i);
    }

    // Keep the previous selection if that marker number still exists;
    // otherwise default to marker 1 as Current and marker 2 (if it
    // exists) as Target, matching the dip/target framing this dialog is
    // built around.
    int curIdx = ui->currentMarkerCombo->findData(prevCurrent);
    ui->currentMarkerCombo->setCurrentIndex(curIdx >= 0 ? curIdx : 0);
    int tgtIdx = ui->targetMarkerCombo->findData(prevTarget);
    ui->targetMarkerCombo->setCurrentIndex(tgtIdx >= 0 ? tgtIdx : (count > 1 ? 1 : 0));

    ui->currentMarkerCombo->blockSignals(false);
    ui->targetMarkerCombo->blockSignals(false);
}

MarkerComparisonDialog::MarkerReadout MarkerComparisonDialog::readoutForMarker(int markerNumber)
{
    MarkerReadout r;
    if (markerNumber < 1)
        return r;

    // Mirrors the real [Markers]header convention (fixed Delete/Marker/
    // Serie/FQ columns first) -- see Markers::computeMarkerRow()/
    // updateInfo() in markers.cpp for why the row comes back in exactly
    // this order. Positions 0-3 aren't actually read from this list (they're
    // always the same four fixed fields), only their presence/count matters.
    QList<int> columns;
    columns << MarkersHeaderColumn::fieldDelete << MarkersHeaderColumn::fieldNum
            << MarkersHeaderColumn::fieldSerie  << MarkersHeaderColumn::fieldFQ
            << MarkersHeaderColumn::fieldSWR    << MarkersHeaderColumn::fieldRL
            << MarkersHeaderColumn::fieldR      << MarkersHeaderColumn::fieldX
            << MarkersHeaderColumn::fieldL      << MarkersHeaderColumn::fieldC;

    QList<QVariant> row = m_markers->valuesForMarkerNumber(markerNumber, columns);
    if (row.size() < 10)
        return r; // no measurement yet, or markerNumber out of range

    r.valid = true;
    r.frequency = row.at(3).toDouble(); // fieldFQ, kHz
    r.swr       = row.at(4).toDouble();
    r.rl        = row.at(5).toDouble();
    r.r         = row.at(6).toDouble();
    r.x         = row.at(7).toDouble();
    r.l         = row.at(8).toDouble(); // nH
    r.c         = row.at(9).toDouble(); // pF
    return r;
}

// 2:1-SWR bandwidth around centerFq, walked from the nearest data point in
// the most recent measurement's SWR trace outward in both directions until
// SWR exceeds 2:1 (or the sweep's own edge is reached, in which case there's
// no real crossing to report). Q = center frequency / that bandwidth --
// the same practical, SWR-bandwidth definition of Q used for antenna/match
// characterization (as opposed to a lab Q meter's own definition).
double MarkerComparisonDialog::qFactorAt(double centerFq)
{
    if (m_measurements->isEmpty())
        return 0;

    int mostRecent = m_measurements->getMeasurementLength() - 1; // same index last() uses
    measurement* mm;
    switch (m_measurements->getFarEndMeasurement()) {
    case 1: mm = m_measurements->getMeasurementSub(mostRecent); break;
    case 2: mm = m_measurements->getMeasurementAdd(mostRecent); break;
    default: mm = m_measurements->last(); break;
    }
    if (mm == nullptr)
        return 0;

    bool calib = m_measurements->getCalibrationEnabled();
    QCPDataMap* swrMap = calib ? &mm->swrGraphCalib : &mm->swrGraph;

    QList<double> keys = swrMap->keys(); // ascending frequency order
    if (keys.isEmpty())
        return 0;

    int centerIdx = 0;
    double bestDist = qAbs(keys.at(0) - centerFq);
    for (int i = 1; i < keys.length(); ++i) {
        double dist = qAbs(keys.at(i) - centerFq);
        if (dist < bestDist) {
            bestDist = dist;
            centerIdx = i;
        }
    }

    const double threshold = 2.0;
    int lowIdx = centerIdx;
    while (lowIdx > 0 && swrMap->value(keys.at(lowIdx)).value <= threshold)
        lowIdx--;
    int highIdx = centerIdx;
    while (highIdx < keys.length() - 1 && swrMap->value(keys.at(highIdx)).value <= threshold)
        highIdx++;

    // Ran off one (or both) edges of the sweep without crossing 2:1 --
    // no real bandwidth to report.
    if (lowIdx == 0 || highIdx == keys.length() - 1)
        return 0;

    double bandwidth = keys.at(highIdx) - keys.at(lowIdx);
    if (bandwidth <= 0)
        return 0;

    return centerFq / bandwidth;
}

double MarkerComparisonDialog::nominalLengthConstantFeet(AntennaType type)
{
    // Classic ham rule-of-thumb constants: L(ft) = K / f(MHz). Half-wave
    // dipole (468) and quarter-wave vertical (234 -- exactly half the
    // dipole figure, since a quarter-wave monopole over a ground plane is
    // electrically half a dipole) are the long-standing ARRL Handbook
    // figures; full-wave (1005) is the equally standard full-wave loop
    // figure. All three already bake in typical end-effect/velocity-factor
    // shortening for a bare wire in free space -- a starting estimate, not
    // a substitute for this particular antenna's real length (see the
    // "Current length" field, which overrides this when filled in).
    switch (type) {
    case QuarterWaveVertical: return 234.0;
    case HalfWaveDipole:      return 468.0;
    case FullWave:            return 1005.0;
    }
    return 468.0;
}

double MarkerComparisonDialog::feetToTrimUnit(double feet, TrimUnit unit)
{
    switch (unit) {
    case Feet:   return feet;
    case Inches: return feet * 12.0;
    case Meters: return feet * 0.3048;
    case Cm:     return feet * 30.48;
    }
    return feet;
}

QString MarkerComparisonDialog::trimUnitSuffix(TrimUnit unit)
{
    switch (unit) {
    case Feet:   return "ft";
    case Inches: return "in";
    case Meters: return "m";
    case Cm:     return "cm";
    }
    return "ft";
}

void MarkerComparisonDialog::recompute()
{
    int currentMarkerNum = ui->currentMarkerCombo->currentData().toInt();
    int targetMarkerNum = ui->targetMarkerCombo->currentData().toInt();

    MarkerReadout cur = readoutForMarker(currentMarkerNum);
    MarkerReadout tgt = readoutForMarker(targetMarkerNum);

    if (!cur.valid || !tgt.valid) {
        // Not enough markers placed yet, or no measurement to read from.
        const QString none = "--";
        ui->deltaFrequencyLabel->setText(none);
        ui->deltaSwrLabel->setText(none);
        ui->deltaReturnLossLabel->setText(none);
        ui->deltaResistanceLabel->setText(none);
        ui->deltaReactanceLabel->setText(none);
        ui->qFactorLabel->setText(none);
        ui->equivalentLLabel->setText(none);
        ui->equivalentCLabel->setText(none);
        ui->calculatedTrimLabel->setText(none);
        ui->suggestedCutLabel->setText(none);
        ui->perLegLabel->setText(none);
        return;
    }

    // -- Marker comparison --
    ui->deltaFrequencyLabel->setText(QString("%1 kHz").arg(tgt.frequency - cur.frequency, 0, 'f', 3));
    ui->deltaSwrLabel->setText(QString::number(tgt.swr - cur.swr, 'f', 2));
    ui->deltaReturnLossLabel->setText(QString("%1 dB").arg(tgt.rl - cur.rl, 0, 'f', 2));
    ui->deltaResistanceLabel->setText(QString("%1 Ohm").arg(tgt.r - cur.r, 0, 'f', 2));
    ui->deltaReactanceLabel->setText(QString("%1 Ohm").arg(tgt.x - cur.x, 0, 'f', 2));

    double q = qFactorAt(cur.frequency);
    ui->qFactorLabel->setText(q > 0 ? QString::number(q, 'f', 1)
                                     : tr("n/a (no 2:1 crossing found)"));
    ui->equivalentLLabel->setText(QString("%1 nH").arg(cur.l, 0, 'f', 1));
    ui->equivalentCLabel->setText(QString("%1 pF").arg(cur.c, 0, 'f', 1));

    // -- Trim calculator --
    if (cur.frequency <= 0 || tgt.frequency <= 0) {
        ui->calculatedTrimLabel->setText("--");
        ui->suggestedCutLabel->setText("--");
        ui->perLegLabel->setText("--");
        return;
    }

    AntennaType type = static_cast<AntennaType>(ui->antennaTypeCombo->currentData().toInt());
    double kFeet = nominalLengthConstantFeet(type); // feet, at 1 MHz -- all trim math below is in feet

    double fDipMHz = cur.frequency / 1000.0;
    double fTargetMHz = tgt.frequency / 1000.0;

    bool userSuppliedLength = false;
    double lengthCurrentFeet = ui->antennaLengthEdit->text().toDouble(&userSuppliedLength);
    if (userSuppliedLength && lengthCurrentFeet > 0) {
        // The length field's own unit follows the app's global Metric/
        // Imperial setting (see antennaLengthUnitLabel in the constructor)
        // -- convert to feet for the math below if it was entered in meters.
        if (m_measureSystemMetric)
            lengthCurrentFeet /= 0.3048;
    } else {
        lengthCurrentFeet = kFeet / fDipMHz; // nominal-formula stand-in -- see nominalLengthConstantFeet()
    }

    double lengthNewFeet = lengthCurrentFeet * (fDipMHz / fTargetMHz);
    double trimFeet = lengthCurrentFeet - lengthNewFeet; // positive == shorten, negative == lengthen
    double suggestedCutFeet = trimFeet * 0.5;

    TrimUnit trimUnit = static_cast<TrimUnit>(ui->trimUnitsCombo->currentData().toInt());
    QString unit = trimUnitSuffix(trimUnit);
    auto describe = [&](double feetAmount) -> QString {
        double amount = feetToTrimUnit(feetAmount, trimUnit);
        if (qAbs(amount) < 1e-6)
            return QString("0 %1").arg(unit);
        QString dir = amount > 0 ? tr("Shorten by") : tr("Lengthen by");
        return QString("%1 %2 %3").arg(dir).arg(qAbs(amount), 0, 'f', 2).arg(unit);
    };

    ui->calculatedTrimLabel->setText(describe(trimFeet));
    ui->suggestedCutLabel->setText(describe(suggestedCutFeet));

    if (type == HalfWaveDipole)
        ui->perLegLabel->setText(describe(trimFeet / 2.0));
    else
        ui->perLegLabel->setText(tr("n/a (single element)"));
}
