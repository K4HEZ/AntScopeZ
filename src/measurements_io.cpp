#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"

extern bool g_developerMode;
extern QMap<QString, QString> g_mapTabPlotNames;
extern int g_maxMeasurements; // defined in measurements.cpp

// Decimal places for R/X in CSV export (Measurements::exportData() below).
// Was a runtime g_developerMode toggle (2 vs 6); made a compile-time
// constant instead 2026-08-20 -- there's no real user-facing reason to
// want more precision here (confirmed against real hardware: a RigExpert
// Match RFE only returns 2 digits of precision itself, so 6 was never
// meaningful, just extra digits). Edit and rebuild if a future device
// actually returns more.
#define EXPORT_PRECISION 2
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::saveData(quint32 number, QString path)
{
    if (number >= (quint32)g_maxMeasurements)
        number = g_maxMeasurements-1;

    if(path.indexOf(".asd") >= 0 )
    {
        QFile saveFile(path);

        if (!saveFile.open(QIODevice::WriteOnly))
        {
            qWarning("Couldn't open save file.");
            return;
        }

        QVector <RawData> data;
        if(m_calibration != NULL)
        {
            if(m_calibration->getCalibrationEnabled())
            {
                data = m_measurements.at(number).dataRXCalib;
            }else
            {
                data = m_measurements.at(number).dataRX;
            }
        }else
        {
            data = m_measurements.at(number).dataRX;
        }

        //Dots
        QJsonObject mainObj;
        mainObj["DotsNumber"] = data.length();

        //Measurements
        QJsonArray measurementsArray;
        for(int i = 0; i < data.length(); ++i)
        {
            QJsonObject obj;
            obj["fq"] = data.at(i).fq;
            obj["r"] = data.at(i).r;
            obj["x"] = data.at(i).x;
            measurementsArray.append(obj);
        }
        mainObj["Measurements"] = measurementsArray;

        QJsonDocument saveDoc(mainObj);

        saveFile.write(saveDoc.toJson());
    }
}

void Measurements::loadData(QString path)
{
    if(path.indexOf(".asd") >= 0 )
    {
        QStringList list;
        list = path.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = path.split("\\");
        }

        QFile loadFile(path);

        if (!loadFile.open(QIODevice::ReadOnly)) {
            g_showMessageBox(NULL, QMessageBox::Information, tr("Error"), tr("Couldn't open saved file."));
            qWarning("Couldn't open saved file.");
            return;
        }

        QByteArray saveData = loadFile.readAll();

        QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
        QJsonObject mainObj = loadDoc.object();

        QJsonArray measureArray = mainObj["Measurements"].toArray();

        int size = measureArray.size();
        if (size < 2) {
            g_showMessageBox(NULL, QMessageBox::Information, tr("Error"), tr("The saved file is too short."));
            qWarning("Couldn't open saved file.");
            return;
        }

        QJsonObject dataObject0 = measureArray.first().toObject();
        RawData data0;
        data0.read(dataObject0);
        double fqMin = data0.fq;
        qint64 fqMinHz = static_cast<qint64>(fqMin * 1000000);
        dataObject0 = measureArray.last().toObject();
        data0.read(dataObject0);
        double fqMax = data0.fq;
        qint64 fqMaxHz = static_cast<qint64>(fqMax * 1000000);
        qint32 dots = size;

        int next = nextPrefix();
        QString nextName = QString("%1> %2").arg(next, 2, 10, QChar('0')).arg(list.last());
        //on_newMeasurement(nextName);
        on_newMeasurement(nextName, fqMinHz, fqMaxHz, dots);
        m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment

        ProgressDlg* progressDlg = new ProgressDlg();
        progressDlg->setValue(0);
        progressDlg->setProgressData(0, size, 1);
        progressDlg->updateActionInfo(tr("Load measurement"));
        progressDlg->updateStatusInfo(tr("please wait ...."));
        progressDlg->show();
        progressDlg->setWindowModality(Qt::WindowModal);
        QApplication::processEvents();

        for(int i = 0; i < size; ++i)
        {
            QJsonObject dataObject = measureArray[i].toObject();
            RawData data;
            data.read(dataObject);
            on_newData(data);
            if ((i%10) == 0) {
                progressDlg->setValue(i);
                progressDlg->updateStatusInfo(QString(tr("loaded %1 dots, from %2")).arg(i).arg(size));
            }
        }
        progressDlg->hide();
        delete progressDlg;

        emit import_finished(fqMin*1000, fqMax*1000);
        // Points column stays "--" (the on_newMeasurement() table rebuild's
        // own placeholder for a not-yet-populated row) until something
        // stamps the real count in -- on_measurementComplete() already
        // does exactly that for a finished live scan; every import path
        // finishes populating dataRX with no equivalent call, so it was
        // silently left at "--" until some unrelated later table rebuild
        // happened to notice the real count.
        on_measurementComplete();
    }else
    {
        importData(path);
    }
    on_redrawGraphs();
}

