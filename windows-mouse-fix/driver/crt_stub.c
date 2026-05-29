/*
 * crt_stub.c — Minimal CRT and runtime stubs for UMDF2 driver.
 * Provides memset/memcpy, CFG stubs, and WDF version globals.
 */
#include <stddef.h>

/* ---- Memory functions ---- */
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
    unsigned char* p = (unsigned char*)dest;
    while (count--) *p++ = (unsigned char)c;
    return dest;
}

#pragma function(memcpy)
void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (count--) *d++ = *s++;
    return dest;
}

#pragma function(memmove)
void* memmove(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (count--) *d++ = *s++;
    } else {
        d += count; s += count;
        while (count--) *--d = *--s;
    }
    return dest;
}

/* ---- Security cookie ---- */
unsigned __int64 __security_cookie = 0x00002B992DDFA232ULL;
void __cdecl __security_check_cookie(unsigned __int64 c) { (void)c; }

/* ---- Control Flow Guard stubs ---- */
static void __cdecl _guard_check_icall_nop_impl(void) { }
void (__cdecl *__guard_check_icall_fptr)(void) = _guard_check_icall_nop_impl;
void (__cdecl *__guard_dispatch_icall_fptr)(void) = _guard_check_icall_nop_impl;
void (__cdecl *__guard_xfg_check_icall_fptr)(void) = _guard_check_icall_nop_impl;
void (__cdecl *__guard_xfg_dispatch_icall_fptr)(void) = _guard_check_icall_nop_impl;
void (__cdecl *__guard_xfg_table_dispatch_icall_fptr)(void) = _guard_check_icall_nop_impl;
void (__cdecl *__castguard_check_failure_os_handled_fptr)(void) = _guard_check_icall_nop_impl;

/* ---- WDF version requirement ---- */
/* WdfMinimumVersionRequired is defined by WDF headers via wdf.h */
