#include "export.h"
#include "ui_export.h"
#include "filedialog.h"
#include "appconfig.h"
#include <QRegularExpression>
#include <QPushButton>
#include <QSignalBlocker>

Export::Export(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Export)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Save)->setDefault(true);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &Export::onSave);
    connect(ui->formatCombo, &QComboBox::currentIndexChanged, this, &Export::onFormatChanged);
    adjustSize();

    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);
    m_settings->beginGroup("Export");
    // Position only -- the size comes from the layout, so an old, smaller
    // saved size can't squeeze the text.
    QRect rect = m_settings->value("geometry", 0).toRect();
    if(rect.x() != 0) {
        this->move(rect.topLeft());
    }
    m_settings->endGroup();
}

Export::~Export()
{
    m_settings->beginGroup("Export");
    m_settings->setValue("geometry", this->geometry());
    m_settings->endGroup();

    delete ui;
}

void Export::setMeasurements(Measurements * _measurements, quint32 number)
{
    m_measurements = _measurements;
    m_measureNumber = number;
    updateDetails();
}

void Export::updateDetails()
{
    // Same resolution suggestedPath() already uses -- see its own comment
    // for why this is the measurement a click will actually export, not
    // just a plausible-looking guess.
    measurement* mm = m_measurements == nullptr ? nullptr
        : m_measurements->getMeasurement(m_measurements->getMeasurementLength() - 1 - m_measureNumber);

    bool isTwoPort = (mm != nullptr) && !mm->dataSParam.isEmpty();

    QString name = (mm != nullptr) ? mm->name : tr("(unknown)");
    QString points = (mm != nullptr) ? QString::number(mm->dataRX.length()) : "--";
    QString type = (mm == nullptr) ? "--"
        : (isTwoPort ? tr("2-port (S11, S21, S12, S22)") : tr("1-port (S11 only)"));

    ui->detailsLabel->setText(tr("Name: %1\nPoints: %2\nType: %3").arg(name, points, type));

    // The 2-port formats are only listed for a measurement with 2-port
    // data; the 1-port ones always are (its S11 is still valid 1-port data).
    updateCorrections(mm);
    buildFormats(isTwoPort);
    adjustSize();
}

// Each box starts checked when that correction is switched on; greyed out
// when it isn't available for this measurement; checked and locked when the
// measurement already has it built in (a file saved that way).
void Export::updateCorrections(const measurement* mm)
{
    Corrections cur = m_measurements != nullptr ? m_measurements->currentCorrections() : Corrections();

    bool oslBuiltIn = mm != nullptr && mm->applied.osl;
    bool oslAvail = mm != nullptr && mm->hasCalibrated();
    ui->oslCheckBox->setChecked(oslBuiltIn || (oslAvail && cur.osl));
    ui->oslCheckBox->setEnabled(oslAvail && !oslBuiltIn);
    ui->oslCheckBox->setText(oslBuiltIn ? tr("OSL calibration (already applied)")
                             : oslAvail ? tr("OSL calibration")
                             : tr("OSL calibration (not available for this measurement)"));

    int builtInCable = mm != nullptr ? mm->applied.cableMode : 0;
    QString mode = [](int m) {
        return m == 1 ? tr("Cable subtract") : m == 2 ? tr("Cable add") : tr("Cable add/subtract");
    }(builtInCable != 0 ? builtInCable : cur.cableMode);
    ui->cableCheckBox->setChecked(builtInCable != 0 || cur.cableMode != 0);
    ui->cableCheckBox->setEnabled(builtInCable == 0 && cur.cableMode != 0);
    if (builtInCable != 0) {
        ui->cableCheckBox->setText(tr("%1 (already applied)").arg(mode));
    } else if (cur.cableMode != 0) {
        bool metric = AppConfig::get().measureSystemMetric;
        double len = metric ? cur.cable.lengthFeet / FEETINMETER : cur.cable.lengthFeet;
        ui->cableCheckBox->setText(tr("%1 (%2 %3)").arg(mode).arg(len, 0, 'f', 2).arg(metric ? tr("m") : tr("ft")));
    } else {
        ui->cableCheckBox->setText(tr("Cable add/subtract (off in Settings > Cable)"));
    }
}

QString Export::suggestedPath(const QString &ext) const
{
    QString name = "Export";
    measurement* mm = m_measurements == nullptr ? nullptr
        : m_measurements->getMeasurement(m_measurements->getMeasurementLength() - 1 - m_measureNumber);
    if (mm != nullptr) {
        QString suggestedName = mm->name;
        suggestedName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
        suggestedName = suggestedName.trimmed();
        if (!suggestedName.isEmpty())
            name = suggestedName;
    }
    // withExtension(), not a plain "+ '.' + ext": name may already end in
    // a matching extension (e.g. re-exporting the same measurement to the
    // same format), and could contain other dots of its own (a date, a
    // decimal) that a naive strip-at-the-wrong-dot would mangle instead of
    // just removing the real extension. See FileDialog::withExtension()'s
    // own doc comment (issue reported 2026-08-14).
    return FileDialog::withExtension(FileDialog::userDataDir() + "/" + name, ext);
}

