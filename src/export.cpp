#include "export.h"
#include "ui_export.h"
#include "filedialog.h"
#include <QRegularExpression>

Export::Export(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Export)
{
    ui->setupUi(this);
    adjustSize();

    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);
    m_settings->beginGroup("Export");
    QRect rect = m_settings->value("geometry", 0).toRect();
    if(rect.x() != 0) {
        this->setGeometry(rect);
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

void Export::setMeasurements(Measurements * _measurements, quint32 number, bool _applyCable, QString _description)
{
    m_measurements = _measurements;
    m_measureNumber = number;
    m_bApplyCable = _applyCable;
    m_description = _description;
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

    // S2P export only makes sense -- and only appears -- for a measurement
    // that actually has 2-port data. The other 5 buttons stay available
    // either way: a 2-port measurement's S11 slice is still legitimate
    // 1-port data, so those aren't disabled just because S2P is now also
    // an option.
    ui->s2pRiBtn->setVisible(isTwoPort);
    ui->label_6->setVisible(isTwoPort);
    ui->s2pMaBtn->setVisible(isTwoPort);
    ui->label_7->setVisible(isTwoPort);
    ui->s2pDbBtn->setVisible(isTwoPort);
    ui->label_9->setVisible(isTwoPort);
}

QString Export::suggestedPath(const QString &ext) const
{
    QString name = "Export";
    measurement* mm = m_measurements == nullptr ? nullptr
        : m_measurements->getMeasurement(m_measurements->getMeasurementLength() - 1 - m_measureNumber);
    if (mm != nullptr) {
        QString suggestedName = mm->name;
        int namePos = suggestedName.indexOf("> ");
        if (namePos != -1)
            suggestedName = suggestedName.mid(namePos+2);
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
void Export::on_asdBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Save"), suggestedPath("asd"), "AntScopeZ (*.asd)");
        if(!path.isEmpty())
        {
            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->saveData(m_measureNumber, path);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_csvBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("csv"), "Comma Separated Values (*.csv)");
        if(!path.isEmpty())
        {
            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 0, m_measureNumber, m_bApplyCable);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_nwlBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("nwl"), "APAK-EL (*.nwl)");

        if(!path.isEmpty())
        {
            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 0, m_measureNumber, m_bApplyCable);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_zRiBtn_clicked()
{
        qInfo() << "Touchstone button clicked";
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s1p"), "Touchstone (*.s1p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s1p", Qt::CaseInsensitive))
                path += ".s1p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 0, m_measureNumber,
                                       m_bApplyCable, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_sRiBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s1p"), "Touchstone (*.s1p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s1p", Qt::CaseInsensitive))
                path += ".s1p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 1, m_measureNumber, m_bApplyCable, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_sMaBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s1p"), "Touchstone (*.s1p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s1p", Qt::CaseInsensitive))
                path += ".s1p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 2, m_measureNumber, m_bApplyCable, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_sDbBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s1p"), "Touchstone (*.s1p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s1p", Qt::CaseInsensitive))
                path += ".s1p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 3, m_measureNumber, m_bApplyCable, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_s2pRiBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s2p"), "Touchstone 2-port (*.s2p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s2p", Qt::CaseInsensitive))
                path += ".s2p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportSParamData(path, 0, m_measureNumber, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_s2pMaBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s2p"), "Touchstone 2-port (*.s2p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s2p", Qt::CaseInsensitive))
                path += ".s2p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportSParamData(path, 1, m_measureNumber, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}

void Export::on_s2pDbBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("s2p"), "Touchstone 2-port (*.s2p)");

        if(!path.isEmpty())
        {
            if (!path.endsWith(".s2p", Qt::CaseInsensitive))
                path += ".s2p";

            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportSParamData(path, 2, m_measureNumber, m_description);
            m_measurements->clearDirty(m_measureNumber); // see measurement::dirty's own comment
        }
    }
}
