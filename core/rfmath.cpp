#include "rfmath.h"
#include "calibration.h"
#include <math.h>

quint32 RfMath::computeSWR(double Z0, double R, double X, double *VSWR, double *RL)
{
    if (R <= 0)
    {
        R = 0.001;
    }
    double SWR, Gamma;
    double XX = X * X;								// always >= 0
    double denominator = (R + Z0) * (R + Z0) + XX;

    if (denominator == 0)
    {
        return 0;
    }
    Gamma = sqrt(((R - Z0) * (R - Z0) + XX) / denominator);
    if (Gamma == 1.0)
    {
        return 0;
    }
    SWR = (1 + Gamma) / (1 - Gamma);

    if ((SWR > 200) || (Gamma > 0.99))
    {
        SWR = 200;
    } else if (SWR < 1)
    {
        SWR = 1;
    }

    if (VSWR)
    {
        *VSWR = SWR;
    }
    if (RL)
    {
        if (Gamma == 0)
        {
            return 0;
        }
        *RL = -20 * log10(Gamma);
    }
    return 1;
}

double RfMath::computeZ(double R, double X)
{
    return sqrt((R*R) + (X*X));
}

void RfMath::parallel(double R, double X, double& Rpar, double& Xpar)
{
    if (qIsNaN(R) || (R<0.001))
        R = 0.01;
    if (qIsNaN(X))
        X = 0;
    Rpar = R*(1+X*X/R/R);
    Xpar = X*(1+R*R/X/X);
}

void RfMath::smithPoint(double Rnorm, double Xnorm, double &x, double &y)
{
    // Unlike the calibrated path a little further down in prepareGraphs()
    // (which explicitly guards qIsNaN(calR)/qIsNaN(calX) before using them),
    // this shared helper never validated its inputs. Defensive hardening,
    // not itself the confirmed crash site (2026-08-20 core dump traced that
    // to a separate pixelToCoord() misuse in measurements_onefq.cpp -- see
    // its own comment) -- but a NaN or Inf Rnorm/Xnorm (device data gone
    // bad upstream) or the Rnorm==-1 && Xnorm==0 degenerate case (Denom==0)
    // would propagate the same way into QCustomPlot's
    // coordToPixel/pixelToCoord, which Q_ASSERTs on NaN in a debug build.
    if (qIsNaN(Rnorm) || qIsNaN(Xnorm) || qIsInf(Rnorm) || qIsInf(Xnorm)) {
        x = 0;
        y = 0;
        return;
    }
    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    if (qFuzzyIsNull(Denom)) {
        x = -6; // Rnorm==-1, Xnorm==0 -- the reflection-coefficient edge closest to this input
        y = 0;
        return;
    }
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;

    x = RhoReal*6;// 6 - radius
    y = RhoImag*6;// 6 - radius
}

Complex RfMath::calibratedZ(double fq, double R, double X, double Z0, Calibration* calibration)
{
    double Gre = (R*R-Z0*Z0+X*X)/((R+Z0)*(R+Z0)+X*X);
    double Gim = (2*Z0*X)/((R+Z0)*(R+Z0)+X*X);

    double GreOut;
    double GimOut;

    double SOR =  1; double SOI = 0; // Ideal model
    double SSR = -1; double SSI = 0;
    double SLR =  0; double SLI = 0;

    double COR, COI; // CalibrationReOpen, CalibrationImOpen
    double CSR, CSI; // CalibrationReShort, CalibrationImShort
    double CLR, CLI; // CalibrationReLoad, CalibrationImLoad
    bool res = calibration->interpolateS(fq, COR, COI, CSR, CSI, CLR, CLI);
//            COR = 1;
//            COI = 0;
//            CSR = -1;
//            CSI = 0;
//            CLR = 0;
//            CLI = 0;

    if (!res)
    {
        SOR =  1; SOI = 0; // Ideal model
        SSR = -1; SSI = 0;
        SLR =  0; SLI = 0;
    }
    calibration->applyCalibration(Gre,Gim,  // Measured
                                    COR,COI,CSR,CSI,CLR,CLI, // Measured parameters of cal standards
                                    SOR,SOI,SSR,SSI,SLR,SLI, // Actual (Ideal) parameters of cal standards
                                    GreOut,GimOut); // Actual
    //-----------vnn_04 _2
    double chek_GreGim=sqrt((GreOut*GreOut)+(GimOut*GimOut));
    //1)   ((GreOut==1)&&(GimOut==0))
    //2)   (chek_GreGim>1)
    if( ((GreOut==1)&&(GimOut==0))||(chek_GreGim>1)){
        if((GreOut==1)&&(GimOut==0)){
            GreOut= 0.999999992;
        }else{
            double ncosA= GreOut/chek_GreGim;
            double nsinA= GimOut/chek_GreGim;
            GreOut=0.999999992*ncosA;
            GimOut=0.999999992*nsinA;
        }
    }

    double calR = (1-GreOut*GreOut-GimOut*GimOut)/((1-GreOut)*(1-GreOut)+GimOut*GimOut);
    calR *= Z0;
    double calX = (2*GimOut)/((1-GreOut)*(1-GreOut)+GimOut*GimOut);
    calX *= Z0;
    return Complex(calR, calX);
}