void Measurements::exportData(QString _name, int _type, int _number, bool _applyCable, QString _description)
{
    if (_number < 0 || m_measurements.isEmpty() || (_number >= m_measurements.size()))
        return;

    bool calibr = (m_calibration != nullptr) && (m_calibration->getCalibrationEnabled());
    QVector<RawData> vector;
    if (_applyCable)
    {
        switch(m_farEndMeasurement) {
        case 1:
            vector = calibr ? m_farEndMeasurementsSub.at(_number).dataRXCalib : m_farEndMeasurementsSub.at(_number).dataRX;
            break;
        case 2:
            vector = calibr ? m_farEndMeasurementsAdd.at(_number).dataRXCalib : m_farEndMeasurementsAdd.at(_number).dataRX;
            break;
        default:
            vector = calibr ? m_measurements.at(_number).dataRXCalib : m_measurements.at(_number).dataRX;
            break;
        }
    } else {
        vector = calibr ? m_measurements.at(_number).dataRXCalib : m_measurements.at(_number).dataRX;
    }
    exportData(_name, _type, vector, _description);
}

void Measurements::exportData(QString _name, int _type, QVector<RawData>& vector, QString _description)
{
    int len = vector.length();;
    qInfo() << "Touchstone export:"
            << _name
            << "points:"
            << vector.size();
    if(_name.indexOf(".s1p") >= 0 )
    {
        QFile file(_name);

        if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))//if (!file.open(QFile::ReadWrite))
        {
            qInfo() << "Touchstone open failed:"
                    << _name
                    << file.errorString();
            return;
        }

        QTextStream out(&file);

        out << "! Touchstone file generated by AntScopeZ";
        out << "\n";

        double Rswr = ((m_calibration != NULL) && (m_calibration->getCalibrationEnabled())) ? m_calibration->getZ0() : 50;

        if (_type == 0) // Z, RI
        {
            out << "# MHz Z RI R " << Rswr << "\n";
            out << "! Format: Frequency Z-real Z-imaginary (normalized to " << Rswr << " Ohm)\n";
        }else if (_type == 1) // S, RI
        {
            out << "# MHz S RI R " << Rswr << "\n";
            out << "! Format: Frequency S-real S-imaginary (normalized to " << Rswr << " Ohm)\n";
        }
        else if (_type == 2) // S, MA
        {
            out << "# MHz S MA R " << Rswr << "\n";
            out << "! Format: Frequency S-magnitude S-angle (normalized to " << Rswr << " Ohm, angle in degrees)\n";
        }
        else if (_type == 3) // S, DB
        {
            out << "# MHz S DB R " << Rswr << "\n";
            out << "! Format: Frequency S-magnitude(dB) S-angle (normalized to " << Rswr << " Ohm, angle in degrees)\n";
        }

        if (!_description.isEmpty())
            out << _description << "\n";

        for (int i = 0; i < len; ++i)
        {
            QString s;

            s = QString("%1").arg(vector.at(i).fq, 0, 'f', 6);		// Fq
            out << s << " ";

            double R = vector.at(i).r;
            double X = vector.at(i).x;

            if (_type == 0) // Z, RI
            {
                if (!qIsNaN(R))
                    s = QString::number(R/Rswr,'g',4);           // R
                else
                    s = "0";
                out << s << " ";
                if (!qIsNaN(X))
                    s = QString::number(X/Rswr,'g',4);           // X
                else
                    s = "0";
                out << s << "\n";
            }
            else
            if (_type == 1) // S, RI
            {
                double Gre = (R*R-Rswr*Rswr+X*X)/((R+Rswr)*(R+Rswr)+X*X);
                double Gim = (2*Rswr*X)/((R+Rswr)*(R+Rswr)+X*X);

                if (!qIsNaN(Gre))
                    s = QString::number(Gre,'g',4);              // Real
                else
                    s = "0";
                out << s << " ";

                if (!qIsNaN(Gim))
                    s = QString::number(Gim,'g',4);              // Imaginary
                else
                    s = "0";
                out << s << "\n";

            }
            else
            if (_type == 2) // S, MA
            {
                double Gre = (R*R-Rswr*Rswr+X*X)/((R+Rswr)*(R+Rswr)+X*X);
                double Gim = (2*Rswr*X)/((R+Rswr)*(R+Rswr)+X*X);

                if (!qIsNaN(Gre))
                    s = QString::number(sqrt(Gre*Gre+Gim*Gim),'g',4);		// Magnitude
                else
                    s = "0";
                out << s << " ";

                if (!qIsNaN(Gim))
                    s = QString::number(atan2(Gim,Gre)/3.1415926*180.0,'g',4);		// Angle
                else
                    s = "0";
                out << s << "\n";
            }
            else
            if (_type == 3) // S, DB
            {
                double Gre = (R*R-Rswr*Rswr+X*X)/((R+Rswr)*(R+Rswr)+X*X);
                double Gim = (2*Rswr*X)/((R+Rswr)*(R+Rswr)+X*X);

                if (!qIsNaN(Gre))
                    s = QString::number(20*log10(sqrt(Gre*Gre+Gim*Gim)),'g',4);	// Magnitude, dB
                else
                    s = "0";
                out << s << " ";

                if (!qIsNaN(Gim))
                    s = QString::number(atan2(Gim,Gre)/3.1415926*180.0,'g',4);		// Angle
                else
                    s = "0";
                out << s << "\n";
            }
        }
        out.flush();
    }else if(_name.indexOf(".csv") >= 0 )
    {
        QString str;
        QFile file(_name);
        bool result = file.open(QFile::ReadWrite);
        if(result)
        {
            str = "#Frequency(MHz);R;X";
            file.write( str.toLocal8Bit(), str.length());
            file.write("\r\n", 2);
            for (int i = 0; i < len; ++i)
            {
                str = QString::number(vector.at(i).fq, 'f', 6) +
                "," +//";" +
                QString::number(vector.at(i).r,'f',EXPORT_PRECISION) +
                "," +//";" +
                QString::number(vector.at(i).x,'f',EXPORT_PRECISION);

                file.write( str.toLocal8Bit(), str.length());
                file.write("\r\n", 2);
            }
            file.close();
        }
    }else if(_name.indexOf(".nwl") >= 0 )
    {
        QString str;
        QFile file(_name);
        bool result = file.open(QFile::ReadWrite);
        if(result)
        {
            str = "/\"Freq(MHz)\" \"Rs\" \"Xs\"/";
            file.write( str.toLocal8Bit(), str.length());
            file.write("\r\n", 2);

            for (int i = 0; i < len; ++i)
            {
                str = QString::number(vector.at(i).fq, 'f', 6) +
                " " +//";" +
                QString::number(vector.at(i).r,'f',2) +
                " " +//";" +
                QString::number(vector.at(i).x,'f',2);

                file.write( str.toLocal8Bit(), str.length());
                file.write("\r\n", 2);
            }
            file.close();
        }
    }
}

