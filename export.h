#ifndef EXPORT_H
#define EXPORT_H

#include <QDialog>
#include <measurements.h>
#include <analyzer/analyzerparameters.h>
#include <QSettings>
#include <settings.h>

namespace Ui {
class Export;
}

class Export : public QDialog
{
    Q_OBJECT

public:
    explicit Export(QWidget *parent = 0);
    ~Export();

    void setMeasurements(Measurements * _measurements, quint32 number,
                         bool _applyCable=false, QString _description=QString());

private:
    Ui::Export *ui;
    Measurements * m_measurements;
    QSettings * m_settings;

    quint32 m_measureNumber;
    bool m_bApplyCable = false;
    QString m_description;

    // Suggested save path for the selected measurement with the given
    // extension (no leading dot) -- FileDialog::userDataDir() plus a
    // filename derived from the measurement's own display name, same
    // "strip auto-numbering prefix, sanitize filesystem-unsafe characters"
    // treatment as MainWindow's .asd Save (mainwindow_measurements_io.cpp).
    // Falls back to "Export.<ext>" if the measurement is gone or its name
    // sanitizes to nothing.
    //
    // Deliberately does NOT encode which Touchstone variant (Z,RI/S,RI/
    // S,MA) was picked into the filename -- all three suggest the same
    // name. The file's own header line ("# MHz S RI R 50" etc., written
    // by Measurements::exportData()) is what actually describes its
    // format, and Measurements::importData() reads that back on import
    // regardless of filename, so there's nothing for the filename to lose
    // by not tagging it -- discussed and confirmed 2026-08-15.
    QString suggestedPath(const QString &ext) const;

private slots:
    void on_csvBtn_clicked();
    void on_zRiBtn_clicked();
    void on_sRiBtn_clicked();
    void on_sMaBtn_clicked();
    void on_nwlBtn_clicked();
};

#endif // EXPORT_H
