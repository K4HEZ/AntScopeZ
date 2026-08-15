#include "export.h"
#include "ui_export.h"
#include "style.h"
#include "filedialog.h"
#include <QRegularExpression>

Export::Export(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Export)
{
    ui->setupUi(this);
    adjustSize();
    QString style;
    style = Style::dialog();
    style += Style::pushButton();
    setStyleSheet(style);

    style = Style::groupBox();
    //ui->groupBox->setStyleSheet(style);

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

void Export::on_csvBtn_clicked()
{
    if(m_measurements != NULL)
    {
        QString path = FileDialog::getSaveFileName(this, tr("Export"), suggestedPath("csv"), "Comma Separated Values (*.csv)");
        if(!path.isEmpty())
        {
            FileDialog::noteUserDataDirIfEnabled(path);
            m_measurements->exportData(path, 0, m_measureNumber, m_bApplyCable);
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
        }
    }
}
