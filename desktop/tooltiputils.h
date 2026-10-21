#ifndef TOOLTIPUTILS_H
#define TOOLTIPUTILS_H

#include <QWidget>

// Plain-text Qt tooltips don't word-wrap -- a long one just renders as a
// single unbounded line, which can run off-screen. Fixing that by inserting
// "\n" (or an HTML <br>) into the *source* string passed to tr() would
// orphan every existing translation of that string in every locale's .ts
// file, for no reason: lupdate matches on the literal source text, and
// wrapping is a presentation choice, not part of the string's meaning.
//
// ToolTipUtils wraps *after* tr() returns instead -- the already-resolved
// runtime string, for whatever locale is active -- so no .ts file is ever
// touched by this. Qt's QToolTip renders width-constrained HTML through
// QTextDocument, which reflows correctly per-language regardless of word
// length or script (English vs. German compound words vs. CJK), unlike a
// fixed-character-count wrap would.
class ToolTipUtils
{
public:
    // Walks every descendant of root and rewraps any plain-text tooltip
    // longer than lengthThreshold into a width-constrained HTML table, so
    // it reflows instead of running off-screen. Skips anything that
    // already looks like HTML (starts with '<'), so it's safe to call more
    // than once, or after a child widget has already set its own rich-text
    // tooltip. Call once, at the end of a widget's own constructor, after
    // setupUi()/setToolTip() calls are already done.
    //
    // Only reaches tooltips that already exist at that point, though -- a
    // tooltip that gets set or replaced later at runtime (e.g. a status
    // label whose tooltip is rebuilt every time some value changes) needs
    // wrapToolTip() below instead, called at the point it's actually set.
    static void wrapLongToolTips(QWidget *root, int lengthThreshold = 50, int widthPx = 300);

    // The single-string version of the same wrap, for a tooltip set
    // dynamically at runtime rather than once at construction time.
    // Returns plainText unchanged if it's short enough or already HTML, so
    // it's safe to call unconditionally rather than checking length first.
    static QString wrapToolTip(const QString &plainText, int lengthThreshold = 50, int widthPx = 300);
};

#endif // TOOLTIPUTILS_H
