#include "markerspopup.h"
#include "mainwindow.h"
#include "style.h"
#include <QCoreApplication>


QMap<int, QString> MarkersHeaderColumn::m_mapHeader;

MarkersPopUp::MarkersPopUp(QWidget *parent) : QWidget(parent),
      m_durability(2000),
      m_hiding(true),
      m_x(0),
      m_y(0),
      m_biasX(0),
      m_biasY(0),
      m_mainX(0),
      m_mainY(0),
      m_mainBiasX(0),
      m_mainBiasY(0),
      m_bgColor(0,0,0,180),
      m_penColor(255,255,255,180),
      m_textColor("white")
{
      setWindowFlags(Qt::FramelessWindowHint |        // Disable window decoration
                     Qt::Tool);                       // Don't show it as a separate window
      setAttribute(Qt::WA_TranslucentBackground);     // Make the background transparent
      // WA_ShowWithoutActivating kept this window from ever becoming the active
      // window, which on this window manager also meant it never received mouse
      // input at all -- clicks on "X" (or anywhere in the popup) were silently
      // dropped. Letting it activate normally fixes click delivery; the
      // resulting false "main window lost focus" is handled in
      // MainWindow::event() instead.

      animation.setTargetObject(this);                // Set the animation's target object
      animation.setPropertyName("popupOpacity");      // Set the animated property
      connect(&animation, &QAbstractAnimation::finished, this, &MarkersPopUp::hide); // Connect the
                                                        // animation-finished signal to the hide slot
      QString path = Settings::setIniFile();
      m_settings = new QSettings(path,QSettings::IniFormat);

      m_timer = new QTimer();
      connect(m_timer, &QTimer::timeout, this, &MarkersPopUp::hideAnimation);

      initLayout();
}

void MarkersPopUp::setName(QString name)
{
    m_name = name;
    m_settings->beginGroup(m_name);
    if(m_name == "Markers")
    {
        m_x = m_settings->value("x",861).toInt();
        m_y = m_settings->value("y",127).toInt();
        m_mainX = m_settings->value("mainX",169).toInt();
        m_mainY = m_settings->value("mainY",101).toInt();
        m_mainBiasX = m_settings->value("mainBiasX",692).toInt();
        m_mainBiasY = m_settings->value("mainBiasY",26).toInt();
    }
//    QWidget* widget = parentWidget() != nullptr ? parentWidget() : qApp->activeWindow();
//    QPoint pt = widget->mapToGlobal(widget->rect().center());
//    QScreen* pScreen = QGuiApplication::screenAt(pt);
//    QRect availableScreenSize = pScreen->availableGeometry();
    int widthDesc = MainWindow::m_mainWindow->width();
    int heightDesc = MainWindow::m_mainWindow->height();
    if((m_x > widthDesc - width()) || (m_x < 0))
    {
        m_x = 500;
    }
    if( (m_y > heightDesc - height()) || (m_y < 0))
    {
        m_y = 500;
    }

    m_settings->endGroup();

    setGeometry(m_x,m_y,width(),height());
}

MarkersPopUp::~MarkersPopUp()
{
    m_settings->beginGroup(m_name);
    m_settings->setValue("x",m_x);
    m_settings->setValue("y",m_y);
    m_settings->setValue("mainX",m_mainX);
    m_settings->setValue("mainY",m_mainY);
    m_settings->setValue("mainBiasX",m_mainBiasX);
    m_settings->setValue("mainBiasY",m_mainBiasY);
    m_settings->endGroup();

    delete m_settings;
}

void MarkersPopUp::initLayout()
{
      //fillHeaderMap();
      createHeader();
      setLayout(&m_layout);
      updateTable();
}

// Same inverse-of-chart-background approach as Measurements::
// inverseChartBackground() -- duplicated rather than shared since the two
// classes don't have a common base, but kept identical on purpose.
QColor MarkersPopUp::chartBackgroundColor()
{
    return Style::theme().chartBackground;
}

