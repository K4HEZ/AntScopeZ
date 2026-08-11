#include "filedialog.h"
#include "style.h"

FileDialog::FileDialog(QObject *parent)
    : QObject{parent}
{}

QString FileDialog::getOpenFileName(QWidget *parent,
                               const QString &caption,
                               const QString &dir,
                               const QString &filter,
                               QString *selectedFilter)
{
    QString name;
    QFileDialog dlg(parent);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setWindowTitle(caption);
    dlg.setDirectory(dir);
    dlg.setNameFilter(filter);
    QString style;
    style += Style::dialog();
    style += Style::pushButton();
    dlg.setStyleSheet(style);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    Q_UNUSED(selectedFilter);
    return name;
}

QString FileDialog::getSaveFileName(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    const QString &filter,
                                    QString *selectedFilter)
{
    QString name;
    QFileDialog dlg(parent);

    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setWindowTitle(caption);
    dlg.setDirectory(dir);
    dlg.setNameFilter(filter);
    dlg.selectFile(QFileInfo(dir).fileName());

    QString style;
    style += Style::dialog();
    style += Style::pushButton();
    dlg.setStyleSheet(style);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    Q_UNUSED(selectedFilter);
    return name;
}

QString FileDialog::getExistingDirectory(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    QFileDialog::Options options)
{
    QString name;
    QFileDialog dlg(parent);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setWindowTitle(caption);
    dlg.setDirectory(dir);
    dlg.setOptions(options);

    QString style;
    style += Style::dialog();
    style += Style::pushButton();
    dlg.setStyleSheet(style);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    return name;
}

QString FileDialog::withExtension(const QString &path, const QString &ext)
{
    if (path.isEmpty())
        return path;

    QFileInfo fi(path);
    QString dir = fi.path();
    QString base = fi.completeBaseName();
    while (base.endsWith("." + ext, Qt::CaseInsensitive))
        base.chop(ext.length() + 1);

    QString result = (dir.isEmpty() || dir == ".") ? base : dir + "/" + base;
    return result + "." + ext;
}