void Measurements::importData(QString _name, bool /*user_format*/)
{
    QStringList list;
    list = _name.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = _name.split("\\");
    }
    on_newMeasurement(list.last());
    m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment

    QFile file(_name);
    bool result = file.open(QFile::ReadOnly);
    if(result)
    {
        QString str = file.readAll();
        double fqMin = DBL_MAX;
        double fqMax = 0;
        QStringList nList = str.split('\n');

        str = nList.takeFirst();
        if (str.at(0) == '#') {
            str.replace('#', ' ');
            on_newUserDataHeader(str.trimmed().split(','));
        }

        while (!nList.isEmpty()) {
            str = nList.takeFirst();
            if (str.isEmpty())
                continue;
            QStringList fields = str.split(',');
            RawData rdata;
            UserData udata;
            bool ok;
            QString field = fields.takeFirst();
            rdata.fq = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            udata.fq = rdata.fq;
            fqMin = qMin(fqMin, rdata.fq);
            fqMax = qMax(fqMax, rdata.fq);

            field = fields.takeFirst();
            rdata.r = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            field = fields.takeFirst();
            rdata.x = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            while (!fields.isEmpty()) {
                udata.values.append(fields.takeFirst().toDouble(&ok));
                if (!ok) {
                    qDebug() << "***** ERROR: " << str;
                    return;
                }
            }
            on_newUserData(rdata, udata);
        }
        emit import_finished(fqMin*1000, fqMax*1000);
        on_measurementComplete(); // stamp the real Points count -- see loadData()'s own comment on this
        on_redrawGraphs();
    }
}