QColor MarkersPopUp::inverseChartBackground()
{
    QColor color = chartBackgroundColor();
#ifndef Q_OS_MACX
    return QColor(255-color.red(), 255-color.green(), 255-color.blue());
#else
    return color;
#endif
}

// Blending a color toward itself (chartBackgroundColor() alone, at any
// alpha) paints the same color the plot behind it already is -- the box
// vanished into the plot instead of reading as a floating card. Shifting it
// partway toward neutral grey keeps it recognizably "the same side" as the
// chart-background (so it still pairs correctly with inverseChartBackground()'s
// row text) while staying visually distinct from the plot itself, light or
// dark. Same approach as Measurements::hintBackgroundColor().
QColor MarkersPopUp::hintBackgroundColor()
{
    QColor color = chartBackgroundColor();
    const qreal towardGrey = 0.35;
    int r = color.red()   + int((128 - color.red())   * towardGrey);
    int g = color.green() + int((128 - color.green()) * towardGrey);
    int b = color.blue()  + int((128 - color.blue())  * towardGrey);
    QColor tinted(r, g, b);
    tinted.setAlpha(200);
    return tinted;
}

// Same blend-toward-grey technique as hintBackgroundColor(), but pushed
// further so the header row reads as a distinct band rather than blending
// into the rest of the (already tinted) table -- the header/data boundary
// was otherwise impossible to pick out at a glance, especially with many
// columns selected.
QColor MarkersPopUp::headerBackgroundColor()
{
    QColor color = chartBackgroundColor();
    const qreal towardGrey = 0.55;
    int r = color.red()   + int((128 - color.red())   * towardGrey);
    int g = color.green() + int((128 - color.green()) * towardGrey);
    int b = color.blue()  + int((128 - color.blue())  * towardGrey);
    QColor tinted(r, g, b);
    tinted.setAlpha(220);
    return tinted;
}

void MarkersPopUp::updateLabelColors()
{
    QString style = "QLabel { color: " + inverseChartBackground().name() + "; }";
    for (const QList<QWidget*>& row : m_rows) {
        for (QWidget* w : row) {
            QLabel* label = qobject_cast<QLabel*>(w);
            if (label)
                label->setStyleSheet(style);
        }
    }

    QColor headerBg = headerBackgroundColor();
    QString headerStyle = QString("QLabel { color: %1; background-color: rgba(%2, %3, %4, %5); font-weight: bold; }")
                               .arg(inverseChartBackground().name())
                               .arg(headerBg.red()).arg(headerBg.green()).arg(headerBg.blue()).arg(headerBg.alpha());
    for (const MarkersHeaderColumn& col : m_headerColumns) {
        QLabel* label = qobject_cast<QLabel*>(col.button);
        if (label)
            label->setStyleSheet(headerStyle);
    }

    if (m_headerSeparator) {
        QColor line = inverseChartBackground();
        m_headerSeparator->setStyleSheet(QString("QFrame { background-color: rgba(%1, %2, %3, 140); border: none; }")
                                              .arg(line.red()).arg(line.green()).arg(line.blue()));
    }

    // See hintBackgroundColor() above.
    setBackgroundColor(hintBackgroundColor());
    update();
}

void MarkersPopUp::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect roundedRect;
    roundedRect.setX(rect().x() + 5);
    roundedRect.setY(rect().y() + 5);
    roundedRect.setWidth(rect().width() - 10);
    roundedRect.setHeight(rect().height() - 10);

    painter.setBrush(QBrush(m_bgColor));
    painter.setPen(m_penColor);

    painter.drawRoundedRect(roundedRect, 5, 5);
}


void MarkersPopUp::on_remove()
{
    QString str = sender()->objectName();
    str.remove(0,2);
    int markerIndex = str.toInt();
    emit removeMarker(markerIndex);
}

QList <QStringList> MarkersPopUp::getPopupList()
{ // print support

    QList <QStringList> retList;
    QStringList tempList;

    // TODO
//    for(int i = 0; i < m_measurementsList.length(); ++i)
//    {
//        tempList.append(m_markersList.at(i));
//        tempList.append(m_measurementsList.at(i));
//        tempList.append(m_fqList.at(i));
//        tempList.append(m_swrList.at(i));
//        tempList.append(m_rlList.at(i));
//        tempList.append(m_zList.at(i));
//        tempList.append(m_phaseList.at(i));
//        retList.append(tempList);
//        tempList.clear();
//    }
    return retList;
}

