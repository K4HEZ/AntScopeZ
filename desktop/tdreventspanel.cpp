#include "tdreventspanel.h"
#include "filedialog.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QHeaderView>
#include <QRegularExpression>
#include <QTextStream>
#include <QMenu>
#include <algorithm>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

static QString kindName(TdrMath::EventKind kind)
{
    switch (kind) {
    case TdrMath::EventKind::OpenEnd: return QObject::tr("Open end");
    case TdrMath::EventKind::ShortEnd: return QObject::tr("Short end");
    case TdrMath::EventKind::HighZ: return QObject::tr("High-Z");
    case TdrMath::EventKind::LowZ: return QObject::tr("Low-Z");
    case TdrMath::EventKind::NearEndMismatch: return QObject::tr("Near-end mismatch");
    case TdrMath::EventKind::PossibleEcho: return QObject::tr("Possible echo");
    case TdrMath::EventKind::User: return QObject::tr("User");
    }
    return QString();
}

TdrEventsPanel::TdrEventsPanel(QWidget* parent) :
    QWidget(parent)
{
    m_heading = new QLabel(this);
    QFont bold = m_heading->font();
    bold.setBold(true);
    m_heading->setFont(bold);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(8);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = m_table->currentRow();
        bool selected = !m_table->selectedItems().isEmpty();
        emit eventSelected(selected && row >= 0 && row < m_distances.size() ? row : -1);
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < m_distances.size())
            emit eventActivated(m_distances.at(row));
    });

    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        int row = m_table->rowAt(pos.y());
        QMenu menu(this);
        if (row >= 0 && row < m_userIndex.size() && m_userIndex.at(row) >= 0) {
            int user = m_userIndex.at(row);
            menu.addAction(tr("Remove Marker"), this, [this, user]() { emit removeUserMarker(user); });
        }
        menu.addAction(tr("Copy as CSV"), this, [this]() { QApplication::clipboard()->setText(toCsv()); });
        menu.addAction(tr("Save as CSV..."), this, [this]() { saveCsv(); });
        menu.addSeparator();
        QAction* clear = menu.addAction(tr("Clear All User Markers"), this, [this]() { emit clearUserMarkers(); });
        clear->setEnabled(std::any_of(m_userIndex.begin(), m_userIndex.end(), [](int u) { return u >= 0; }));
        menu.exec(m_table->viewport()->mapToGlobal(pos));
    });

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_heading);
    layout->addWidget(m_table);

    setEvents(Measurements::TdrEventSet(), 0);
}

void TdrEventsPanel::setEvents(const Measurements::TdrEventSet& set, double knownLength)
{
    m_distances.clear();
    m_userIndex = set.userIndex;
    for (const TdrMath::Event& e : set.events)
        m_distances << e.distance;
    QString unit = set.metric ? QStringLiteral("m") : QStringLiteral("ft");
    m_table->setHorizontalHeaderLabels({
        tr("#"), tr("Distance, %1").arg(unit), tr("Time, ns"), tr("% length"),
        tr("Kind"), tr("Z, Ω"), tr("ρ"), tr("Note"),
    });

    if (!set.valid) {
        m_heading->setText(set.note.isEmpty() ? tr("TDR events") : set.note);
        m_table->setRowCount(0);
        return;
    }
    m_scanName = set.name;
    m_heading->setText(set.note.isEmpty() ? tr("TDR events: %1").arg(set.name)
                                          : tr("TDR events: %1 (%2)").arg(set.name, set.note));

    if (set.events.isEmpty()) {
        m_table->setRowCount(1);
        for (int c = 0; c < m_table->columnCount(); ++c)
            m_table->setItem(0, c, new QTableWidgetItem(QString()));
        m_table->item(0, 4)->setText(tr("No reflection"));
        m_table->item(0, 7)->setText(tr("Nothing above the noise floor"));
        m_table->resizeColumnsToContents();
        return;
    }

    m_table->setRowCount(set.events.size());
    for (int row = 0; row < set.events.size(); ++row) {
        const TdrMath::Event& e = set.events.at(row);
        QStringList notes;
        if (e.echoOf >= 0 && e.echoOf < set.events.size()) {
            double ns = TdrMath::roundTripNs(set.events.at(e.echoOf).distance, set.velFactor, set.metric);
            notes << tr("about twice marker @ %1 ns").arg(ns, 0, 'f', 1);
        }
        if (e.nearRangeEdge)
            notes << tr("near range edge");
        QString percent = knownLength > 0 ? QString::number(100.0 * e.distance / knownLength, 'f', 0) + "%"
                                          : QStringLiteral("--");
        const QStringList cells = {
            QString::number(row + 1),
            QString::number(e.distance, 'f', 2),
            QString::number(TdrMath::roundTripNs(e.distance, set.velFactor, set.metric), 'f', 1),
            percent,
            kindName(e.kind),
            QString::number(e.impedance, 'f', 0),
            QString::number(e.amplitude, 'f', 2),
            notes.join(QStringLiteral("; ")),
        };
        for (int c = 0; c < cells.size(); ++c)
            m_table->setItem(row, c, new QTableWidgetItem(cells.at(c)));
    }
    m_table->resizeColumnsToContents();
}

QString TdrEventsPanel::toCsv() const
{
    auto quote = [](QString s) {
        if (s.contains(QRegularExpression("[\",\n]")))
            return "\"" + s.replace("\"", "\"\"") + "\"";
        return s;
    };
    QStringList lines;
    QStringList fields;
    for (int c = 0; c < m_table->columnCount(); ++c)
        fields << quote(m_table->horizontalHeaderItem(c) ? m_table->horizontalHeaderItem(c)->text() : QString());
    lines << fields.join(',');
    for (int r = 0; r < m_table->rowCount(); ++r) {
        fields.clear();
        for (int c = 0; c < m_table->columnCount(); ++c)
            fields << quote(m_table->item(r, c) ? m_table->item(r, c)->text() : QString());
        lines << fields.join(',');
    }
    return lines.join('\n') + '\n';
}

void TdrEventsPanel::saveCsv()
{
    QString base = QStringLiteral("TDR events");
    if (!m_scanName.isEmpty())
        base += " - " + m_scanName;
    base.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    QString path = FileDialog::getSaveFileName(this, tr("Save"),
                       FileDialog::withExtension(FileDialog::userDataDir() + "/" + base, "csv"),
                       tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".csv", Qt::CaseInsensitive))
        path += ".csv";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << toCsv();
    FileDialog::noteUserDataDirIfEnabled(path);
}

QStringList TdrEventsPanel::headerLabels() const
{
    QStringList labels;
    for (int c = 0; c < m_table->columnCount(); ++c)
        labels << (m_table->horizontalHeaderItem(c) ? m_table->horizontalHeaderItem(c)->text() : QString());
    return labels;
}

QList<QStringList> TdrEventsPanel::rowTexts() const
{
    QList<QStringList> rows;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c)
            cells << (m_table->item(r, c) ? m_table->item(r, c)->text() : QString());
        rows << cells;
    }
    return rows;
}