// 2-port Touchstone export. Unlike exportData()'s S,RI/S,MA paths, this
// needs no R/X -> Gamma conversion -- dataSParam already holds the raw
// complex S-parameters exactly as parsed off the original file's option
// line (sparamFromFormat()), so this is a straight passthrough. That also
// means there's no real reference impedance to round-trip: the original
// file's own "R <value>" is never stored on the measurement (only used
// transiently, at import, to derive the R/X-based graphs). "R 50" here is
// just the near-universal RF convention, same fallback exportData() itself
// uses when no calibration Z0 is available -- not a claim about what the
// source file actually said.
void Measurements::exportSParamData(QString _name, int _type, int _number, QString _description)
{
    if (_number < 0 || m_measurements.isEmpty() || (_number >= m_measurements.size()))
        return;

    const QList<SParamPoint>& points = m_measurements.at(_number).dataSParam;
    if (points.isEmpty())
        return;

    QFile file(_name);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
    {
        qInfo() << "Touchstone (2-port) open failed:" << _name << file.errorString();
        return;
    }

    // _type: 0 = RI (real/imaginary), 1 = MA (magnitude/angle), 2 = DB
    // (magnitude in dB/angle) -- same three choices exportData() offers for
    // 1-port S-parameter export (no Z here -- Z would mean Z21/Z12/Z22, a
    // different quantity dataSParam never holds; see the Z-parameter
    // 2-port import guard's own comment).
    QString formatToken = (_type == 2) ? "DB" : (_type == 1) ? "MA" : "RI";
    auto formatPair = [_type](std::complex<double> v) -> QString {
        double a, b;
        if (_type == 2) { // DB
            a = 20*log10(std::abs(v));
            b = std::arg(v)*180.0/M_PI;
        } else if (_type == 1) { // MA
            a = std::abs(v);
            b = std::arg(v)*180.0/M_PI;
        } else { // RI
            a = v.real();
            b = v.imag();
        }
        return QString::number(a, 'g', 4) + " " + QString::number(b, 'g', 4);
    };

    QTextStream out(&file);
    out << "! Touchstone file generated by AntScopeZ\n";
    out << "# MHz S " << formatToken << " R 50\n";
    if (_type == 2)
        out << "! Format: Frequency S11-mag(dB) S11-angle S21-mag(dB) S21-angle S12-mag(dB) S12-angle S22-mag(dB) S22-angle\n";
    else if (_type == 1)
        out << "! Format: Frequency S11-mag S11-angle S21-mag S21-angle S12-mag S12-angle S22-mag S22-angle\n";
    else
        out << "! Format: Frequency S11-real S11-imag S21-real S21-imag S12-real S12-imag S22-real S22-imag\n";
    if (!_description.isEmpty())
        out << _description << "\n";

    foreach (const SParamPoint& sp, points) {
        out << QString::number(sp.fq, 'f', 6) << " "
            << formatPair(sp.s11) << " " << formatPair(sp.s21) << " "
            << formatPair(sp.s12) << " " << formatPair(sp.s22) << "\n";
    }
    out.flush();
}

std::complex<double> Measurements::sparamFromFormat(int iFormat, double v1, double v2)
{
    switch (iFormat) {
    case 2: // RI -- already Cartesian
        return std::complex<double>(v1, v2);
    case 3: // DB -- v1 is magnitude in dB, v2 is angle in degrees
        return std::polar(pow(10.0, v1/20.0), v2/180.0*M_PI);
    default: // MA -- v1 is linear magnitude, v2 is angle in degrees
        return std::polar(v1, v2/180.0*M_PI);
    }
}

