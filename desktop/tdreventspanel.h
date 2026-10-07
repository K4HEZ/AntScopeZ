#ifndef TDREVENTSPANEL_H
#define TDREVENTSPANEL_H

#include <QWidget>
#include "measurements.h"

class QLabel;
class QTableWidget;

// TDR mode's counterpart of the markers table: reflections found in the
// latest TDR scan, by distance.
class TdrEventsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TdrEventsPanel(QWidget* parent = nullptr);

    // knownLength: entered cable length in the set's unit; 0 = none.
    void setEvents(const Measurements::TdrEventSet& set, double knownLength);

    // What the table shows, for printing.
    QStringList headerLabels() const;
    QList<QStringList> rowTexts() const;
    QVector<double> eventDistances() const { return m_distances; }

signals:
    void eventSelected(int index); // -1 = none
    void eventActivated(double distance); // double-click
    void removeUserMarker(int index);
    void clearUserMarkers();

private:
    QString toCsv() const;
    void saveCsv();

    QString m_scanName;
    QLabel* m_heading;
    QTableWidget* m_table;
    QVector<double> m_distances;
    QVector<int> m_userIndex;
};

#endif // TDREVENTSPANEL_H
