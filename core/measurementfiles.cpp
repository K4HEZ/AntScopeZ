#include "measurementfiles.h"
#include "rfmath.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <math.h>
#include <string.h>

// Decimal places for R/X in CSV export (writeRaw() below).
// Was a runtime g_developerMode toggle (2 vs 6); made a compile-time
// constant instead 2026-08-20 -- there's no real user-facing reason to
// want more precision here (confirmed against real hardware: a RigExpert
// Match RFE only returns 2 digits of precision itself, so 6 was never
// meaningful, just extra digits). Edit and rebuild if a future device
// actually returns more.
#define EXPORT_PRECISION 2
// One Touchstone comment line recording Corrections; parsed back by
// parseCorrectionsLine(). Readers that don't know it just skip the comment.
static QString correctionsLine(const Corrections& c)
{
    QString cable = (c.cableMode == 1) ? "subtract" : (c.cableMode == 2) ? "add" : "none";
    QString line = QString("! AntScopeZ corrections: OSL calibration=%1; cable=%2")
                       .arg(c.osl ? "yes" : "no", cable);
    if (c.cableMode != 0) {
        static const char* units[] = {"dB/100ft", "dB/ft", "dB/100m", "dB/m"};
        const char* unit = units[qBound(0, c.cable.lossUnits, 3)];
        QString at = c.cable.lossAtAnyFq ? QString("any frequency")
                                         : QString("%1 MHz").arg(c.cableLossFqMHz);
        line += QString(", length=%1 ft\n! AntScopeZ cable model: velocity factor %2, R0 %3 ohm, "
                        "conductive loss %4 %6, dielectric loss %5 %6, at %7")
                    .arg(c.cable.lengthFeet, 0, 'f', 2)
                    .arg(c.cable.velFactor, 0, 'f', 4)
                    .arg(c.cable.resistance, 0, 'f', 2)
                    .arg(c.cable.lossConductive)
                    .arg(c.cable.lossDielectric)
                    .arg(unit, at);
    }
    return line;
}

// `upper` is an upper-cased line; true if it was a corrections line.
static bool parseCorrectionsLine(const QString& upper, Corrections& c)
{
    if (!upper.startsWith("! ANTSCOPEZ CORRECTIONS:"))
        return false;
    c.osl = upper.contains("OSL CALIBRATION=YES");
    c.cableMode = upper.contains("CABLE=SUBTRACT") ? 1 : upper.contains("CABLE=ADD") ? 2 : 0;
    int i = upper.indexOf("LENGTH=");
    if (i >= 0)
        c.cable.lengthFeet = upper.mid(i + 7).section(' ', 0, 0).toDouble();
    return true;
}

static QJsonObject correctionsJson(const Corrections& c)
{
    QJsonObject o;
    o["OslCalibration"] = c.osl;
    o["Cable"] = (c.cableMode == 1) ? "subtract" : (c.cableMode == 2) ? "add" : "none";
    o["CableLengthFeet"] = c.cable.lengthFeet;
    if (c.cableMode != 0) {
        o["CableVelocityFactor"] = c.cable.velFactor;
        o["CableR0"] = c.cable.resistance;
        o["CableLossConductive"] = c.cable.lossConductive;
        o["CableLossDielectric"] = c.cable.lossDielectric;
        o["CableLossUnits"] = c.cable.lossUnits;
        o["CableLossAtAnyFrequency"] = c.cable.lossAtAnyFq;
        o["CableLossFrequencyMHz"] = c.cableLossFqMHz;
    }
    return o;
}

static Corrections correctionsFromJson(const QJsonObject& o)
{
    Corrections c;
    c.osl = o["OslCalibration"].toBool();
    QString cable = o["Cable"].toString();
    c.cableMode = (cable == "subtract") ? 1 : (cable == "add") ? 2 : 0;
    c.cable.lengthFeet = o["CableLengthFeet"].toDouble();
    if (o.contains("CableVelocityFactor")) {
        c.cable.velFactor = o["CableVelocityFactor"].toDouble();
        c.cable.resistance = o["CableR0"].toDouble();
        c.cable.lossConductive = o["CableLossConductive"].toDouble();
        c.cable.lossDielectric = o["CableLossDielectric"].toDouble();
        c.cable.lossUnits = o["CableLossUnits"].toInt();
        c.cable.lossAtAnyFq = o["CableLossAtAnyFrequency"].toBool();
        c.cableLossFqMHz = o["CableLossFrequencyMHz"].toDouble();
    }
    return c;
}