// AntScopeZ's own format -- listed first (export.ui). Not "most
// complete": saveData() writes the same frequency/R/X fields CSV/NWL/S1P
// do (see its own body, measurements_io.cpp), just as JSON doubles
// instead of rounded text (2-4 digits for those) -- a difference not
// actually worth promoting as an advantage, since real measurement
// precision is well below where 2-4 rounded digits would even show it
// (Harold's correction, 2026-09-06: don't oversell unrounded-ness as
// data quality). Never touches dataSParam either way -- a 2-port
// measurement's S21/S12/S22 aren't in an .asd file at all; S2P is the
// one with genuinely more data than this, not the other way around.
// saveData(), not exportData()/exportSParamData() like every other button
// here: a different Measurements method (JSON, not Touchstone/CSV/NWL),
// but m_measureNumber means the same plain row to all of them regardless
// (see Measurements::clearDirty()'s own comment).
void Export::buildFormats(bool twoPort)
{
    const QString s1p = "Touchstone (*.s1p)";
    const QString s2p = "Touchstone 2-port (*.s2p)";
    m_formats = {
        {"asd", tr("AntScopeZ (.asd)"), "asd", "AntScopeZ (*.asd)",
         tr("AntScopeZ's own format. Keeps the complete measurement: the points as "
            "received, the OSL-calibrated version if there is one, and a note of the "
            "corrections in effect. Doesn't hold 2-port (S21/S12) data."),
         Kind::Asd, 0},
        {"s1p-z-ri", tr("Touchstone Z, RI (.s1p)"), "s1p", s1p,
         tr("One-port impedance (Z), real/imaginary."), Kind::OnePort, 0},
        {"s1p-s-ri", tr("Touchstone S, RI (.s1p)"), "s1p", s1p,
         tr("One-port reflection (S11), real/imaginary."), Kind::OnePort, 1},
        {"s1p-s-ma", tr("Touchstone S, MA (.s1p)"), "s1p", s1p,
         tr("One-port reflection (S11), magnitude/angle."), Kind::OnePort, 2},
        {"s1p-s-db", tr("Touchstone S, dB (.s1p)"), "s1p", s1p,
         tr("One-port reflection (S11), magnitude in dB/angle."), Kind::OnePort, 3},
        {"csv", tr("CSV (.csv)"), "csv", "Comma Separated Values (*.csv)",
         tr("Frequency, R and X as comma-separated values."), Kind::OnePort, 0},
        {"nwl", tr("NWL (.nwl)"), "nwl", "APAK-EL (*.nwl)",
         tr("Frequency, series R and X, APAK-EL NWL format."), Kind::OnePort, 0},
    };
    if (twoPort) {
        m_formats << Format{"s2p-ri", tr("Touchstone 2-port, RI (.s2p)"), "s2p", s2p,
                            tr("All four S-parameters (S11, S21, S12, S22), real/imaginary."), Kind::TwoPort, 0}
                  << Format{"s2p-ma", tr("Touchstone 2-port, MA (.s2p)"), "s2p", s2p,
                            tr("All four S-parameters (S11, S21, S12, S22), magnitude/angle."), Kind::TwoPort, 1}
                  << Format{"s2p-db", tr("Touchstone 2-port, dB (.s2p)"), "s2p", s2p,
                            tr("All four S-parameters (S11, S21, S12, S22), magnitude in dB/angle."), Kind::TwoPort, 2};
    }

    // Last format used, if this measurement can be saved that way.
    m_settings->beginGroup("Export");
    QString last = m_settings->value("format", "asd").toString();
    m_settings->endGroup();

    QSignalBlocker block(ui->formatCombo);
    ui->formatCombo->clear();
    int select = 0;
    for (int i = 0; i < m_formats.size(); ++i) {
        ui->formatCombo->addItem(m_formats[i].label);
        if (m_formats[i].id == last)
            select = i;
    }
    ui->formatCombo->setCurrentIndex(select);
    onFormatChanged();
}

const Export::Format* Export::currentFormat() const
{
    int i = ui->formatCombo->currentIndex();
    return (i >= 0 && i < m_formats.size()) ? &m_formats[i] : nullptr;
}

void Export::onFormatChanged()
{
    const Format* f = currentFormat();
    if (f == nullptr)
        return;
    ui->formatDescription->setText(f->description);

    bool corrections = (f->kind == Kind::OnePort);
    ui->oslCheckBox->setVisible(corrections);
    ui->cableCheckBox->setVisible(corrections);
    if (f->kind == Kind::Asd)
        ui->correctionsNote->setText(tr("Not needed: AntScopeZ files keep the complete measurement, "
                                        "corrected and uncorrected."));
    else if (f->kind == Kind::TwoPort)
        ui->correctionsNote->setText(tr("2-port data isn't affected by AntScopeZ corrections."));
    else
        ui->correctionsNote->setText(tr("The analyzer's own calibration, if any, is always included."));
    adjustSize();
}

void Export::onSave()
{
    const Format* f = currentFormat();
    if (m_measurements == nullptr || f == nullptr)
        return;

    QString path = FileDialog::getSaveFileName(this, tr("Save"), suggestedPath(f->ext), f->filter);
    if (path.isEmpty())
        return;
    if (!path.endsWith("." + f->ext, Qt::CaseInsensitive))
        path += "." + f->ext;

    FileDialog::noteUserDataDirIfEnabled(path);
    switch (f->kind) {
    case Kind::Asd:
        m_measurements->saveData(m_measureNumber, path);
        break;
    case Kind::OnePort:
        m_measurements->exportData(path, f->type, m_measureNumber,
                                   ui->oslCheckBox->isChecked(), ui->cableCheckBox->isChecked());
        break;
    case Kind::TwoPort:
        m_measurements->exportSParamData(path, f->type, m_measureNumber);
        break;
    }
    m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment

    m_settings->beginGroup("Export");
    m_settings->setValue("format", f->id);
    m_settings->endGroup();
    accept();
}
