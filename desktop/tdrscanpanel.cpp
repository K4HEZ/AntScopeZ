#include "tdrscanpanel.h"
#include "ui_tdrscanpanel.h"
#include "settings.h"
#include "cablepickerdialog.h"

// Same "no real reflection" noise floor CalcTdr() itself uses to zero out
// m_pdTdrImp[]/tdrImpGraph samples (see measurements_tdr.cpp) -- reusing it
// here keeps "did we actually find a reflection" consistent with what the
// TDR math itself already decided was noise. Moved from TDRAnalysisDialog
// 2026-08-21.
static const double kTdrPeakNoiseFloor = 0.015;

TdrScanPanel::TdrScanPanel(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::TdrScanPanel)
{
    ui->setupUi(this);

    ui->topFreqSlider->setRange((int)m_minFqKHz, (int)m_maxFqKHz);
    ui->topFreqEdit->setText(QString::number(m_maxFqKHz));

    ui->dotsSlider->setRange(TDR_MINPOINTS, TDR_MAXPOINTS);
    ui->dotsSlider->setValue(TDR_MINPOINTS);
    ui->dotsEdit->setText(QString::number(TDR_MINPOINTS));

    ui->kaiserBetaNameLabel->setVisible(false);
    ui->kaiserBetaSpin->setVisible(false);

    loadCables();

    connect(ui->topFreqSlider, &QSlider::valueChanged, this, &TdrScanPanel::onTopFreqSliderChanged);
    connect(ui->topFreqEdit, &QLineEdit::editingFinished, this, &TdrScanPanel::onTopFreqEditChanged);
    connect(ui->dotsSlider, &QSlider::valueChanged, this, &TdrScanPanel::onDotsSliderChanged);
    connect(ui->dotsEdit, &QLineEdit::editingFinished, this, &TdrScanPanel::onDotsEditChanged);
    // textChanged (not editingFinished) -- picking a cable preset
    // calls velocityFactorEdit->setText() programmatically, which only
    // fires textChanged, not editingFinished. Matches TDRAnalysisDialog's
    // own original wiring.
    connect(ui->velocityFactorEdit, &QLineEdit::textChanged, this, &TdrScanPanel::onVelocityFactorEdited);
    connect(ui->cableTypeButton, &QPushButton::clicked, this, &TdrScanPanel::onCableTypeClicked);
    connect(ui->knownLengthEdit, &QLineEdit::textChanged, this, &TdrScanPanel::refreshResult);
    connect(ui->applyVfButton, &QPushButton::clicked, this, &TdrScanPanel::onApplyVfClicked);
    connect(ui->windowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TdrScanPanel::onWindowComboChanged);
    connect(ui->tdrSingleButton, &QPushButton::clicked, this, &TdrScanPanel::onScanClicked);
    connect(ui->kaiserBetaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double beta) {
        if (windowForComboIndex(ui->windowCombo->currentIndex()) == TdrWindow::Kaiser)
            emit windowChanged(TdrWindow::Kaiser, beta);
    });

    ui->windowCombo->setCurrentIndex(1); // Hamming -- matches Measurements' own default

    updateWindowExplanation();
    updateEstimateLabels();
    refreshResult();
}

TdrScanPanel::~TdrScanPanel()
{
    delete ui;
}

void TdrScanPanel::loadCables()
{
    m_cables = CableCatalog::load(Settings::programDataPath("cables.txt"));
    updateCableButton();
}

void TdrScanPanel::updateCableButton()
{
    ui->cableTypeButton->setText(m_cableName.isEmpty() ? tr("Select cable preset...") : m_cableName);
}

void TdrScanPanel::onCableTypeClicked()
{
    QStringList names;
    for (const CableSpec& c : m_cables)
        names << c.name;
    const QString name = CablePickerDialog::pick(this, names, m_cableName);
    const int i = names.indexOf(name);
    if (i < 0)
        return;

    m_pickingCable = true;
    ui->velocityFactorEdit->setText(QString::number(m_cables.at(i).velocityFactor, 'f', 3));
    m_pickingCable = false;
    m_cableName = name;
    updateCableButton();
    // velocityFactorEdit::textChanged already triggers onVelocityFactorEdited().
}

