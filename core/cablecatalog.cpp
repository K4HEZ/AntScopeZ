#include "cablecatalog.h"
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

QList<CableSpec> CableCatalog::load(const QString& path)
{
    QList<CableSpec> list;

    auto ideal = [](const QString& name, double r0) {
        CableSpec c;
        c.name = name;
        c.r0 = r0;
        c.velocityFactor = 0.66;
        return c;
    };
    // Same 4 built-ins Settings::openCablesFile() starts with.
    list << ideal(QCoreApplication::translate("CableCatalog", "Ideal 50-Ohm cable"), 50.0);
    list << ideal(QCoreApplication::translate("CableCatalog", "Ideal 75-Ohm cable"), 75.0);
    list << ideal(QCoreApplication::translate("CableCatalog", "Ideal 25-Ohm cable"), 25.0);
    list << ideal(QCoreApplication::translate("CableCatalog", "Ideal 37.5-Ohm cable"), 37.5);

    if (path.isEmpty())
        return list;

    QFile file(path);
    if (!file.open(QFile::ReadOnly))
        return list;

    QTextStream in(&file);
    QString line;
    do {
        line = in.readLine();
        if (line.isEmpty() || line.at(0) == ';')
            continue;
        // Fields: name, R0, velocity factor, conductive loss, dielectric
        // loss, loss units, frequency the loss figures are specified at
        // (or 0) -- see cables.txt's own header. Only the first 3 matter
        // here; still require all 7 so a malformed line is skipped the
        // same way Settings::openCablesFile() skips it, not misread.
        QStringList fields = line.split(',');
        if (fields.length() != 7)
            continue;
        CableSpec c;
        c.name = fields.at(0);
        c.r0 = fields.at(1).toDouble();
        c.velocityFactor = fields.at(2).toDouble();
        list << c;
    } while (!line.isNull());

    return list;
}

QStringList CableCatalog::filterNames(const QStringList& names, const QString& filter)
{
    const QStringList words = filter.split(QChar(' '), Qt::SkipEmptyParts);
    QStringList out;
    for (const QString& n : names) {
        bool all = true;
        for (const QString& w : words)
            all = all && n.contains(w, Qt::CaseInsensitive);
        if (all)
            out << n;
    }
    return out;
}

void CableCatalog::pushRecent(QStringList& recents, const QString& name, int maxRecent)
{
    recents.removeAll(name);
    recents.prepend(name);
    while (recents.size() > maxRecent)
        recents.removeLast();
}

QStringList CableCatalog::pruneRecents(const QStringList& recents, const QStringList& names, int maxRecent)
{
    QStringList out;
    for (const QString& n : recents)
        if (names.contains(n) && out.size() < maxRecent)
            out << n;
    return out;
}
