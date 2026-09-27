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

    void setMeasurements(Measurements * _measurements, quint32 number);

private:
    Ui::Export *ui;
    Measurements * m_measurements;
    QSettings * m_settings;

    quint32 m_measureNumber;

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

    // Fills detailsLabel (name/points/type) from the selected measurement
    // and the format list (2-port formats only for 2-port data) -- called once from
    // setMeasurements(), since that's the only place m_measureNumber is
    // ever set. Resolves the measurement the same "count back from the
    // newest" way suggestedPath() already does, so the details shown
    // always describe the same measurement a click would actually export.
    void updateDetails();
    Corrections m_corrections; // the measurement's, for the notes

    // One entry per format in formatCombo.
    enum class Kind { Asd, OnePort, TwoPort };
    struct Format {
        QString id;          // persisted in the ini
        QString label;
        QString ext;         // no leading dot
        QString filter;      // file dialog filter
        QString description;
        Kind kind;
        int type;            // exportData()/exportSParamData() type
    };
    QList<Format> m_formats;
    void buildFormats(bool twoPort);
    const Format* currentFormat() const;

private slots:
    void onFormatChanged();
    void onSave();
};

#endif // EXPORT_H