SParamPoint Measurements::zToSParam(double fq, std::complex<double> z11,
                                     std::complex<double> z21, std::complex<double> z12,
                                     std::complex<double> z22, double z0)
{
    // Standard 2-port Z-to-S identity (real, positive z0 -- Touchstone's
    // "R n" applies the same reference impedance to both ports, so there's
    // no need for the more general unequal-port-impedance form):
    //
    //   dZ  = (Z11+Z0)(Z22+Z0) - Z12*Z21
    //   S11 = ((Z11-Z0)(Z22+Z0) - Z12*Z21) / dZ
    //   S12 = 2*Z12*Z0 / dZ
    //   S21 = 2*Z21*Z0 / dZ
    //   S22 = ((Z11+Z0)(Z22-Z0) - Z12*Z21) / dZ
    std::complex<double> zRef(z0, 0.0);
    std::complex<double> cross = z12 * z21;
    std::complex<double> dZ = (z11 + zRef) * (z22 + zRef) - cross;

    SParamPoint sp;
    sp.fq = fq;
    if (std::abs(dZ) > 0.0) {
        sp.s11 = ((z11 - zRef) * (z22 + zRef) - cross) / dZ;
        sp.s12 = (2.0 * z12 * zRef) / dZ;
        sp.s21 = (2.0 * z21 * zRef) / dZ;
        sp.s22 = ((z11 + zRef) * (z22 - zRef) - cross) / dZ;
    } else {
        // dZ == 0 is a degenerate/singular network at this frequency
        // (division would be NaN/Inf) -- leave this point at a flat zero
        // rather than poisoning the whole trace with a non-finite value.
        sp.s11 = sp.s12 = sp.s21 = sp.s22 = std::complex<double>(0.0, 0.0);
    }
    return sp;
}

// std::arg() always wraps into (-180, 180] degrees. A real transmission
// phase can rack up many full turns across a wide sweep, so the raw
// wrapped value jumps unpredictably between adjacent points -- worse the
// fewer points there are (a >360-degree change between two samples wraps
// into something that looks like noise, not a smooth ramp). Unwrap by
// accumulating the shortest-path delta between consecutive points
// instead, same technique as numpy.unwrap()/MATLAB's unwrap(). This can't
// recover the *true* phase if an actual >360-degree jump happened between
// two real samples (that's undersampling, not fixable in the display
// layer), but it's still a smooth, honest trace instead of misleading
// vertical jumps, and is exact whenever points are reasonably dense.
double Measurements::unwrapPhaseDeg(double rawDeg, bool& havePrev, double& prevRaw, double& prevUnwrapped)
{
    if (!havePrev) {
        havePrev = true;
        prevRaw = prevUnwrapped = rawDeg;
        return rawDeg;
    }
    double delta = rawDeg - prevRaw;
    while (delta > 180.0) delta -= 360.0;
    while (delta <= -180.0) delta += 360.0;
    prevUnwrapped += delta;
    prevRaw = rawDeg;
    return prevUnwrapped;
}

// Fills in a just-imported 2-port measurement's dataSParam plus the
// derived S21/S12 magnitude(dB)/phase(degrees) graphs the S21 tab
// actually plots. Batch, not per-point -- on_newSParamPoint() (the live-
// capture path) redraws after every single point, which is fine for one
// point at a time but far too slow across a whole imported file.
void Measurements::populateSParamData(const QList<SParamPoint>& points)
{
    if (m_measurements.isEmpty())
        return;

    measurement& mm = m_measurements.last();

    bool haveS21Prev = false, haveS12Prev = false;
    double s21PrevRaw = 0, s21PrevUnwrapped = 0;
    double s12PrevRaw = 0, s12PrevUnwrapped = 0;

    foreach (const SParamPoint& sp, points) {
        mm.dataSParam.append(sp);

        // sp.fq is MHz, matching RawData.fq's own convention -- *1000 to
        // the kHz every chart key actually uses (see e.g. this file's
        // RawData path, or "double fq = _rawData.fq*1000;" in
        // measurements.cpp) -- without it, the whole trace renders
        // compressed 1000x toward the origin.
        double fqKey = sp.fq*1000;

        QCPGraphData mag, phase;
        mag.key = phase.key = fqKey;

        mag.value = 20*log10(std::abs(sp.s21));
        phase.value = unwrapPhaseDeg(std::arg(sp.s21)*180.0/M_PI, haveS21Prev, s21PrevRaw, s21PrevUnwrapped);
        mm.s21MagGraph.add(mag);
        mm.s21PhaseGraph.add(phase);

        mag.value = 20*log10(std::abs(sp.s12));
        phase.value = unwrapPhaseDeg(std::arg(sp.s12)*180.0/M_PI, haveS12Prev, s12PrevRaw, s12PrevUnwrapped);
        mm.s12MagGraph.add(mag);
        mm.s12PhaseGraph.add(phase);
    }
}