MeasurementFiles::ReadResult MeasurementFiles::readAsd(const QString& path)
{
    ReadResult res;
    QFile loadFile(path);

    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open saved file.");
        res.error = ReadError::CannotOpen;
        return res;
    }

    QByteArray saveData = loadFile.readAll();

    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
    QJsonObject mainObj = loadDoc.object();

    QJsonArray measureArray = mainObj["Measurements"].toArray();
    if (mainObj.contains("Corrections"))
        res.applied = correctionsFromJson(mainObj["Corrections"].toObject());

    int size = measureArray.size();
    if (size < 2) {
        qWarning("Couldn't open saved file.");
        res.error = ReadError::TooShort;
        return res;
    }

    for (int i = 0; i < size; ++i)
    {
        RawData data;
        data.read(measureArray[i].toObject());
        res.raw.append(data);
    }
    // Only used if it matches the points one for one.
    QJsonArray origArray = mainObj["AsReceived"].toArray();
    if (origArray.size() == size) {
        for (int i = 0; i < size; ++i) {
            RawData data;
            data.read(origArray[i].toObject());
            res.asReceived.append(data);
        }
    }
    // First/last point, not min/max -- as the loader always did.
    res.fqMinMHz = res.raw.first().fq;
    res.fqMaxMHz = res.raw.last().fq;
    res.error = ReadError::None;
    return res;
}

MeasurementFiles::ReadResult MeasurementFiles::readTouchstone(const QString& path)
{
    ReadResult res;
    QString sPathName = path;

    if (sPathName.isEmpty())
    {
        return res;
    }

    QFile ifs(sPathName);

    if (!ifs.open(QFile::ReadWrite))
    {
        return res;
    }
    QTextStream in(&ifs);
    int iLines=0;

    double  fqmul = 1000.0; // Default is GHz
    int iUnit = 1; // Default is S
    int iFormat = 1; // Default is MA

    QString line;//char str[1000]; // Whole string
    char strn[5][100]; // Substrings

    double f, param1, param2; // S11 (or Z11) pair
    double s21p1, s21p2, s12p1, s12p2, s22p1, s22p2; // 2-port pairs

    double Z0 = 50;

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
                            return res;
                        }
                    }
                    else
                    {
                        //bErr = true;
                        //break;
                        return res;
                    }
                }
                else
                {
                    //bErr = true;
                    //break;
                    return res;
                }
            }

            // Check possible combinations
            if(! (((iUnit == 1) && (iFormat == 1)) // S, MA
                    || ((iUnit == 1) && (iFormat == 2))  // S, RI
                    || ((iUnit == 1) && (iFormat == 3))  // S, DB
                    || ((iUnit == 2) && (iFormat == 2))  // Z, RI
                ))
            {
                return res;
            }

            continue;
        }

        if (parseCorrectionsLine(line, res.applied))
            continue;

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
            return res;
        }

        std::complex<double> s11c = RfMath::sparamFromFormat(iFormat, param1, param2);

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
        res.raw.append(data);
        res.fqMinMHz = qMin(res.fqMinMHz, data.fq);
        res.fqMaxMHz = qMax(res.fqMaxMHz, data.fq);

        // A 2-port Z-parameter file (Z, RI is a real, allowed
        // combination per the check above) puts Z21/Z12/Z22 in these
        // same 9 columns, not S21/S12/S22 -- a different physical
        // quantity (ohms, not a unitless ratio). #7: these used to be
        // silently skipped entirely rather than converted; now run
        // through RfMath::zToSParam()'s real Z-to-S 2-port matrix conversion,
        // same as s11c already is via the iUnit==2 branch above for
        // the RawData R/X. S-parameter files (iUnit==1) need no
        // conversion -- s11c/s21/s12/s22 are already S-parameters.
        if (lineIs2Port && (iUnit == 1))
        {
            SParamPoint sp;
            sp.fq = f*fqmul;
            sp.s11 = s11c;
            sp.s21 = RfMath::sparamFromFormat(iFormat, s21p1, s21p2);
            sp.s12 = RfMath::sparamFromFormat(iFormat, s12p1, s12p2);
            sp.s22 = RfMath::sparamFromFormat(iFormat, s22p1, s22p2);
            res.sparams.append(sp);
        }
        else if (lineIs2Port && (iUnit == 2))
        {
            std::complex<double> z21 = RfMath::sparamFromFormat(iFormat, s21p1, s21p2);
            std::complex<double> z12 = RfMath::sparamFromFormat(iFormat, s12p1, s12p2);
            std::complex<double> z22 = RfMath::sparamFromFormat(iFormat, s22p1, s22p2);
            res.sparams.append(RfMath::zToSParam(f*fqmul, s11c, z21, z12, z22, Z0));
        }
    }while (!line.isNull());
    res.error = ReadError::None;
    return res;
}

