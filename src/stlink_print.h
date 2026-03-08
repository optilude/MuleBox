#pragma once

#include <stdio.h>
#include <stdarg.h>
#include "daisy_core.h"

// Minimal standalone semihosting print function (works without USB or Makefile changes)
// Prints a string over the ST-Link debug probe
static inline void PrintSTLink(const char* format, ...) {
    // Only execute if a debugger is connected to avoid HardFaults when running standalone!
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0) {
        return;
    }

    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Trigger ARM Semihosting SYS_WRITE0 (0x04)
    register uint32_t r0 asm("r0") = 0x04;
    register const char* r1 asm("r1") = buffer;
    __asm__ volatile (
        "bkpt 0xab\n"
        : "+r" (r0)
        : "r" (r1)
        : "memory"
    );
}