void Measurements::importData(QString _name)
{
    if((_name.indexOf(".s1p") >= 0) || (_name.indexOf(".s2p") >= 0))
    {
        QStringList list;
        list = _name.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = _name.split("\\");
        }

        QString sPathName = _name;

        if (sPathName.isEmpty())
        {
            return;
        }

        QFile ifs(sPathName);

        if (!ifs.open(QFile::ReadWrite))
        {
            return;
        }
        QTextStream in(&ifs);
        bool bGood = true;

        int iLines=0, iPoints=0;

        double  fqmul = 1000.0; // Default is GHz
        int iUnit = 1; // Default is S
        int iFormat = 1; // Default is MA

        QString line;//char str[1000]; // Whole string
        char strn[5][100]; // Substrings

        double f, param1, param2; // S11 (or Z11) pair
        double s21p1, s21p2, s12p1, s12p2, s22p1, s22p2; // 2-port pairs

        double Z0 = 50;

        double fqMin = DBL_MAX;
        double fqMax = 0;
        QList<RawData> rawArray;
        QList<SParamPoint> sparamArray;
        do//while (ifs.isOpen() && (!ifs.eof()))
        {
            line = in.readLine();
            line = line.toUpper();
            iLines++;

            if ( (line.length() > 2) && (line[0] == '#')) // Option line
            {
                line.remove(0,1);
                int ns = sscanf(line.toLocal8Bit(), "%s %s %s %s %s", strn[0], strn[1], strn[2], strn[3], strn[4]);
                for (int i=0; i<ns; i++)
                {
                    // Frequency unit

                    if (!strcmp(strn[i], "GHZ"))
                        fqmul = 1000.0;
                    else
                    if (!strcmp(strn[i], "MHZ"))
                        fqmul = 1.0;
                    else
                    if (!strcmp(strn[i], "KHZ"))
                        fqmul = 0.001;
                    else
                    if (!strcmp(strn[i], "HZ"))
                        fqmul = 0.000001;
                    else

                    // Parameter

                    if (!strcmp(strn[i], "S"))
                        iUnit = 1;
                    else
                    if (!strcmp(strn[i], "Z"))
                        iUnit = 2;
                    else

                    // Format

                    if (!strcmp(strn[i], "MA"))
                        iFormat = 1;
                    else
                    if (!strcmp(strn[i], "RI"))
                        iFormat = 2;
                    else
                    if (!strcmp(strn[i], "DB"))
                        iFormat = 3;
                    else

                    // R n

                    if (!strcmp(strn[i], "R"))
                    {
                        if ( i < (ns-1) )
                        {
                            i++;

//                            setlocale(LC_NUMERIC,"C");
                            Z0 = atof(strn[i]);
//                            setlocale(LC_NUMERIC,"");

                            if ( (Z0<=0) || (Z0>10000) )
                            {
                                //bErr = true;
                                //break;
                                return;
                            }
                        }
                        else
                        {
                            //bErr = true;
                            //break;
                            return;
                        }
                    }
                    else
                    {
                        //bErr = true;
                        //break;
                        return;
                    }
                }

                // Check possible combinations
                if(! (((iUnit == 1) && (iFormat == 1)) // S, MA
                        || ((iUnit == 1) && (iFormat == 2))  // S, RI
                        || ((iUnit == 1) && (iFormat == 3))  // S, DB
                        || ((iUnit == 2) && (iFormat == 2))  // Z, RI
                    ))
                {
                    return;
                }

                continue;
            }

            if ( (strstr(line.toLocal8Bit(), "!") != NULL) || (strstr(line.toLocal8Bit(), ".") == NULL) ) // Comment or void line
                continue;

            // Scan data lines -- try a 2-port row (freq + 8 values: S11,
            // S21, S12, S22 each as a value pair, in that order per the
            // Touchstone spec) first, fall back to a 1-port row (freq +
            // 2 values). The field count actually present on the line is
            // the reliable signal, not the file extension (a naming
            // convention only) -- one path handles both .s1p and .s2p.
            int nFields = sscanf(line.toLocal8Bit(), "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
                                  &f, &param1, &param2, &s21p1, &s21p2, &s12p1, &s12p2, &s22p1, &s22p2);
            bool lineIs2Port = (nFields == 9);
            if (!lineIs2Port && (nFields != 3))
            {
                return;
            }

            std::complex<double> s11c = sparamFromFormat(iFormat, param1, param2);

            double r = 0, x = 0;
            if (iUnit == 2) // Z, RI -- direct copy, not a reflection coefficient
            {
                r = s11c.real();
                x = s11c.imag();
            }
            else // S, MA/RI/DB -- reflection coefficient -> equivalent series R/X
            {
                double Gr = s11c.real();
                double Gi = s11c.imag();
                r = (1-Gr*Gr-Gi*Gi)/((1-Gr)*(1-Gr)+Gi*Gi);
                x = (2*Gi)/((1-Gr)*(1-Gr)+Gi*Gi);
            }

            if ( qIsNaN(r) || (r<0) )
            {
                r = 0;
            }
            if ( qIsNaN(x) )
            {
                x = 0;
            }

            RawData data;
            data.fq = f*fqmul;
            data.r =r*(Z0);
            data.x =x*(Z0);
            //on_newData(data);
            rawArray.append(data);
            iPoints++;
            fqMin = qMin(fqMin, data.fq);
            fqMax = qMax(fqMax, data.fq);

            // A 2-port Z-parameter file (Z, RI is a real, allowed
            // combination per the check above) puts Z21/Z12/Z22 in these
            // same 9 columns, not S21/S12/S22 -- a different physical
            // quantity (ohms, not a unitless ratio). #7: these used to be
            // silently skipped entirely rather than converted; now run
            // through zToSParam()'s real Z-to-S 2-port matrix conversion,
            // same as s11c already is via the iUnit==2 branch above for
            // the RawData R/X. S-parameter files (iUnit==1) need no
            // conversion -- s11c/s21/s12/s22 are already S-parameters.
            if (lineIs2Port && (iUnit == 1))
            {
                SParamPoint sp;
                sp.fq = f*fqmul;
                sp.s11 = s11c;
                sp.s21 = sparamFromFormat(iFormat, s21p1, s21p2);
                sp.s12 = sparamFromFormat(iFormat, s12p1, s12p2);
                sp.s22 = sparamFromFormat(iFormat, s22p1, s22p2);
                sparamArray.append(sp);
            }
            else if (lineIs2Port && (iUnit == 2))
            {
                std::complex<double> z21 = sparamFromFormat(iFormat, s21p1, s21p2);
                std::complex<double> z12 = sparamFromFormat(iFormat, s12p1, s12p2);
                std::complex<double> z22 = sparamFromFormat(iFormat, s22p1, s22p2);
                sparamArray.append(zToSParam(f*fqmul, s11c, z21, z12, z22, Z0));
            }
        }while (!line.isNull());

        on_newMeasurement(list.last(), static_cast<qint64>(fqMin*1000000), static_cast<qint64>(fqMax*1000000), iPoints);
        m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment
        foreach (auto data, rawArray) {
            on_newData(data);
        }
        if (!sparamArray.isEmpty())
        {
            populateSParamData(sparamArray);
        }
        emit import_finished(fqMin*1000, fqMax*1000);
        on_measurementComplete(); // stamp the real Points count -- see loadData()'s own comment on this

        if (bGood && (iPoints>1) )
        {            
            return;
        }
        else
        {
            return;
        }
    }
    else if(_name.indexOf(".csv") >= 0 )
    {        
        // Never confirmed working (User Defined tab, gated by
        // USER_DEFINED_FEATURE -- see CMakeLists.txt); left exactly as it
        // was rather than fixed, per 2026-08-20 decision to just gate it
        // off until real EFRX-capable hardware turns up.
#if USER_DEFINED_FEATURE
        if (g_developerMode) {
            importData(_name, true);
            return;
        }
#endif

        QStringList list;
        list = _name.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = _name.split("\\");
        }