MeasurementFiles::ReadResult MeasurementFiles::readCsv(const QString& path)
{
    ReadResult res;
    QFile file(path);
    bool result = file.open(QFile::ReadOnly);
    if(!result)
        return res;

    QString str = file.readAll();
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
            res.raw.append(data);
            res.fqMinMHz = qMin(res.fqMinMHz, data.fq);
            res.fqMaxMHz = qMax(res.fqMaxMHz, data.fq);
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
            res.raw.append(data);
            res.fqMinMHz = qMin(res.fqMinMHz, data.fq);
            res.fqMaxMHz = qMax(res.fqMaxMHz, data.fq);
        }
    }
    res.error = ReadError::None;
    return res;
}

MeasurementFiles::ReadResult MeasurementFiles::readNwl(const QString& path)
{
    ReadResult res;
    QFile file(path);
    bool result = file.open(QFile::ReadOnly);
    if(!result)
        return res;

    QString str = file.readAll();

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
            res.raw.append(data);
            res.fqMinMHz = qMin(res.fqMinMHz, data.fq);
            res.fqMaxMHz = qMax(res.fqMaxMHz, data.fq);
        }
    }
    res.error = ReadError::None;
    return res;
}

bool MeasurementFiles::writeAsd(QString path, const QVector<RawData>& data, const Corrections& applied,
                                const QVector<RawData>& asReceived)
{
    // Was `if (path.indexOf(".asd") >= 0) { ... }` wrapping the whole
    // function -- a path without ".asd" in it (FileDialog::getSaveFileName()
    // doesn't call setDefaultSuffix(), so this isn't guaranteed) made
    // saving silently do nothing at all: no file, no warning. Append
    // the extension instead of refusing to save (matches RigExpert
    // AntScope2 2.0.3's fix, issue #10).
    if (path.indexOf(".asd") < 0)
        path += ".asd";

    QFile saveFile(path);

    if (!saveFile.open(QIODevice::WriteOnly))
    {
        qWarning("Couldn't open save file.");
        return false;
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
    // Points as shown -- what older readers use.
    mainObj["Measurements"] = measurementsArray;
    mainObj["Corrections"] = correctionsJson(applied);

    if (applied.any() && asReceived.size() == data.size()) {
        QJsonArray origArray;
        for (const RawData& p : asReceived) {
            QJsonObject obj;
            obj["fq"] = p.fq;
            obj["r"] = p.r;
            obj["x"] = p.x;
            origArray.append(obj);
        }
        mainObj["AsReceived"] = origArray;
    }

    QJsonDocument saveDoc(mainObj);

    saveFile.write(saveDoc.toJson());
    return true;
}

void MeasurementFiles::writeRaw(const QString& path, int type, const QVector<RawData>& data,
                                double z0, const QString& description, const Corrections& applied)
{
    int len = data.length();
    qInfo() << "Touchstone export:"
            << path
            << "points:"
            << data.size();
    if(path.indexOf(".s1p") >= 0 )
    {
        QFile file(path);

        if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))//if (!file.open(QFile::ReadWrite))
        {
            qInfo() << "Touchstone open failed:"
                    << path
                    << file.errorString();
            return;
        }

        QTextStream out(&file);

        out << "! Touchstone file generated by AntScopeZ";
        out << "\n";

        double Rswr = z0;

        if (type == 0) // Z, RI
        {
            out << "# MHz Z RI R " << Rswr << "\n";
            out << "! Format: Frequency Z-real Z-imaginary (normalized to " << Rswr << " Ohm)\n";
        }else if (type == 1) // S, RI
        {
            out << "# MHz S RI R " << Rswr << "\n";
            out << "! Format: Frequency S-real S-imaginary (normalized to " << Rswr << " Ohm)\n";
        }
        else if (type == 2) // S, MA
        {
            out << "# MHz S MA R " << Rswr << "\n";
            out << "! Format: Frequency S-magnitude S-angle (normalized to " << Rswr << " Ohm, angle in degrees)\n";
        }
        else if (type == 3) // S, DB
        {
            out << "# MHz S DB R " << Rswr << "\n";
            out << "! Format: Frequency S-magnitude(dB) S-angle (normalized to " << Rswr << " Ohm, angle in degrees)\n";
        }

        out << correctionsLine(applied) << "\n";
        if (!description.isEmpty())
            out << description << "\n";

        for (int i = 0; i < len; ++i)
        {
            QString s;

            s = QString("%1").arg(data.at(i).fq, 0, 'f', 6);		// Fq
            out << s << " ";

            double R = data.at(i).r;
            double X = data.at(i).x;

            if (type == 0) // Z, RI
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
            if (type == 1) // S, RI
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
            if (type == 2) // S, MA
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
            if (type == 3) // S, DB
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
    }else if(path.indexOf(".csv") >= 0 )
    {
        QString str;
        QFile file(path);
        bool result = file.open(QFile::ReadWrite);
        if(result)
        {
            str = "#Frequency(MHz);R;X";
            file.write( str.toLocal8Bit(), str.length());
            file.write("\r\n", 2);
            for (int i = 0; i < len; ++i)
            {
                str = QString::number(data.at(i).fq, 'f', 6) +
                "," +//";" +
                QString::number(data.at(i).r,'f',EXPORT_PRECISION) +
                "," +//";" +
                QString::number(data.at(i).x,'f',EXPORT_PRECISION);

                file.write( str.toLocal8Bit(), str.length());
                file.write("\r\n", 2);
            }
            file.close();
        }
    }else if(path.indexOf(".nwl") >= 0 )
    {
        QString str;
        QFile file(path);
        bool result = file.open(QFile::ReadWrite);
        if(result)
        {
            str = "/\"Freq(MHz)\" \"Rs\" \"Xs\"/";
            file.write( str.toLocal8Bit(), str.length());
            file.write("\r\n", 2);

            for (int i = 0; i < len; ++i)
            {
                str = QString::number(data.at(i).fq, 'f', 6) +
                " " +//";" +
                QString::number(data.at(i).r,'f',2) +
                " " +//";" +
                QString::number(data.at(i).x,'f',2);

                file.write( str.toLocal8Bit(), str.length());
                file.write("\r\n", 2);
            }
            file.close();
        }
    }
}

