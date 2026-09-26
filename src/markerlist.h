#ifndef MARKERLIST_H
#define MARKERLIST_H

#include <QVector>

// One marker, UI-free. The desktop's `marker` (markers.h) adds its chart
// lines/labels on top.
struct MarkerData
{
    double frequency = 0; // kHz, as the charts' x axis
};

// The markers, in display order, plus the rules around them. T is
// MarkerData or something built on it; owns its elements.
template <class T>
class MarkerList : public QVector<T*>
{
public:
    // Markers are shown numbered from 1.
    static int number(int index) { return index + 1; }

    bool canAdd(int maxCount) const { return this->size() < maxCount; }

    // Auto-placing a marker at the lowest SWR after a scan: only when the
    // setting is on and there's room.
    bool wantsAutoMarker(bool enabled, int maxCount) const { return enabled && canAdd(maxCount); }

    // Detaches and returns the marker at index; the caller deletes it
    // after any UI cleanup.
    T* take(int index)
    {
        T* m = this->at(index);
        this->remove(index, 1);
        return m;
    }
};

#endif // MARKERLIST_H
