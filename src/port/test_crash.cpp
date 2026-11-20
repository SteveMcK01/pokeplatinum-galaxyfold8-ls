// Crash and hang backtraces for desktop test runs and Android builds (port/test_script.h).
// C++ for POSIX's signal API: the game's C files build as C99.

#include "port/test_script.h"

#if defined(__linux__)

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unwind.h>

#if defined(__ANDROID__)
#include <android/log.h>
#else
extern char __executable_start;
#endif

static pthread_t sGameThread;
static volatile sig_atomic_t sExitExpected;
static volatile sig_atomic_t sInstalled;

void TestScript_ExitExpected(void)
{
    sExitExpected = 1;
}

static uintptr_t ImageBase(void)
{
#if defined(__ANDROID__)
    Dl_info info;
    if (dladdr((const void *)&TestScript_InstallCrashHandlers, &info) != 0 && info.dli_fbase != nullptr) {
        return (uintptr_t)info.dli_fbase;
    }
    return 0;
#else
    return (uintptr_t)&__executable_start;
#endif
}

static void WriteCrashLine(int logFd, const char *line, int length)
{
    if (length <= 0) {
        return;
    }
    write(STDOUT_FILENO, line, length);
    if (logFd >= 0) {
        write(logFd, line, length);
    }
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_FATAL, "PokePlatinumCrash", "%s", line);
#endif
}

struct BacktraceState {
    void **current;
    void **end;
};

static _Unwind_Reason_Code UnwindCallback(struct _Unwind_Context *context, void *arg)
{
    BacktraceState *state = static_cast<BacktraceState *>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc != 0) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        }
        *state->current++ = reinterpret_cast<void *>(pc);
    }
    return _URC_NO_REASON;
}

static int CaptureBacktrace(void **buffer, int maxFrames)
{
    BacktraceState state = { buffer, buffer + maxFrames };
    _Unwind_Backtrace(UnwindCallback, &state);
    return static_cast<int>(state.current - buffer);
}

static void PrintBacktrace(const char *what, int logFd)
{
    const uintptr_t base = ImageBase();
    char line[160];
    int length = snprintf(line, sizeof(line), "[crash] %s, base %#lx\n", what, (unsigned long)base);
    WriteCrashLine(logFd, line, length);
    void *frames[64];
    const int count = CaptureBacktrace(frames, 64);
    for (int i = 0; i < count; i++) {
        Dl_info info;
        if (dladdr(frames[i], &info) != 0 && info.dli_sname != nullptr) {
            length = snprintf(line, sizeof(line), "[crash] frame #%02d %#lx (%s+%#lx)\n", i,
                (unsigned long)((uintptr_t)frames[i] - base), info.dli_sname,
                (unsigned long)((uintptr_t)frames[i] - (uintptr_t)info.dli_saddr));
        } else {
            length = snprintf(line, sizeof(line), "[crash] frame #%02d %#lx\n", i,
                (unsigned long)((uintptr_t)frames[i] - base));
        }
        WriteCrashLine(logFd, line, length);
    }
}

static void OnCrash(int number, siginfo_t *info, void *context)
{
    int logFd = -1;
#if defined(__ANDROID__)
    logFd = open("crash.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
#endif
    char what[48];
    snprintf(what, sizeof(what), "CRASH signal %d (addr %p)", number, info != nullptr ? info->si_addr : nullptr);
#if defined(__x86_64__)
    {
        const ucontext_t *early = (const ucontext_t *)context;
        const uintptr_t base = ImageBase();
        char line[160];
        int length = snprintf(line, sizeof(line), "[test] CRASH signal %d at %#lx, base %#lx, address %p\n", number,
                              (unsigned long)(early->uc_mcontext.gregs[REG_RIP] - base),
                              (unsigned long)base, info != nullptr ? info->si_addr : nullptr);
        WriteCrashLine(logFd, line, length);
    }
#elif defined(__aarch64__)
    {
        const ucontext_t *early = (const ucontext_t *)context;
        const uintptr_t base = ImageBase();
        char line[192];
        int length = snprintf(line, sizeof(line), "[crash] CRASH signal %d pc=%#lx lr=%#lx base=%#lx addr=%p\n", number,
                              (unsigned long)(early->uc_mcontext.pc - base),
                              (unsigned long)(early->uc_mcontext.regs[30] - base),
                              (unsigned long)base, info != nullptr ? info->si_addr : nullptr);
        WriteCrashLine(logFd, line, length);
    }
#else
    (void)context;
#endif
    PrintBacktrace(what, logFd);
    if (logFd >= 0) {
        fsync(logFd);
        close(logFd);
    }
    _exit(128 + number);
}

#if !defined(__ANDROID__)
static void OnHang(int number)
{
    if (!pthread_equal(pthread_self(), sGameThread)) {
        pthread_kill(sGameThread, number);
        return;
    }
    PrintBacktrace("HANG", -1);
    _exit(128 + number);
}

static void OnExit(void)
{
    if (!sExitExpected) {
        PrintBacktrace("EXIT", -1);
    }
}
#endif

void TestScript_InstallCrashHandlers(void)
{
    if (sInstalled) {
        return;
    }
    sInstalled = 1;
    sGameThread = pthread_self();
#if !defined(__ANDROID__)
    atexit(OnExit);
#endif
    static char signalStack[1 << 16];
    stack_t stack = {};
    stack.ss_sp = signalStack;
    stack.ss_size = sizeof(signalStack);
    sigaltstack(&stack, nullptr);

    struct sigaction action = {};
#if !defined(__ANDROID__)
    action.sa_flags = SA_ONSTACK | SA_RESTART;
    action.sa_handler = OnHang;
    sigaction(SIGQUIT, &action, nullptr);
#endif
    action.sa_flags = SA_ONSTACK | SA_RESTART | SA_SIGINFO;
    action.sa_sigaction = OnCrash;
    const int crashes[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
    for (int number : crashes) {
        sigaction(number, &action, nullptr);
    }
}

#else

void TestScript_ExitExpected(void)
{
}

void TestScript_InstallCrashHandlers(void)
{
}

#endif