// 2-port Touchstone export. Unlike writeRaw()'s S,RI/S,MA paths, this
// needs no R/X -> Gamma conversion -- dataSParam already holds the raw
// complex S-parameters exactly as parsed off the original file's option
// line (RfMath::sparamFromFormat()), so this is a straight passthrough. That also
// means there's no real reference impedance to round-trip: the original
// file's own "R <value>" is never stored on the measurement (only used
// transiently, at import, to derive the R/X-based graphs). "R 50" here is
// just the near-universal RF convention, same fallback the app uses for 1-port
// export when no calibration Z0 is available -- not a claim about what the
// source file actually said.
bool MeasurementFiles::writeTouchstone2Port(const QString& path, int type, const QList<SParamPoint>& points,
                                            const QString& description)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
    {
        qInfo() << "Touchstone (2-port) open failed:" << path << file.errorString();
        return false;
    }

    // type: 0 = RI (real/imaginary), 1 = MA (magnitude/angle), 2 = DB
    // (magnitude in dB/angle) -- same three choices exportData() offers for
    // 1-port S-parameter export (no Z here -- Z would mean Z21/Z12/Z22, a
    // different quantity dataSParam never holds; see the Z-parameter
    // 2-port import guard's own comment).
    QString formatToken = (type == 2) ? "DB" : (type == 1) ? "MA" : "RI";
    auto formatPair = [type](std::complex<double> v) -> QString {
        double a, b;
        if (type == 2) { // DB
            a = 20*log10(std::abs(v));
            b = std::arg(v)*180.0/M_PI;
        } else if (type == 1) { // MA
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
    if (type == 2)
        out << "! Format: Frequency S11-mag(dB) S11-angle S21-mag(dB) S21-angle S12-mag(dB) S12-angle S22-mag(dB) S22-angle\n";
    else if (type == 1)
        out << "! Format: Frequency S11-mag S11-angle S21-mag S21-angle S12-mag S12-angle S22-mag S22-angle\n";
    else
        out << "! Format: Frequency S11-real S11-imag S21-real S21-imag S12-real S12-imag S22-real S22-imag\n";
    if (!description.isEmpty())
        out << description << "\n";

    foreach (const SParamPoint& sp, points) {
        out << QString::number(sp.fq, 'f', 6) << " "
            << formatPair(sp.s11) << " " << formatPair(sp.s21) << " "
            << formatPair(sp.s12) << " " << formatPair(sp.s22) << "\n";
    }
    out.flush();
    return true;
}

QString MeasurementFiles::nameFromPath(const QString& path)
{
    QStringList list = path.split("/");
    if (list.length() == 1)
        list = path.split("\\");
    return list.last();
}
