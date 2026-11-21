// bo3-audio-fix: a pass-through winmm.dll for Call of Duty: Black Ops III.
//
// Black Ops III keeps the legacy WINMM sound mixers in a fixed table at
// startup. With more than 32 mixers on the system (one per playback or
// recording device) the game hangs on a black screen with sound. This DLL sits
// next to BlackOps3.exe, forwards every winmm export to the real System32
// winmm.dll, and caps what mixerGetNumDevs reports.
//
// Optional bo3_audio_fix.ini next to the DLL:
//   [fix]
//   MaxMixers=32   ; default. 0 also works and stops the game touching mic volume.
//                  ; -1 passes the real count through (fix off).
// Built with /DBO3FIX_DEBUG it also logs every mixer call to bo3_audio_fix.log.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include "exports.h"

#define DEFAULT_MAX_MIXERS 32

void* g_real[EXPORT_COUNT];   // read by the jump stubs in stubs.asm
static int g_maxMixers = DEFAULT_MAX_MIXERS;

static void* real(const char* name) {
    for (int i = 0; i < EXPORT_COUNT; i++)
        if ((ULONG_PTR)g_names[i] > 0xFFFF && strcmp(g_names[i], name) == 0) return g_real[i];
    return NULL;
}

#ifdef BO3FIX_DEBUG
#include <stdarg.h>
#include <share.h>
static FILE* g_f;
static int g_lines;
static CRITICAL_SECTION g_cs;
static LARGE_INTEGER g_freq, g_t0;
static double now_ms(void) {
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_t0.QuadPart) * 1000.0 / (double)g_freq.QuadPart;
}
static void logf_(const char* fmt, ...) {
    if (!g_f || g_lines > 5000) return;
    EnterCriticalSection(&g_cs);
    va_list ap; va_start(ap, fmt);
    fprintf(g_f, "[%9.1f ms] [tid %5lu] ", now_ms(), GetCurrentThreadId());
    vfprintf(g_f, fmt, ap); fputc('\n', g_f); fflush(g_f);
    va_end(ap); g_lines++;
    LeaveCriticalSection(&g_cs);
}
#else
#define logf_(...) ((void)0)
#endif

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);

    char path[MAX_PATH];
    if (!GetSystemDirectoryA(path, MAX_PATH)) return FALSE;
    strcat_s(path, MAX_PATH, "\\winmm.dll");
    HMODULE sys = LoadLibraryA(path);
    if (!sys) return FALSE;
    for (int i = 0; i < EXPORT_COUNT; i++) g_real[i] = (void*)GetProcAddress(sys, g_names[i]);

    char dir[MAX_PATH], ini[MAX_PATH];
    GetModuleFileNameA(h, dir, MAX_PATH);
    char* s = strrchr(dir, '\\'); if (s) *s = 0;
    snprintf(ini, MAX_PATH, "%s\\bo3_audio_fix.ini", dir);
    g_maxMixers = GetPrivateProfileIntA("fix", "MaxMixers", DEFAULT_MAX_MIXERS, ini);

#ifdef BO3FIX_DEBUG
    InitializeCriticalSection(&g_cs);
    QueryPerformanceFrequency(&g_freq); QueryPerformanceCounter(&g_t0);
    char log[MAX_PATH];
    snprintf(log, MAX_PATH, "%s\\bo3_audio_fix.log", dir);
    g_f = _fsopen(log, "w", _SH_DENYWR);
    logf_("bo3-audio-fix (debug) loaded; MaxMixers=%d", g_maxMixers);
#endif
    return TRUE;
}

UINT WINAPI Hook_mixerGetNumDevs(void) {
    UINT n = ((UINT (WINAPI*)(void))real("mixerGetNumDevs"))();
    UINT out = (g_maxMixers >= 0 && n > (UINT)g_maxMixers) ? (UINT)g_maxMixers : n;
    logf_("mixerGetNumDevs real=%u returned=%u", n, out);
    return out;
}

#ifdef BO3FIX_DEBUG
UINT WINAPI Hook_waveInGetNumDevs(void) {
    UINT n = ((UINT (WINAPI*)(void))real("waveInGetNumDevs"))();
    logf_("waveInGetNumDevs -> %u", n);
    return n;
}
MMRESULT WINAPI Hook_mixerOpen(LPHMIXER ph, UINT id, DWORD_PTR cb, DWORD_PTR inst, DWORD fl) {
    MMRESULT m = ((MMRESULT (WINAPI*)(LPHMIXER, UINT, DWORD_PTR, DWORD_PTR, DWORD))real("mixerOpen"))(ph, id, cb, inst, fl);
    logf_("mixerOpen id=%u -> %u", id, m);
    return m;
}
MMRESULT WINAPI Hook_mixerGetDevCapsA(UINT_PTR id, LPMIXERCAPSA caps, UINT cb) {
    MMRESULT m = ((MMRESULT (WINAPI*)(UINT_PTR, LPMIXERCAPSA, UINT))real("mixerGetDevCapsA"))(id, caps, cb);
    logf_("mixerGetDevCapsA id=%llu -> %u name=\"%s\"", (unsigned long long)id, m, (m == 0 && caps) ? caps->szPname : "");
    return m;
}
MMRESULT WINAPI Hook_mixerGetLineInfoA(HMIXEROBJ h, LPMIXERLINEA li, DWORD fl) {
    MMRESULT m = ((MMRESULT (WINAPI*)(HMIXEROBJ, LPMIXERLINEA, DWORD))real("mixerGetLineInfoA"))(h, li, fl);
    logf_("mixerGetLineInfoA flags=0x%lx -> %u line=\"%s\"", fl, m, (m == 0 && li) ? li->szName : "");
    return m;
}
MMRESULT WINAPI Hook_mixerGetLineControlsA(HMIXEROBJ h, LPMIXERLINECONTROLSA lc, DWORD fl) {
    MMRESULT m = ((MMRESULT (WINAPI*)(HMIXEROBJ, LPMIXERLINECONTROLSA, DWORD))real("mixerGetLineControlsA"))(h, lc, fl);
    logf_("mixerGetLineControlsA flags=0x%lx -> %u", fl, m);
    return m;
}
MMRESULT WINAPI Hook_mixerGetControlDetailsA(HMIXEROBJ h, LPMIXERCONTROLDETAILS d, DWORD fl) {
    MMRESULT m = ((MMRESULT (WINAPI*)(HMIXEROBJ, LPMIXERCONTROLDETAILS, DWORD))real("mixerGetControlDetailsA"))(h, d, fl);
    logf_("mixerGetControlDetailsA ctrl=%lu -> %u", d ? d->dwControlID : 0, m);
    return m;
}
MMRESULT WINAPI Hook_mixerSetControlDetails(HMIXEROBJ h, LPMIXERCONTROLDETAILS d, DWORD fl) {
    MMRESULT m = ((MMRESULT (WINAPI*)(HMIXEROBJ, LPMIXERCONTROLDETAILS, DWORD))real("mixerSetControlDetails"))(h, d, fl);
    logf_("mixerSetControlDetails ctrl=%lu -> %u", d ? d->dwControlID : 0, m);
    return m;
}
#endif