void RfMath::prepareGraphs(const RawData& _rawData, double Z0, Calibration* calibration,
                           GraphData& _data, GraphData& _calibData)
{
    _data.FQ = _rawData.fq;
    _data.R = _rawData.r;
    _data.X = _rawData.x;

    computeSWR(Z0,_data.R,_data.X,&_data.SWR,&_data.RL);
    _data.Z = computeZ(_data.R,_data.X);

    //------------------RXZ par-----------------------------------------------------
    double R = _rawData.r;
    double X = _rawData.x;
    if (qIsNaN(R) || (R<0.001) )
        R = 0.01;
    if (qIsNaN(X))
        X = 0;
    parallel(R, X, _data.Rpar, _data.Xpar);
    _data.Zpar = computeZ(_data.Rpar, _data.Xpar);

    //----------------------calc phase----------------------------------------------
    double Rnorm = R/Z0;
    double Xnorm = X/Z0;
    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;
    double RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
    double RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);
    _data.RhoPhase = RhoPhase;
    _data.RhoMod = RhoMod;

    //----------------------Calc calibration if performed---------------------------
    if(calibration != nullptr)
    {
        if(calibration->getCalibrationPerformed())
        {
            _calibData.FQ = _rawData.fq;
            R = _rawData.r;
            X = _rawData.x;
            Complex cal = calibratedZ(_rawData.fq, R, X, Z0, calibration);
            double calR = cal.real();
            double calX = cal.imag();
            double calZ = computeZ(calR,calX);

            _calibData.R = calR;
            _calibData.X = calX;
            _calibData.Z = calZ;
            computeSWR(Z0, calR, calX, &_calibData.SWR, &_calibData.RL);

            double calRpar, calXpar;
            parallel(calR, calX, calRpar, calXpar);

            _calibData.Rpar = calRpar;
            _calibData.Xpar = calXpar;
            _calibData.Zpar = computeZ(calRpar, calXpar);

            if (qIsNaN(calR) || (calR<0.001) )
                calR = 0.01;
            if (qIsNaN(calX))
                calX = 0;
            Rnorm = calR/Z0;
            Xnorm = calX/Z0;

            Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
            RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
            RhoImag = 2*Xnorm/Denom;

            RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
            RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);

            _calibData.RhoPhase = RhoPhase;
            _calibData.RhoMod = RhoMod;
        }
    }
    //----------------------calc smith-------------------------------
    double ptX,ptY;
    smithPoint(Rnorm, Xnorm, ptX, ptY);
    _data.ptX = ptX;
    _data.ptY = ptY;
}

Complex RfMath::cableTransform(double fq, double R, double X, const CableParams& cable, bool subtract)
{
    double Klen = 1;
    switch (cable.lossUnits)
    {
    case 0: Klen = 1; break;
    case 1: Klen = 1*100.0; break;
    case 2: Klen = 1/FEETINMETER; break;
    case 3: Klen = 1/FEETINMETER*100.0; break;
    }

    Complex Zload = Complex( R, X);

    double dMatchedLossDb;  // Note that K1/K2 are in dB/100 ft
    if(!cable.lossAtAnyFq)
        dMatchedLossDb = cable.lossConductive*Klen*sqrt(fq) + cable.lossDielectric*Klen*fq;
    else
        dMatchedLossDb = cable.lossConductive*Klen + cable.lossDielectric*Klen;


#define NEPER 8.68588963806504        // = 20 / Ln(10)

    double Alpha = dMatchedLossDb / 100.0 / NEPER; // Nepers (attenuation) per foot
    double Beta = (2*M_PI * fq) / (SPEEDOFLIGHT*FEETINMETER/1000000.0 * cable.velFactor); // Radians (phase constant) per foot

    double Alphal = Alpha * cable.lengthFeet;
    double Betal = Beta * cable.lengthFeet;

    if(subtract)
    {
        Alphal = -Alphal;
        Betal = -Betal;
    }
    // Was: an extra *FEETINMETER correction here, applied only when
    // cable.lossUnits==0 (dB/100feet, the default). Alpha/Beta are
    // already correctly "per foot" for every loss-unit choice (Klen
    // above already normalizes to that basis), and cable.lengthFeet is
    // *always* already in feet by the time it reaches here regardless of
    // which unit is displayed (Settings::getCableLength() converts at
    // the UI boundary) -- so this extra multiply threw the result off by
    // FEETINMETER (~3.28x) specifically for the default dropdown
    // selection, and only that one. Verified numerically: the other
    // three loss-unit choices already agreed with each other exactly:
    // removing this makes dB/100feet agree with them too, rather than
    // being the odd one out. See issue #31.

    Complex Sinh_gl = Complex( cos(Betal) * sinh(Alphal), sin(Betal) * cosh(Alphal) );
    Complex Cosh_gl = Complex( cos(Betal) * cosh(Alphal), sin(Betal) * sinh(Alphal) );

    Complex Zo = Complex(cable.resistance, -cable.resistance * (Alpha / Beta));

    Complex ZIZL = Zo * ( (Zload*Cosh_gl + Zo*Sinh_gl) /  (Zo*Cosh_gl + Zload*Sinh_gl) );

    R = ZIZL.real();
    if(R<0.0001)
        R = 0.0001;
    X = ZIZL.imag();
    return Complex(R, X);
}

std::complex<double> RfMath::sparamFromFormat(int iFormat, double v1, double v2)
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

SParamPoint RfMath::zToSParam(double fq, std::complex<double> z11,
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
double RfMath::unwrapPhaseDeg(double rawDeg, bool& havePrev, double& prevRaw, double& prevUnwrapped)
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
