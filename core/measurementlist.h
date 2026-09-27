#ifndef MEASUREMENTLIST_H
#define MEASUREMENTLIST_H

#include <QList>
#include <QtGlobal>
#include "measurementdata.h"

// The app's list of measurements plus the scan life-cycle rules, UI-free.
// T is MeasurementData or something built on it (the desktop's
// `measurement` adds plot caches). It *is* a QList<T>, so plain list access
// keeps working; callers do their own UI work (plot series, table rows)
// around these calls.
template <class T>
class MeasurementList : public QList<T>
{
public:
    // Keep at most maxCount measurements: while this is true, the caller
    // removes row 0 (with whatever UI cleanup that needs) before startNew().
    bool needsEviction(int maxCount) const { return this->size() >= maxCount; }

    // Appends a fresh measurement with the next serial number.
    T& startNew(const QString& name)
    {
        m_interrupted = false;
        int serial = nextSerialNumber(); // before appending -- see its comment
        this->append(T());
        this->last().serialNumber = serial;
        this->last().name = name;
        return this->last();
    }

    // A real sweep (not a single-point reading) begins on the new last row.
    void startSweep(qint64 from, qint64 to, qint64 dots)
    {
        this->last().set(from, to, dots);
        m_inProgress = true;
    }

    // Continuous mode's next pass over the last row: points now replace the
    // previous pass's, in order, from the first one.
    void continueSweep(qint64 from, qint64 to, qint64 dots)
    {
        m_continuing = true;
        m_point = 0;
        this->last().set(from, to, dots);
    }

    void setContinuous(bool state) { m_continuing = state; m_point = 0; }
    void stopContinuing() { m_continuing = false; }
    bool isContinuing() const { return m_continuing; }

    // Esc/Stop. Several devices can't abort a sweep in flight and keep
    // sending points; accepting() turns them away until the next startNew().
    void interrupt() { m_interrupted = true; }
    void clearInterrupted() { m_interrupted = false; }
    bool isInterrupted() const { return m_interrupted; }
    bool accepting() const { return !m_interrupted && !this->isEmpty(); }

    bool inProgress() const { return m_inProgress; }
    void setInProgress(bool state) { m_inProgress = state; }

    // Index the next point lands at in the last row (restarts at 0 on each
    // Continuous pass, unlike dataRX.size()).
    int pointIndex() const { return m_point; }

    // Stores one point in the last row (see MeasurementData::addPoint()).
    // Doesn't advance pointIndex(); call nextPoint() once done with it.
    bool addPoint(const RawData& raw, double Z0, Calibration* calibration,
                  RawData* calibrated = nullptr)
    {
        return this->last().addPoint(raw, m_point, m_continuing, Z0, calibration, calibrated);
    }
    void nextPoint() { ++m_point; }

    // The scan is over. True if the last row ended with no points at all
    // (cancelled or failed before any reply) -- nothing to view, export or
    // compare, so the caller should remove it.
    bool complete()
    {
        m_inProgress = false;
        m_continuing = false;
        return !this->isEmpty() && this->last().dataRX.isEmpty();
    }

    // Rename marks the row changed since its last save.
    void rename(int row, const QString& name)
    {
        (*this)[row].name = name;
        (*this)[row].dirty = true;
    }
    void clearDirty(int row) { (*this)[row].dirty = false; }

    int nextSerialNumber() const
    {
        int next = 0;
        for (int idx = 0; idx < this->size(); idx++)
            next = qMax(next, this->at(idx).serialNumber);
        next++;
        // g_maxMeasurements caps concurrent measurements at 15 (the caller
        // evicts the oldest via needsEviction() before ever appending past
        // it), so by the time this wraps, every measurement that could
        // collide with 1..N has long since been evicted (confirmed safe at
        // that cap, Harold, 2026-09-17).
        if (next > 99)
            next = 1;
        return next;
    }

private:
    int m_point = 0;
    bool m_continuing = false;
    bool m_interrupted = false;
    bool m_inProgress = false;
};

#endif // MEASUREMENTLIST_H