void TdrScanPanel::setFrequencyLimits(qint64 minFqKHz, qint64 maxFqKHz)
{
    Q_UNUSED(minFqKHz); // TDR always floors at TDR_MIN_FREQUENCY regardless -- see the class comment
    m_minFqKHz = TDR_MIN_FREQUENCY;
    m_maxFqKHz = qMax(m_minFqKHz, maxFqKHz);

    bool wasBlocked = ui->topFreqSlider->blockSignals(true);
    ui->topFreqSlider->setRange((int)m_minFqKHz, (int)m_maxFqKHz);
    // Default to the full range every time this panel opens -- not
    // persisted (see the class comment / tdr-scan-rework-plan memory).
    ui->topFreqSlider->setValue((int)m_maxFqKHz);
    ui->topFreqSlider->blockSignals(wasBlocked);

    ui->topFreqEdit->setText(QString::number(ui->topFreqSlider->value()));
    updateEstimateLabels();
}

void TdrScanPanel::setVelocityFactor(double vf)
{
    ui->velocityFactorEdit->setText(QString::number(vf, 'f', 3));
    // velocityFactorEdit::textChanged already triggers onVelocityFactorEdited().
}

void TdrScanPanel::setMeasureSystemMetric(bool metric)
{
    m_measureSystemMetric = metric;
    ui->knownLengthUnitLabel->setText(metric ? "m" : "ft");
    updateEstimateLabels();
    refreshResult();
}

void TdrScanPanel::setMeasurements(Measurements* measurements)
{
    m_measurements = measurements;
    refreshResult();
}

void TdrScanPanel::setScanning(bool scanning)
{
    m_scanning = scanning;
    ui->topFreqSlider->setEnabled(!scanning);
    ui->topFreqEdit->setEnabled(!scanning);
    ui->dotsSlider->setEnabled(!scanning);
    ui->dotsEdit->setEnabled(!scanning);
    ui->velocityFactorEdit->setEnabled(!scanning);
    ui->cableTypeButton->setEnabled(!scanning);
    updateScanButtonEnabled();
    // windowCombo/kaiserBetaSpin stay enabled while scanning -- changing the
    // window only re-plots already-captured data, doesn't touch the
    // in-flight request (see windowChanged()'s comment).
}

void TdrScanPanel::setConnected(bool connected)
{
    m_connected = connected;
    updateScanButtonEnabled();
}

void TdrScanPanel::updateScanButtonEnabled()
{
    ui->tdrSingleButton->setEnabled(m_connected && !m_scanning);
}

void TdrScanPanel::onTopFreqSliderChanged(int value)
{
    ui->topFreqEdit->setText(QString::number(value));
    updateEstimateLabels();
}

void TdrScanPanel::onTopFreqEditChanged()
{
    bool ok = false;
    qint64 value = ui->topFreqEdit->text().toLongLong(&ok);
    if (!ok)
        value = ui->topFreqSlider->value();
    value = qBound(m_minFqKHz, value, m_maxFqKHz);

    bool wasBlocked = ui->topFreqSlider->blockSignals(true);
    ui->topFreqSlider->setValue((int)value);
    ui->topFreqSlider->blockSignals(wasBlocked);

    ui->topFreqEdit->setText(QString::number(value));
    updateEstimateLabels();
}

void TdrScanPanel::onDotsSliderChanged(int value)
{
    ui->dotsEdit->setText(QString::number(value));
    updateEstimateLabels();
}

void TdrScanPanel::onDotsEditChanged()
{
    bool ok = false;
    int value = ui->dotsEdit->text().toInt(&ok);
    if (!ok)
        value = ui->dotsSlider->value();
    value = qBound((int)TDR_MINPOINTS, value, (int)TDR_MAXPOINTS);

    bool wasBlocked = ui->dotsSlider->blockSignals(true);
    ui->dotsSlider->setValue(value);
    ui->dotsSlider->blockSignals(wasBlocked);

    ui->dotsEdit->setText(QString::number(value));
    updateEstimateLabels();
}

void TdrScanPanel::onVelocityFactorEdited()
{
    if (!m_pickingCable && !m_cableName.isEmpty()) {
        m_cableName.clear();
        updateCableButton();
    }
    updateEstimateLabels();
    refreshResult();
}

