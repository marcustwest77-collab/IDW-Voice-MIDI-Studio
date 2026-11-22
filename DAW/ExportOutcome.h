#pragma once
namespace idw::daw {
// A finished export reports an explicit outcome so callers never infer
// cancellation from the text of an error message.
enum class ExportOutcome { completed, canceled, failed };
// Alert presentation is kept apart from the UI so the title and icon of every
// outcome can be checked without opening a window.
enum class ExportMediaKind { wav, midi };
enum class ExportAlertIcon { info, warning };
struct ExportAlertPlan { ExportAlertIcon icon; const char* title; };
inline ExportAlertPlan exportAlertPlan(ExportMediaKind media,ExportOutcome outcome){
    const bool wav=media==ExportMediaKind::wav;
    if(outcome==ExportOutcome::completed)
        return {ExportAlertIcon::info,wav?"WAV export complete":"MIDI export complete"};
    if(outcome==ExportOutcome::canceled)
        return {ExportAlertIcon::info,wav?"WAV export canceled":"MIDI export canceled"};
    return {ExportAlertIcon::warning,wav?"WAV bounce unavailable":"MIDI export unavailable"};
}
// Returned by the renderers. It converts to bool in conditions so existing
// success checks keep working, but cancellation is distinguishable by value.
struct ExportResult {
    ExportOutcome outcome=ExportOutcome::failed;
    ExportResult()=default;
    ExportResult(ExportOutcome value):outcome(value){}
    bool completed() const {return outcome==ExportOutcome::completed;}
    explicit operator bool() const {return completed();}
    bool operator==(ExportOutcome other) const {return outcome==other;}
    bool operator!=(ExportOutcome other) const {return outcome!=other;}
};
}
