#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QObject>
#include <QFileDialog>

class FileDialog : public QObject
{
    Q_OBJECT
public:
    explicit FileDialog(QObject *parent = nullptr);
    static QString getOpenFileName(QWidget *parent = nullptr,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   const QString &filter = QString(),
                                   QString *selectedFilter = nullptr);
    static QString getSaveFileName(QWidget *parent = nullptr,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   const QString &filter = QString(),
                                   QString *selectedFilter = nullptr);
    static QString getExistingDirectory(QWidget *parent = nullptr,
                                        const QString &caption = QString(),
                                        const QString &dir = QString(),
                                        QFileDialog::Options options = QFileDialog::ShowDirsOnly);

    // Returns path with its extension replaced by ext (no leading dot,
    // e.g. "csv" not ".csv"). Used to build a suggested save-dialog name
    // from a remembered last-path that may have a different (or, if it
    // came from a previously-corrupted setting, a repeated) extension.
    // Strips every trailing occurrence of "." + ext, not just one, so an
    // already-doubled name (e.g. "foo.s1p.s1p") self-heals in one call
    // instead of the old indexOf('.')+remove(...,4) pattern, which only
    // ever stripped one 4-character chunk starting at the *first* dot in
    // the whole path and could perpetuate/worsen existing corruption.
    static QString withExtension(const QString &path, const QString &ext);
signals:
};

#endif // FILEDIALOG_H
