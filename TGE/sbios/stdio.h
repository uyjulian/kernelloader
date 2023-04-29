/* Copyright (c) 2007 - 2012 Mega Man */
#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>

#ifdef SIO_DEBUG
void sio_puts(const char *b);
int sio_putc(int c);
#endif
int printf(const char *format, ...);
int puts(const char *buffer);
int snprintf(char * restrict str, size_t size, const char * restrict format, ...);
int vsnprintf(char *b, int len, const char *fmt, va_list pvar);

#ifdef CALLBACK_DEBUG
typedef void callback_prints_t(const char *text);
extern callback_prints_t *callback_prints;
#endif


#endif