void MarkersPopUp::show()
{
    if (m_markers == 0 || m_measurements == 0)
        return;

    setWindowOpacity(0.0);

    animation.setDuration(150);
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);

    setGeometry(QCursor::pos().x() - 400,
                QCursor::pos().y() - 100,
                width(),
                height());

    QWidget::show();

    animation.start();
    if(m_hiding)
    {
        m_timer->start(m_durability);
    }
}

void MarkersPopUp::focusShow()
{
    //qDebug() << "MarkersPopUp::focusShow()" << m_menuVisible;
    // Was: also called activateWindow() here, unconditionally -- and this
    // runs synchronously every time a marker is added (Markers::add() calls
    // straight into this, no deferral), forcing MainWindow to lose real WM
    // activation to this popup on every single marker placed. That's the
    // same class of activation race that was making the plot's wheel/drag
    // look "stuck" (see the graphBriefHint fix), just hitting the
    // Start/Delete buttons instead here. show()+raise() alone still makes
    // this window visible and topmost; WA_ShowWithoutActivating is already
    // off (see the constructor comment), so the WM still grants it real
    // activation if the user actually clicks on it -- that's what needed
    // fixing originally, not forcing activation proactively every time it's
    // shown/refreshed.
    QWidget::show();
    raise();
}

void MarkersPopUp::focusHide()
{
    //qDebug() << "MarkersPopUp::focusHide()" << m_menuVisible;
    if (m_menuVisible) {
        setVisible(true);
        return;
    }
    QWidget::hide();
}

void MarkersPopUp::hideAnimation()
{
    m_timer->stop();
    animation.setDuration(1000);
    animation.setStartValue(1.0);
    animation.setEndValue(0.0);
    animation.start();
}

void MarkersPopUp::hide()
{
    if(getPopupOpacity() == 0.0)
    {
        QWidget::hide();
    }
}

void MarkersPopUp::setPopupOpacity(float opacity)
{
    popupOpacity = opacity;

    setWindowOpacity(opacity);
}

float MarkersPopUp::getPopupOpacity() const
{
    return popupOpacity;
}

void MarkersPopUp::mousePressEvent(QMouseEvent * event)
{
    m_biasX = event->pos().x();
    m_biasY = event->pos().y();
}

void MarkersPopUp::mouseMoveEvent(QMouseEvent * )
{
    m_x = QCursor::pos().x() - m_biasX;
    m_y = QCursor::pos().y() - m_biasY;
    setGeometry(m_x,
                m_y,
                width(),
                height());
    m_mainBiasX = m_x - m_mainX;
    m_mainBiasY = m_y - m_mainY;
}

void MarkersPopUp::MainWindowPos(int x, int y)
{
    m_mainX = x;
    m_mainY = y;

    m_x = x + m_mainBiasX;
    m_y = y + m_mainBiasY;
    setGeometry(m_x,
                m_y,
                width(),
                height());
}

void MarkersPopUp::setPosition(int x, int y)
{
    m_x = x;
    m_y = y;
    setGeometry(m_x,
                m_y,
                width(),
                height());
}

void MarkersPopUp::setTextColor(QString color)
{
    m_textColor = color;
}

void MarkersPopUp::on_translate()
{
    // The m_removeLabel/m_numberLabel/etc. members these calls used to
    // target are gone -- createHeader() builds the header as a dynamic set
    // of QLabels (m_headerColumns) from MarkersHeaderColumn::headerMap()
    // instead now. That map is lazily built once and cached forever
    // (QMap<int, QString> MarkersHeaderColumn::m_mapHeader), so switching
    // language at runtime otherwise never reaches it -- clear it so the
    // next headerMap() call (from createHeader() below) rebuilds it in the
    // new language, then actually rebuild the header labels.
    MarkersHeaderColumn::m_mapHeader.clear();
    createHeader();
}