TdrWindow TdrScanPanel::windowForComboIndex(int index)
{
    // Matches tdrscanpanel.ui's windowCombo item order.
    switch (index)
    {
    case 0: return TdrWindow::Rectangular;
    case 2: return TdrWindow::Hann;
    case 3: return TdrWindow::Blackman;
    case 4: return TdrWindow::Kaiser;
    case 1:
    default: return TdrWindow::Hamming;
    }
}

void TdrScanPanel::onWindowComboChanged(int index)
{
    TdrWindow window = windowForComboIndex(index);
    bool isKaiser = (window == TdrWindow::Kaiser);
    ui->kaiserBetaNameLabel->setVisible(isKaiser);
    ui->kaiserBetaSpin->setVisible(isKaiser);
    updateWindowExplanation();
    emit windowChanged(window, ui->kaiserBetaSpin->value());
}

void TdrScanPanel::updateWindowExplanation()
{
    // Condensed from docs/tdr-use.md's table -- keep the two in sync if
    // either changes.
    static const QString explanations[] = {
        tr("Sharpest resolution, most ringing near a strong reflection -- "
           "use to separate two close, comparably-strong reflections."),
        tr("General-purpose default -- good resolution, low ringing near a "
           "strong reflection."),
        tr("Slightly softer resolution than Hamming, quieter further from a "
           "strong reflection -- use when hunting a small fault well away "
           "from a dominant one."),
        tr("Lowest ringing, softest resolution -- use when a strong "
           "reflection (e.g. an open/shorted far end) might be masking a "
           "weaker fault nearby."),
        tr("Adjustable via beta -- higher beta trades resolution for lower "
           "ringing, continuously between Rectangular- and Blackman-like "
           "extremes."),
    };
    int index = ui->windowCombo->currentIndex();
    if (index >= 0 && index < (int)(sizeof(explanations)/sizeof(explanations[0])))
        ui->windowExplanationLabel->setText(explanations[index]);
}

double TdrScanPanel::velocityFactor() const
{
    bool ok = false;
    double vf = ui->velocityFactorEdit->text().toDouble(&ok);
    if (!ok || vf <= 0 || vf > 1.0)
        return 0.66; // sane fallback -- same default Settings > Cable's own ideal-cable presets use
    return vf;
}

void TdrScanPanel::updateEstimateLabels()
{
    bool ok = false;
    int dots = ui->dotsEdit->text().toInt(&ok);
    if (!ok)
        dots = ui->dotsSlider->value();
    qint64 topFreqKHz = ui->topFreqEdit->text().toLongLong(&ok);
    if (!ok)
        topFreqKHz = ui->topFreqSlider->value();

    // calcTdrEstimate() takes MHz (matches RawData::fq's own unit, see its
    // declaration in measurements.h); this panel's fields are kHz to match
    // the rest of the app's frequency-field convention.
    double topFreqMHz = (double)topFreqKHz / 1000.0;
    TdrMath::Estimate est =
        TdrMath::estimate(dots, topFreqMHz, velocityFactor(), m_measureSystemMetric);

    if (est.fftSize == 0)
    {
        ui->unambiguousRangeLabel->setText("--");
        ui->resolutionLabel->setText("--");
        return;
    }

    QString unit = m_measureSystemMetric ? "m" : "ft";
    ui->unambiguousRangeLabel->setText(QString("%1 %2").arg(est.unambiguousRange, 0, 'f', 2).arg(unit));
    ui->resolutionLabel->setText(QString("%1 %2").arg(est.resolution, 0, 'f', 2).arg(unit));
}

