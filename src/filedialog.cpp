#include "filedialog.h"
#include "apppaths.h"
#include <QFileInfo>

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
    // `dir` may be a plain directory, or (less commonly for Open, but
    // handled the same way as getSaveFileName() below for consistency) a
    // directory plus a filename to preselect. QFileDialog's own static
    // convenience functions split those apart automatically; the instance
    // API used here (needed for the shared styling applied below) does
    // not -- setDirectory() alone silently fails to navigate anywhere if
    // given a path that isn't literally an existing directory.
    QFileInfo fi(dir);
    if (fi.isDir()) {
        dlg.setDirectory(dir);
    } else {
        dlg.setDirectory(fi.path());
        dlg.selectFile(fi.fileName());
    }
    dlg.setNameFilter(filter);

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

    // `dir` is normally a directory plus a suggested filename (e.g.
    // FileDialog::userDataDir() + "/" + suggestedName + ".ext"). Passing
    // that whole string straight to setDirectory() -- as this used to do
    // -- doesn't work: setDirectory() expects an actual existing
    // directory, and since the full path (with its nonexistent suggested
    // filename on the end) never literally exists on disk, it silently
    // fails to navigate there at all, leaving the dialog wherever Qt's own
    // fallback happens to be. That's the confirmed cause of every
    // Save-type dialog in the app (Export, Print, Screenshot, .asd Save,
    // main-window screenshot capture) not actually defaulting to
    // UserDataDir despite it being passed in correctly. Split directory
    // and filename apart explicitly instead (isDir() also covers the case
    // of a caller passing a bare directory with nothing to preselect).
    QFileInfo fi(dir);
    if (fi.isDir()) {
        dlg.setDirectory(dir);
    } else {
        dlg.setDirectory(fi.path());
        dlg.selectFile(fi.fileName());
    }
    dlg.setNameFilter(filter);

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
    dlg.setWindowTitle(caption);
    dlg.setDirectory(dir);
    // Never actually set to Directory mode before -- despite the name,
    // this behaved as a generic file-open picker (Open/Cancel buttons,
    // files selectable, a click on a folder navigated into it instead of
    // choosing it). setOptions() below replaces the whole flags set
    // (not a merge with setOption() calls made before it), so both of
    // these need to go in via the same call as DontUseNativeDialog.
    dlg.setFileMode(QFileDialog::Directory);
    dlg.setOptions(options | QFileDialog::DontUseNativeDialog | QFileDialog::ShowDirsOnly);

    if (dlg.exec() == QDialog::Accepted) {
        name = dlg.selectedFiles().constFirst();
    }

    return name;
}

QString FileDialog::userDataDir()
{
    return AppPaths::userDataDir();
}

void FileDialog::setUserDataDir(const QString &dir)
{
    AppPaths::setUserDataDir(dir);
}

void FileDialog::noteUserDataDirIfEnabled(const QString &savedFilePath)
{
    if (savedFilePath.isEmpty() || !userDataDirFollowsSaves())
        return;

    setUserDataDir(QFileInfo(savedFilePath).path());
}

bool FileDialog::userDataDirFollowsSaves()
{
    return AppPaths::userDataDirFollowsSaves();
}

void FileDialog::setUserDataDirFollowsSaves(bool follows)
{
    AppPaths::setUserDataDirFollowsSaves(follows);
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