void MarkersPopUp::createHeader()
{
    QMap<int, QString>& mapHeader = MarkersHeaderColumn::headerMap();

    // createHeader() used to run once per session; it now runs on every
    // Settings Markers-tab edit too (via reloadColumns()), so the old
    // m_headerColumns.clear() -- which dropped the QList entries without
    // deleting the QLabels/separator they pointed to -- would leak a set of
    // widgets on every edit instead of rarely. Delete the previous ones
    // before rebuilding.
    for (const MarkersHeaderColumn& old : m_headerColumns)
        delete old.button;
    m_headerColumns.clear();
    delete m_headerSeparator;
    m_headerSeparator = nullptr;

    m_settings->beginGroup("Markers");
    QString buttons = m_settings->value("header", "0,1,2,3,4,5,6,7,8,9").toString();
    m_settings->endGroup();
    QList<QString> list = buttons.split(',');

    QColor headerBg = headerBackgroundColor();
    QString headerStyle = QString("QLabel { color: %1; background-color: rgba(%2, %3, %4, %5); font-weight: bold; }")
                               .arg(inverseChartBackground().name())
                               .arg(headerBg.red()).arg(headerBg.green()).arg(headerBg.blue()).arg(headerBg.alpha());

    int column = 0;
    foreach (QString key, list) {
        if (key.isEmpty())
            continue;
        MarkersHeaderColumn data;
        int type = key.toInt();
        data.index = column;
        QLabel* label = new QLabel(this);
        data.button = label;
        label->setStyleSheet(headerStyle);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        label->setAlignment(Qt::AlignCenter);
        label->setProperty("field_type", type);
        label->setText(mapHeader[type]);
        // No Qt::AlignHCenter here -- letting the label fill its column
        // (rather than shrinking to its own sizeHint, centered within the
        // cell) is what makes the header background paint as one
        // continuous band across all columns instead of a patchy one
        // behind each label's text only.
        m_layout.addWidget(label, 0, column++);
        m_headerColumns << data;
    }

    QColor line = inverseChartBackground();
    m_headerSeparator = new QFrame(this);
    m_headerSeparator->setFixedHeight(2);
    m_headerSeparator->setStyleSheet(QString("QFrame { background-color: rgba(%1, %2, %3, 140); border: none; }")
                                          .arg(line.red()).arg(line.green()).arg(line.blue()));
    m_layout.addWidget(m_headerSeparator, 1, 0, 1, qMax(column, 1));
}

void MarkersPopUp::reloadColumns()
{
    clearTable();
    createHeader();
    updateMarkers(m_markers, m_measurements, true);
    emit changeColumns();
}


QList<int> MarkersPopUp::getColumns()
{
    QList<int> list;
    for (int i=0; i<m_headerColumns.size(); i++) {
        list << m_headerColumns[i].button->property("field_type").toInt();
    }
    return list;
}

