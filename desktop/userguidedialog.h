#ifndef USERGUIDEDIALOG_H
#define USERGUIDEDIALOG_H

#include <QDialog>
#include <QUrl>

class QTextBrowser;

// Help > User Guide -- a lightweight (QTextBrowser, no HTML rendering
// engine) non-modal viewer for docs/user-guide.md, with the doc's own
// self-links (#some-heading) actually working. Qt's markdown importer
// (QTextDocument::setMarkdown()) turns those into real hyperlinks on its
// own, but doesn't generate the matching named anchor at the heading
// itself -- confirmed empirically (a plain setMarkdown() heading comes
// back with zero anchors) -- so tagHeadingAnchors() does that part by
// hand, computing the same GitHub-style slug the guide's own links
// already assume (verified against several of the guide's real,
// non-trivial link targets, e.g. "Import / Export" -> "import--export").
class UserGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserGuideDialog(QWidget *parent = nullptr);

private slots:
    void onAnchorClicked(const QUrl& link);

private:
    void loadGuide();
    void tagHeadingAnchors();
    static QString slugify(const QString& headingText);

    QTextBrowser* m_browser;
    // Directory user-guide.md was actually loaded from -- lets a relative
    // link elsewhere in the doc (e.g. "../BUILDINFO.md") resolve to a real
    // local path before being handed to QDesktopServices::openUrl(),
    // instead of a bare relative URL that wouldn't resolve to anything.
    QString m_guideDir;
};

#endif // USERGUIDEDIALOG_H
