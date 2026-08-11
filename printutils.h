#ifndef PRINTUTILS_H
#define PRINTUTILS_H

#include <QPageSize>

// Shared helpers for the app's various print/PDF-export paths (print.cpp,
// screenshot.cpp), so they don't each duplicate/diverge on the same
// decisions -- see BUILDINFO.md's PDF-output known issues for the history
// of why this exists.
class PrintUtils
{
public:
    // The page size to use when nothing more specific has been chosen:
    // queried from the actual default printer (straight from CUPS/the OS)
    // rather than hardcoded, so it reflects the real system default
    // instead of an assumption baked into this app. Falls back to Letter
    // only if no default printer/page size is available to ask.
    static QPageSize defaultPageSize();
};

#endif // PRINTUTILS_H