void MarkersPopUp::updateMarkers(int markers, int measurements, bool force)
{
    // on_measurementComplete() calls this on every scan tick, including
    // during a continuous scan. Rebuilding unconditionally destroys and
    // recreates every "X" remove button each time, which can delete the
    // button between a mouse press and release and silently eat the click.
    // Values are refreshed separately via updateInfo(), so skip the rebuild
    // when the table shape hasn't actually changed. Insert/remove-column
    // passes force=true since m_rows must be rebuilt to match the new
    // header size even though the marker/measurement counts are unchanged.
    if (!force && markers == m_markers && measurements == m_measurements) {
        return;
    }

    clearTable();

    m_markers = markers;
    m_measurements = measurements;

    if (markers == 0) {
        hide();
        return;
    }

    for (int i=0; i<m_headerColumns.size(); i++) {
        m_layout.addWidget(m_headerColumns[i].button, 0, i);
        m_headerColumns[i].button->show();
    }
    // clearTable() detaches every layout item generically (it doesn't know
    // header/separator from data cells), so the separator needs re-adding
    // here too, same as the header buttons just above.
    if (m_headerSeparator) {
        m_layout.addWidget(m_headerSeparator, 1, 0, 1, qMax(m_headerColumns.size(), 1));
        m_headerSeparator->show();
    }

    int rowCount = m_measurements==0 ? 1 : m_measurements;
    int rowIndex = 2; // row 0 = header, row 1 = separator
    QString labelStyle = "QLabel { color: " + inverseChartBackground().name() + "; }";
    for (int i=0; i<m_markers; i++) {
        QToolButton* button = new QToolButton(this);
        button->setStyleSheet(Style::toolButton());
        QString str = "RM" + QString::number(i);
        button->setObjectName(str);
        button->setMaximumWidth(20);
        button->setText("X");
        connect(button, &QToolButton::clicked, this, &MarkersPopUp::on_remove);
        m_layout.addWidget(button, rowIndex, 0);
        m_removeButtons << button;
        for (int j=0; j<rowCount; j++) {
            QList<QWidget*> row;
            // Column 0 is the delete button, not a data cell. It is tracked in
            // m_removeButtons instead; keep a placeholder so the remaining
            // indices line up with m_headerColumns.
            row << nullptr;
            for (int k=1; k<m_headerColumns.size(); k++) {
                QLabel* label = new QLabel(this);
                // Not Style::label() (the app's Light/Dark theme text
                // color) -- this table floats over the plot's own
                // independently-configurable chart-background, so it
                // needs a color that contrasts with that instead. See
                // updateLabelColors().
                label->setStyleSheet(labelStyle);
                label->setAlignment(Qt::AlignCenter);
                row << qobject_cast<QLabel*>(label);
                m_layout.addWidget(label, rowIndex, k);
                label->show();
            }
            m_rows << row;
            rowIndex++;
        }
    }

    updateTable();
}

void MarkersPopUp::updateInfo(QList<QList<QVariant>>& info)
{
    // Mirrors updateMarkers()'s own rowCount fallback: with no measurements
    // yet, the table is still built with one (blank) row per marker rather
    // than zero, so info -- which now supplies one row per marker in that
    // case too (see Markers::updateInfo()) -- has to be walked the same way
    // or every cell it fills in gets silently skipped.
    int rowCount = m_measurements==0 ? 1 : m_measurements;
    if (info.size() != (m_markers*rowCount))
        return;

    int rowIndex = 0;
    for (int i=0; i<m_markers; i++) {
        for (int j=0; j<rowCount; j++) {
            QList<QVariant>& rowInfo = info[rowIndex];
            QList<QWidget*>& rowLabel = m_rows[rowIndex];
            for (int k=1; k<m_headerColumns.size(); k++) { // ignore fieldDelete
                if (j != 0 && k == MarkersHeaderColumn::fieldNum)
                    continue;
                QVariant val = rowInfo[k];
                int type = m_headerColumns[k].button->property("field_type").toInt();
                QString str = formatText(type, val);
                QLabel* label = qobject_cast<QLabel*>(rowLabel[k]);
                label->setText(str);
            }
            rowIndex++;
        }
    }
}

void MarkersPopUp::clearTable(void)
{
    QLayoutItem* item = nullptr;
    while((item=m_layout.takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->setVisible(false);
        delete item; // takeAt() hands ownership of the item to us
    }
    for(int i=0; i<m_rows.size(); i++) {
        QList<QWidget*>& row = m_rows[i];
        for (int j=0; j<row.size(); j++) {
            delete row[j];
        }
    }
    m_rows.clear();

    // The delete buttons used to be stored in m_rows via
    // qobject_cast<QLabel*>(button), which is always null for a QToolButton --
    // so they were never freed and every rebuild left another hidden set of
    // them behind, all sharing the same "RM<n>" object names.
    qDeleteAll(m_removeButtons);
    m_removeButtons.clear();
}

void MarkersPopUp::updateTable()
{
    adjustSize();
}

QString MarkersPopUp::formatText(int type, QVariant v)
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
    }
    return m_mapHeader;
}