//        on_newMeasurement(list.last());
        QList<RawData> rawArray;
        QFile file(_name);
        bool result = file.open(QFile::ReadOnly);
        if(result)
        {
            QString str = file.readAll();
            double fqMin = DBL_MAX;
            double fqMax = 0;
            QStringList nList = str.split('\n');

            double mul=1.0;
            QString strFQ = nList.at(0);
            if (strFQ.contains("kHz", Qt::CaseInsensitive))
                mul = 0.001;
            else if (strFQ.contains("MHz", Qt::CaseInsensitive))
                mul = 1;
            else if (strFQ.contains("GHz", Qt::CaseInsensitive))
                mul = 1000;
            else if (strFQ.contains("Hz", Qt::CaseInsensitive))
                mul = 0.000001;
            else {
                QStringList dList = strFQ.split(',');
                if(dList.length() == 3)
                {
                    RawData data;
                    data.fq = dList.at(0).toDouble()*mul;
                    data.r = dList.at(1).toDouble();
                    data.x = dList.at(2).toDouble();
                    //on_newData(data);
                    rawArray.append(data);
                    fqMin = qMin(fqMin, data.fq);
                    fqMax = qMax(fqMax, data.fq);
                }

            }
            for(int i = 1; i < nList.length(); ++i)
            {
                QStringList dList = nList.at(i).split(',');
                if(dList.length() == 3)
                {
                    RawData data;
                    data.fq = dList.at(0).toDouble()*mul;
                    data.r = dList.at(1).toDouble();
                    data.x = dList.at(2).toDouble();
                    //on_newData(data);
                    rawArray.append(data);
                    fqMin = qMin(fqMin, data.fq);
                    fqMax = qMax(fqMax, data.fq);
                }
            }
            on_newMeasurement(list.last(), static_cast<qint64>(fqMin*1000000), static_cast<qint64>(fqMax*1000000), rawArray.length());
            m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment
            foreach (auto data, rawArray) {
                on_newData(data);
            }
            emit import_finished(fqMin*1000, fqMax*1000);
            on_measurementComplete(); // stamp the real Points count -- see loadData()'s own comment on this
        }
    }else if(_name.indexOf(".nwl") >= 0 )
    {
        QStringList list;
        list = _name.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = _name.split("\\");
        }
