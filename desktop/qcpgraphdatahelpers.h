#ifndef QCPGRAPHDATAHELPERS_H
#define QCPGRAPHDATAHELPERS_H

#include "qcustomplot.h"

// QCustomPlot 1.x's QCPDataMap was a QMap<double, QCPData> keyed by
// frequency; ::value(key) did an exact-key lookup, returning a
// default-constructed (zero) QCPData on a miss. 2.x's QCPGraphDataContainer
// (QCPDataContainer<QCPGraphData>) is a sorted QVector with no key-indexed
// lookup at all -- only range queries (findBegin/findEnd) and positional
// access (at(int)). This app relies on the exact-match-or-zero behavior in
// ~120 places (mouse-hover crosshair value lookups in measurements_popups.cpp,
// marker frequency-interpolation in markers.cpp), so it needs a real
// replacement, not a mechanical rename -- see the qcustomplot-2x-upgrade
// investigation, 2026-08-25.
//
// findBegin(key, false) is QCPDataContainer's own std::lower_bound call
// (expandedRange=false skips the "back up one" step meant for range
// iteration) -- the same primitive QCPDataContainer::remove(double) uses
// internally for its own exact-key removal. Confirmed only ever called on
// frequency-keyed QCPGraphDataContainer fields in this app, never on the
// Smith-chart QCPCurveDataContainer ones (those are always iterated or
// handed whole to setData(), never looked up by key) -- no curve overload
// needed.
inline double graphValueAt(const QCPGraphDataContainer &data, double key)
{
    QCPGraphDataContainer::const_iterator it = data.findBegin(key, false);
    if (it != data.constEnd() && it->key == key)
        return it->value;
    return 0.0;
}

inline double graphValueAt(QSharedPointer<QCPGraphDataContainer> data, double key)
{
    return data ? graphValueAt(*data, key) : 0.0;
}

// Same exact-match-or-default idea as graphValueAt(), but for call sites
// that used QMap<double,QCPData>::value(key)'s *whole point* (both key and
// value), not just the value -- default-constructed QCPGraphData() on a
// miss, same as QMap::value()'s own miss behavior.
inline QCPGraphData graphDataAt(const QCPGraphDataContainer &data, double key)
{
    QCPGraphDataContainer::const_iterator it = data.findBegin(key, false);
    if (it != data.constEnd() && it->key == key)
        return *it;
    return QCPGraphData();
}

#endif // QCPGRAPHDATAHELPERS_H
