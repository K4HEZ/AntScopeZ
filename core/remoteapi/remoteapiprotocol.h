#ifndef REMOTEAPIPROTOCOL_H
#define REMOTEAPIPROTOCOL_H

#include <complex>
#include <limits>

// Shared NDJSON wire-protocol constants for RemoteApiServer/
// RemoteApiConnection -- one JSON object per line, both directions. See
// the json-tcp-api branch's plan doc for the full command/response
// reference; this header is just the single source of truth for the
// literal strings so command handlers (spread across remoteapiconnection.cpp
// as this grows) can't drift from each other via a typo.

namespace RemoteApiProtocol {

// Request "cmd" values (client -> server).
constexpr const char* CMD_STATUS = "status";
constexpr const char* CMD_DEVICES = "devices";
constexpr const char* CMD_CONNECT = "connect";
constexpr const char* CMD_DISCONNECT = "disconnect";
constexpr const char* CMD_SWEEP = "sweep";
constexpr const char* CMD_STOP = "stop";
constexpr const char* CMD_SUBSCRIBE = "subscribe";
constexpr const char* CMD_UNSUBSCRIBE = "unsubscribe";
constexpr const char* CMD_LAST = "last";

// Common JSON keys, both directions.
constexpr const char* KEY_CMD = "cmd";
constexpr const char* KEY_ID = "id";
constexpr const char* KEY_OK = "ok";
constexpr const char* KEY_ERROR = "error";
// Named KEY_EVENT_NAME, not KEY_EVENT -- windows.h's wincon.h #defines
// KEY_EVENT to 1 (a console INPUT_RECORD.EventType constant), which
// silently mangles any other identifier named exactly that anywhere it's
// transitively included (this repo's devinfo/redeviceinfo.h -> ftdi/
// ftdiinfo.h -> <windows.h> chain reaches it even from files that never
// include windows.h themselves) -- confirmed by the actual compiler error
// this produced before the rename.
constexpr const char* KEY_EVENT_NAME = "event";

// UI kHz -> AnalyzerPro::on_measure() Hz -> RawData/SParamPoint MHz is the
// real unit chain in this codebase (confirmed by tracing
// MainWindow::on_singleStart_clicked() -> AnalyzerPro::on_measure() ->
// analyzer/analyzerparameters.h's own RawData/SParamPoint comments), not
// assumed. This API picks Hz on the wire (matching the deleted Python
// daemon's freq_hz field). Applied at exactly one boundary -- converting a
// RawData/SParamPoint's MHz fq into this API's freq_hz -- never inlined as
// a bare *1e6 at another call site. This is the same class of bug that
// broke the abandoned OneFqWidget UDP bridge (a silent MHz/kHz/Hz mismatch
// between onefqwidget.cpp and measurements_onefq.cpp).
constexpr double MHZ_TO_HZ = 1'000'000.0;

// 50-ohm-reference reflection-coefficient/impedance/SWR math, needed
// because RawData (HID/Serial devices) already carries R/X directly while
// SParamPoint (NanoVNA) only carries the complex reflection coefficient
// (s11) -- converting both into one common point shape (see
// RemoteApiConnection's point-building code) needs both directions.
// Verified against the same reference points the deleted Python daemon's
// test suite used: matched load (Gamma=0 -> Z=Z0, SWR=1), open circuit
// (Gamma=1 -> SWR=infinity), short circuit (Gamma=-1, |Gamma|=1 -> SWR=
// infinity too -- SWR depends only on |Gamma|, not its phase).
inline std::complex<double> reflectionFromImpedance(std::complex<double> z, double z0 = 50.0)
{
    return (z - z0) / (z + z0);
}

inline std::complex<double> impedanceFromReflection(std::complex<double> gamma, double z0 = 50.0)
{
    return z0 * (1.0 + gamma) / (1.0 - gamma);
}

inline double swrFromReflectionMagnitude(double gammaMagnitude)
{
    if (gammaMagnitude >= 1.0)
        return std::numeric_limits<double>::infinity();
    return (1.0 + gammaMagnitude) / (1.0 - gammaMagnitude);
}

} // namespace RemoteApiProtocol

#endif // REMOTEAPIPROTOCOL_H