//        on_newMeasurement(list.last());

        QList<RawData> rawArray;
        QFile file(_name);
        bool result = file.open(QFile::ReadOnly);
        if(result)
        {
            QString str = file.readAll();

            double fqMin = DBL_MAX;
            double fqMax = 0;
            QStringList nList = str.split('\n');

            double mul=1.0;
            QString strFQ = nList.at(0);
            if (strFQ.contains("kHz", Qt::CaseInsensitive))
                mul = 0.001;
            else if (strFQ.contains("MHz", Qt::CaseInsensitive))
                mul = 1;
            else if (strFQ.contains("GHz", Qt::CaseInsensitive))
                mul = 1000;
            else if (strFQ.contains("Hz", Qt::CaseInsensitive))
                mul = 0.000001;

            for(int i = 1; i < nList.length(); ++i)
            {
                QStringList dList = nList.at(i).split(' ');
                if(dList.length() ==3)
                {
                    RawData data;
                    data.fq = dList.at(0).toDouble()*mul;
                    data.r = dList.at(1).toDouble();
                    data.x = dList.at(2).toDouble();
                    //on_newData(data);
                    rawArray.append(data);                    fqMin = qMin(fqMin, data.fq);
                    fqMax = qMax(fqMax, data.fq);
                }
            }
            on_newMeasurement(list.last(), static_cast<qint64>(fqMin*1000000), static_cast<qint64>(fqMax*1000000), rawArray.length());
            m_measurements.last().dirty = false; // loaded from a file, see measurement::dirty's own comment
            foreach (auto data, rawArray) {
                on_newData(data);
            }
            emit import_finished(fqMin*1000, fqMax*1000);
            on_measurementComplete(); // stamp the real Points count -- see loadData()'s own comment on this
        }
    } else {
        g_showMessageBox(nullptr, QMessageBox::Information, tr("Load data"), tr("Oops, this format is not supported!"), QMessageBox::Close);
    }
}

int Measurements::nextPrefix()
{
    int next = 0;
    for (int idx=0; idx<m_measurements.size(); idx++)
    {
        QString existed = m_measurements[idx].name;
        if (existed.indexOf('>') == 2) {
            QString num = existed.left(2);
            bool ok = false;
            int prefix = num.toInt(&ok);
            if (ok) {
                next = qMax(next, prefix);
            }
        }
    }
    next++;
    if (next > 99)
        next = 1;
    return next;
}

