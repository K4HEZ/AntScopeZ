#include "userguidedialog.h"
#include "settings.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QTextBlock>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QSet>

UserGuideDialog::UserGuideDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("User Guide"));
    resize(900, 700);

    m_browser = new QTextBrowser(this);
    // We handle every click ourselves (see onAnchorClicked()) rather than
    // letting QTextBrowser follow links on its own -- it has no reason to
    // know the difference between "jump to a heading in this same
    // document" and "this points somewhere else entirely".
    m_browser->setOpenLinks(false);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &UserGuideDialog::onAnchorClicked);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_browser);

    loadGuide();
}

void UserGuideDialog::loadGuide()
{
    QString path = Settings::programDataPath("user-guide.md");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_browser->setPlainText(tr("Couldn't find the user guide.\n\nExpected it at:\n%1").arg(path));
        return;
    }
    m_guideDir = QFileInfo(path).absolutePath();

    QString text = QString::fromUtf8(file.readAll());
    // GitHub dialect (the default) is what the doc is actually written
    // against -- tables, and the YAML front matter at the top (Jekyll's
    // layout/title block) gets parsed out into metaInformation() instead
    // of rendered as literal text, confirmed empirically rather than
    // assumed.
    m_browser->setMarkdown(text);
    tagHeadingAnchors();
}

// The guide's own self-links ([Text](#some-heading)) already come through
// setMarkdown() as real, clickable hyperlinks -- Qt's markdown importer
// does that part on its own. What it does *not* do is generate a matching
// named anchor at the heading itself for scrollToAnchor() to find --
// confirmed empirically: a heading block comes back from setMarkdown()
// with zero anchors of its own, unlike a real HTML <h2 id="..."> would
// have. This walks every heading and tags it with the same GitHub-style
// slug the guide's own links already assume, so clicking one actually
// goes somewhere.
void UserGuideDialog::tagHeadingAnchors()
{
    QTextDocument* doc = m_browser->document();
    QSet<QString> usedSlugs;

    for (QTextBlock block = doc->begin(); block != doc->end(); block = block.next()) {
        if (block.blockFormat().headingLevel() <= 0)
            continue;

        QString slug = slugify(block.text());
        if (slug.isEmpty())
            continue;

        // GitHub itself disambiguates a repeated heading with -1/-2/...
        // suffixes -- not currently needed (no duplicate headings in this
        // guide), but cheap enough to get right rather than silently
        // mis-tag one if that ever changes.
        QString uniqueSlug = slug;
        for (int suffix = 1; usedSlugs.contains(uniqueSlug); suffix++)
            uniqueSlug = QString("%1-%2").arg(slug).arg(suffix);
        usedSlugs.insert(uniqueSlug);

        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        format.setAnchor(true);
        format.setAnchorNames({uniqueSlug});
        cursor.mergeCharFormat(format);
    }
}

// GitHub's own heading-slug algorithm: lowercase, drop anything that isn't
// a letter/digit/space/hyphen, then turn *each* remaining whitespace
// character into its own hyphen -- not a collapsed run. That distinction
// actually matters: verified against this guide's own real link targets,
// e.g. the heading "Import / Export" (the "/" vanishes, leaving two
// adjacent spaces) links as "#import--export", a genuine double hyphen,
// not "#import-export".
QString UserGuideDialog::slugify(const QString& headingText)
{
    QString s = headingText.toLower();
    s.remove(QRegularExpression("[^a-z0-9\\s-]"));
    s.replace(QRegularExpression("\\s"), "-");
    return s;
}

void UserGuideDialog::onAnchorClicked(const QUrl& link)
{
    // A same-document "#fragment" link -- no scheme, no path, just a
    // fragment -- is exactly the self-link case tagHeadingAnchors() exists
    // for.
    if (link.scheme().isEmpty() && link.path().isEmpty() && link.hasFragment()) {
        m_browser->scrollToAnchor(link.fragment());
        return;
    }

    // Anything else -- an external http(s) link, or a relative link to a
    // different local file (e.g. "../BUILDINFO.md") -- goes to the system
    // handler instead of trying to grow this into a multi-document
    // browser. A relative link needs resolving against the guide's own
    // directory first, or it's just a meaningless bare relative URL by
    // itself.
    QUrl resolved = link;
    if (link.isRelative() && !m_guideDir.isEmpty())
        resolved = QUrl::fromLocalFile(m_guideDir + "/").resolved(link);
    QDesktopServices::openUrl(resolved);
}
