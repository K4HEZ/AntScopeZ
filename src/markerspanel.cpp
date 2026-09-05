#include "markerspanel.h"
#include "mainwindow.h"
#include "style.h"
#include <QCoreApplication>
#include <QHeaderView>
#include <QMenu>

QMap<int, QString> MarkersHeaderColumn::m_mapHeader;

MarkersPanel::MarkersPanel(QWidget *parent) : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->setVisible(false);
    // Tab/Backtab move focus out of the table instead of cycling between
    // cells -- same reasoning as Presets::setTable() (presets.cpp).
    m_table->setTabKeyNavigation(false);
    // Issue #36 -- right-click "Clear All" (removing every marker at once,
    // vs. clicking each row's own "X" button individually).
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &MarkersPanel::on_customContextMenuRequested);
    m_layout->addWidget(m_table);

    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);

    createHeader();
}

MarkersPanel::~MarkersPanel()
{
    delete m_settings;
}

void MarkersPanel::createHeader()
{
    QMap<int, QString>& mapHeader = MarkersHeaderColumn::headerMap();

    m_settings->beginGroup("Markers");
    QString buttons = m_settings->value("header", "0,1,2,3,4,5,6,7,8,9").toString();
    m_settings->endGroup();

    m_columnTypes.clear();
    for (const QString& key : buttons.split(',')) {
        if (key.isEmpty())
            continue;
        m_columnTypes << key.toInt();
    }

    m_table->setColumnCount(m_columnTypes.size());
    QStringList labels;
    for (int type : m_columnTypes)
        labels << mapHeader.value(type);
    m_table->setHorizontalHeaderLabels(labels);
    // Same as Presets::setTable() -- fits content, user-resizable.
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->resizeColumnsToContents();
}

