#include <cstdio>
#include <csignal>
#include <iostream>

#include <boost/stacktrace.hpp>

#include <test/SignalToStacktraceScope.h>

#if !NDEBUG && !BOOST_OS_WINDOWS
    typedef void (*FSignalHandler)(int signum);
    static void stacktraceHandler(int signum, const char* signame, FSignalHandler oldHandler) {
        fprintf(stderr, "\n\n===========================================\nReceived signal %s\n", signame);
        fflush(stderr); // flush before stack trace in case getting the stack trace itself produces another segfault
        std::cerr << boost::stacktrace::stacktrace();
        fprintf(stderr, "===========================================\n\n");
        fflush(stderr);

        // then raise the signal again to go back to the test framework
        signal(signum, oldHandler);
        raise(signum);
    }

    #define FOREACH_SIGNAL(F) \
        F(SIGSEGV) \
        F(SIGFPE) \
        F(SIGABRT) \
        F(SIGTERM)

    #define DEFINE_SIGNAL_WRAPPER(S) \
        static FSignalHandler oldHandler_##S = nullptr; \
        static void stacktraceHandler_##S(int signum) \
        { \
            stacktraceHandler(signum, #S, oldHandler_##S); \
        }
    FOREACH_SIGNAL(DEFINE_SIGNAL_WRAPPER)
    #undef DEFINE_SIGNAL_WRAPPER

    SignalToStacktraceScope::SignalToStacktraceScope() {
        #define HOOK_SIGNAL_WRAPPER(S) oldHandler_##S = signal(S, &stacktraceHandler_##S);
        FOREACH_SIGNAL(HOOK_SIGNAL_WRAPPER)
        #undef HOOK_SIGNAL_WRAPPER
    }

    SignalToStacktraceScope::~SignalToStacktraceScope() {
        #define UNHOOK_SIGNAL_WRAPPER(S) { signal(S, oldHandler_##S); oldHandler_##S = nullptr; }
        FOREACH_SIGNAL(UNHOOK_SIGNAL_WRAPPER)
        #undef UNHOOK_SIGNAL_WRAPPER
    }

    #undef FOREACH_SIGNAL
#else
    SignalToStacktraceScope::SignalToStacktraceScope() { }
    SignalToStacktraceScope::~SignalToStacktraceScope() { }
#endif