void TdrScanPanel::refreshResult()
{
    ui->rangeNoteLabel->setText(QString());

    if (m_measurements == nullptr) {
        ui->cableLengthLabel->setText("--");
        ui->reflectionLabel->setText(tr("-- (run a TDR scan first)"));
        ui->calculatedVfLabel->setText("--");
        ui->applyVfButton->setEnabled(false);
        return;
    }

    Measurements::TdrPeak p = m_measurements->findTdrPeak(m_measureSystemMetric, velocityFactor());

    if (!p.found) {
        ui->cableLengthLabel->setText("--");
        ui->reflectionLabel->setText(tr("-- (run a TDR scan first)"));
        ui->calculatedVfLabel->setText("--");
        ui->applyVfButton->setEnabled(false);
        return;
    }

    QString unit = m_measureSystemMetric ? "m" : "ft";
    QStringList notes;

    if (qAbs(p.amplitude) < kTdrPeakNoiseFloor) {
        ui->cableLengthLabel->setText(tr("n/a (no reflection above noise floor)"));
        ui->reflectionLabel->setText(tr("None detected"));
    } else {
        ui->cableLengthLabel->setText(QString("%1 %2").arg(p.distance, 0, 'f', 2).arg(unit));
        // Impedance -- read straight from tdrZGraph (see findTdrPeak()'s
        // comment), not just the open/short binary this used to be.
        ui->reflectionLabel->setText(p.amplitude > 0
                                       ? tr("Open (≈ %1 %2)").arg(p.impedanceOhms, 0, 'f', 0).arg(QChar(0x03A9))
                                       : tr("Short (≈ %1 %2)").arg(p.impedanceOhms, 0, 'f', 0).arg(QChar(0x03A9)));
        if (p.nearRangeEdge) {
            notes << tr("Peak is near the edge of this scan's range -- "
                        "the real reflection may be farther away than this "
                        "scan can resolve. More sweep points raises the range.");
        }

        // Fault vs. far-end -- if a known cable length is entered, compare
        // it against where the peak actually is. "Meaningfully shorter"
        // uses this scan's own resolution estimate (calcTdrEstimate(), same
        // formula Scan setup's "Resolution (estimate)" row already shows)
        // as the tolerance, rather than an arbitrary percentage -- a
        // difference smaller than the scan's own resolving power isn't
        // distinguishable from measurement noise anyway.
        bool knownLengthValidForFault = false;
        double knownLengthForFault = ui->knownLengthEdit->text().toDouble(&knownLengthValidForFault);
        if (knownLengthValidForFault && knownLengthForFault > 0) {
            bool ok = false;
            int dots = ui->dotsEdit->text().toInt(&ok);
            if (!ok) dots = ui->dotsSlider->value();
            qint64 topFreqKHz = ui->topFreqEdit->text().toLongLong(&ok);
            if (!ok) topFreqKHz = ui->topFreqSlider->value();
            TdrMath::Estimate est = TdrMath::estimate(
                dots, (double)topFreqKHz / 1000.0, velocityFactor(), m_measureSystemMetric);

            if (est.fftSize != 0 && (knownLengthForFault - p.distance) > est.resolution) {
                notes << tr("Peak is %1 %2 short of the entered cable length -- "
                            "possibly a fault partway along the cable rather than "
                            "just the far end.")
                             .arg(knownLengthForFault - p.distance, 0, 'f', 2).arg(unit);
            }
        }
    }
    ui->rangeNoteLabel->setText(notes.join(" "));

    // -- Reverse-solve: velocity factor from a known length --
    bool knownLengthValid = false;
    double knownLength = ui->knownLengthEdit->text().toDouble(&knownLengthValid);
    double localVf = velocityFactor();
    if (knownLengthValid && knownLength > 0 && p.distance > 0 && localVf > 0
            && qAbs(p.amplitude) >= kTdrPeakNoiseFloor) {
        m_lastCalculatedVf = localVf * (knownLength / p.distance);
        ui->calculatedVfLabel->setText(QString::number(m_lastCalculatedVf, 'f', 3));
        ui->applyVfButton->setEnabled(true);
    } else {
        m_lastCalculatedVf = 0;
        ui->calculatedVfLabel->setText("--");
        ui->applyVfButton->setEnabled(false);
    }
}

void TdrScanPanel::onApplyVfClicked()
{
    if (m_lastCalculatedVf <= 0)
        return;
    setVelocityFactor(m_lastCalculatedVf); // copies it up into the Scan setup field, resets the cable button
    emit applyVelocityFactorAsCustom(m_lastCalculatedVf);
}

void TdrScanPanel::onScanClicked()
{
    bool ok = false;
    qint64 topFreqKHz = ui->topFreqEdit->text().toLongLong(&ok);
    if (!ok)
        topFreqKHz = ui->topFreqSlider->value();
    int dots = ui->dotsEdit->text().toInt(&ok);
    if (!ok)
        dots = ui->dotsSlider->value();
    TdrWindow window = windowForComboIndex(ui->windowCombo->currentIndex());
    emit scanRequested(topFreqKHz, dots, window, ui->kaiserBetaSpin->value(), velocityFactor());
}
