#pragma once
namespace idw::daw {
// A finished export reports an explicit outcome so callers never infer
// cancellation from the text of an error message.
enum class ExportOutcome { completed, canceled, failed };
// Alert presentation is kept apart from the UI so the title of every outcome
// can be checked without opening a window.
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
// Returned by the renderers. Converting to bool means "completed", so existing
// success checks keep working, while cancellation stays distinguishable by value.
struct ExportResult {
    ExportOutcome outcome;
    ExportResult(ExportOutcome value):outcome(value){}
    operator bool() const {return outcome==ExportOutcome::completed;}
};
}