void MarkersPanel::on_customContextMenuRequested(const QPoint& pos)
{
    if (m_markers == 0)
        return; // nothing to clear
    QMenu menu(this);
    QAction* clearAll = menu.addAction(tr("Clear All"));
    connect(clearAll, &QAction::triggered, this, &MarkersPanel::clearAllMarkers);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MarkersPanel::on_remove()
{
    QString str = sender()->objectName();
    str.remove(0, 2);
    int markerIndex = str.toInt();
    emit removeMarker(markerIndex);
}

void MarkersPanel::on_translate()
{
    // headerMap() lazily builds and caches its QMap forever, so switching
    // language at runtime otherwise never reaches it -- clear it so the
    // next headerMap() call (from createHeader() below) rebuilds it in the
    // new language, then actually rebuild the header labels.
    MarkersHeaderColumn::m_mapHeader.clear();
    createHeader();
}

void MarkersPanel::reloadColumns()
{
    createHeader();
    updateMarkers(m_markers, m_measurements, true);
    emit changeColumns();
}

QList<int> MarkersPanel::getColumns()
{
    return m_columnTypes;
}

void MarkersPanel::updateMarkers(int markers, int measurements, bool force)
{
    // on_measurementComplete() calls this on every scan tick, including
    // during a continuous scan. Rebuilding unconditionally destroys and
    // recreates every "X" remove button each time, which can delete the
    // button between a mouse press and release and silently eat the click.
    // Values are refreshed separately via updateInfo(), so skip the rebuild
    // when the table shape hasn't actually changed. Insert/remove-column
    // passes force=true since the table must be rebuilt to match the new
    // header size even though the marker/measurement counts are unchanged.
    if (!force && markers == m_markers && measurements == m_measurements) {
        return;
    }

    clearTable();

    m_markers = markers;
    m_measurements = measurements;

    if (markers == 0 || m_columnTypes.isEmpty()) {
        return;
    }

    int rowCount = m_measurements == 0 ? 1 : m_measurements;
    m_table->setRowCount(m_markers * rowCount);

    int row = 0;
    for (int i = 0; i < m_markers; i++) {
        QToolButton* button = new QToolButton(m_table);
        QString str = "RM" + QString::number(i);
        button->setObjectName(str);
        button->setMaximumWidth(20);
        button->setText("X");
        connect(button, &QToolButton::clicked, this, &MarkersPanel::on_remove);
        m_table->setCellWidget(row, 0, button);
        // One delete button per marker, spanning the whole block of
        // measurement rows for that marker -- matches the floating popup's
        // layout (a single button anchored to the marker, not repeated per
        // measurement row).
        if (rowCount > 1)
            m_table->setSpan(row, 0, rowCount, 1);

        for (int j = 0; j < rowCount; j++) {
            for (int k = 1; k < m_columnTypes.size(); k++) {
                QTableWidgetItem* item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                m_table->setItem(row, k, item);
            }
            row++;
        }
    }
}

void MarkersPanel::updateInfo(QList<QList<QVariant>>& info)
{
    // Mirrors updateMarkers()'s own rowCount fallback: with no measurements
    // yet, the table is still built with one (blank) row per marker rather
    // than zero, so info -- which now supplies one row per marker in that
    // case too (see Markers::updateInfo()) -- has to be walked the same way
    // or every cell it fills in gets silently skipped.
    int rowCount = m_measurements == 0 ? 1 : m_measurements;
    if (info.size() != (m_markers * rowCount))
        return;

    int row = 0;
    for (int i = 0; i < m_markers; i++) {
        for (int j = 0; j < rowCount; j++) {
            const QList<QVariant>& rowInfo = info[row];
            for (int k = 1; k < m_columnTypes.size(); k++) { // ignore fieldDelete
                if (j != 0 && k == MarkersHeaderColumn::fieldNum)
                    continue;
                QVariant val = rowInfo[k];
                QString str = formatText(m_columnTypes[k], val);
                QTableWidgetItem* item = m_table->item(row, k);
                if (item)
                    item->setText(str);
            }
            row++;
        }
    }
}

void MarkersPanel::clearTable(void)
{
    // Destroys every item and cell widget (the "X" delete buttons
    // included) and rebuilds from zero -- simpler and safer than picking
    // through what changed, and updateMarkers() always repopulates
    // immediately after calling this.
    m_table->setRowCount(0);
}

QString MarkersPanel::formatText(int type, QVariant v)
{
    if (!v.isValid() || v.toDouble() == DBL_MAX)
        return "";

    QString str;
    switch (type) {
    case MarkersHeaderColumn::fieldDelete:
        break;
    case MarkersHeaderColumn::fieldNum:
        str = QString::number(v.toInt());
        break;
    case MarkersHeaderColumn::fieldSerie:
        str = QString::number(v.toInt());
        break;
    case MarkersHeaderColumn::fieldZ:
        str = v.toString();
        break;
    case MarkersHeaderColumn::fieldZpar:
        str = v.toString();
        break;
    default:
        str = QString::number(v.toDouble(),'f', 2);
        break;
    }
    return str;
}

///////////////////////////////////////////////////////
QMap<int, QString>& MarkersHeaderColumn::headerMap()
{
    if (m_mapHeader.isEmpty()) {
        // QCoreApplication::translate(), not tr() -- this is a plain struct,
        // not a QObject, so tr() (which needs a metaobject/className to key
        // the lookup) isn't available here. "MarkersHeaderColumn" is the
        // context lupdate/QTranslator index this under, same role tr()'s
        // enclosing class name would normally play. Previously plain
        // literals, so these never got picked up by lupdate at all --
        // always showed English regardless of locale.
        int i = MarkersHeaderColumn::fieldDelete;
        // "x" rather than "Del" -- universally understood as a delete/close
        // glyph without needing translation at all (user's call, 2026-08-16).
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "x"));
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "Marker"));
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", " # "));
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "FQ, kHz"));
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "SWR"));        // SWR - standing wave ratio
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "RL, dB"));     // RL - return loss
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "Phase°"));     // phase - phase
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "R, Ohm"));     // R - resistance (series model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "X, Ohm"));     // X - reactance (series model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "Z, Ohm"));     // Z - impedance
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "L, nH"));      // L - inductance (series model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "C, pF"));      // C - capacitance (series model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "rho"));        // rho - magnitude
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "|Z|, Ohm"));   // |Z| - impedance modulus
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "R||, Ohm"));   // R|| - resistance (parallel model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "X||, Ohm"));   // X|| - reactance (parallel model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "Z||, Ohm"));   // Z|| - impedance (parallel model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "L||, nH"));    // L|| - inductance (parallel model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "C||, pF"));    // C|| - capacitance (parallel model)
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "S21, dB"));    // S21 - forward transmission magnitude
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "S21 Phase°"));
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "S12, dB"));    // S12 - reverse transmission magnitude
        m_mapHeader.insert(i++, QCoreApplication::translate("MarkersHeaderColumn", "S12 Phase°"));
    }
    return m_mapHeader;
}
