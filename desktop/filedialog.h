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

    // Shared "where the user keeps their AntScopeZ files" folder (Settings
    // -> General's "Data folder" field), backed by [General]/UserDataDir in
    // the ini. Independent of any one dialog -- replaces the previous mess
    // of every save/export/screenshot dialog keeping its own remembered
    // last-path setting (5 separate ini keys that didn't agree with each
    // other and weren't actually used to default the dialogs' starting
    // folder). Callers that want a Save/Open dialog to default here build
    // their suggested path from this, e.g. userDataDir() + "/" +
    // suggestedName. Lazily loads from the ini on first call, then caches
    // for the rest of the process; the first time the app has ever run (no
    // ini value yet), defaults to QStandardPaths::DocumentsLocation +
    // "/AntScopeZ" and creates it.
    static QString userDataDir();

    // Explicitly (re)points UserDataDir -- e.g. Settings' "Browse..."
    // button. Always persists immediately, regardless of the "follows
    // saves" setting below.
    static void setUserDataDir(const QString &dir);

    // Call after a Save-type dialog is *accepted* (never on cancel) with
    // the full path the user chose. No-op unless
    // [General]/UserDataDirFollowsSaves is on (Settings' "Save actions
    // update this folder" checkbox). Open/Import call sites should not call
    // this -- browsing to open a one-off file from somewhere else shouldn't
    // relocate where new saves land.
    static void noteUserDataDirIfEnabled(const QString &savedFilePath);

    // Backing accessors for Settings' "Save actions update this folder"
    // checkbox -- [General]/UserDataDirFollowsSaves, default off (a folder
    // set in Settings, or left at the default, should stay put unless the
    // user opts in to it following where they save).
    static bool userDataDirFollowsSaves();
    static void setUserDataDirFollowsSaves(bool follows);

    // Returns path with its extension replaced by ext (no leading dot,
    // e.g. "csv" not ".csv"). Use this rather than string-concatenating
    // "." + ext directly whenever the base name might already have an
    // extension on it -- e.g. derived from a measurement/printout name
    // that could itself contain dots (dates, decimals, ...), or from a
    // name that's already been round-tripped through a save once. Strips
    // every trailing occurrence of "." + ext, not just one, so an
    // already-doubled name (e.g. "foo.asd.asd") self-heals in one call
    // instead of accumulating further (issue reported 2026-08-14: names
    // like "07_27_37 12.08.2026.asd" -- note the date's own dots -- kept
    // growing an extra ".asd" every save, because the naive version of
    // this used QFileInfo::baseName(), which truncates at the *first*
    // dot in the name, not the last, so it never fully stripped a
    // trailing extension sitting after other dots).
    static QString withExtension(const QString &path, const QString &ext);

signals:
};

#endif // FILEDIALOG_H
