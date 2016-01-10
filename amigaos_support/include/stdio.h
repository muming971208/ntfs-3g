/**
 * Copyright (c) 2015-2016 Fredrik Wikstrom <fredrik@a500.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef STDIO_H
#define STDIO_H

#include_next <stdio.h>

#include <stdarg.h>

int my_vprintf(const char *fmt, va_list args);
int my_printf(const char *fmt, ...);
int my_vfprintf(FILE *s, const char *fmt, va_list args);
int my_fprintf(FILE *s, const char *fmt, ...);

#define vprintf(fmt,args)      my_vprintf(fmt, args)
#define printf(fmt,args...)    my_printf(fmt, ## args)
#define vfprintf(s,fmt,args)   my_vfprintf(s, fmt, args)
#define fprintf(s,fmt,args...) my_fprintf(s, fmt, ## args)

#ifndef __AROS__
int my_vsnprintf(char *buffer, size_t size, const char *fmt, va_list arg);
int my_snprintf(char *buffer, size_t size, const char *fmt, ...);
#define vsnprintf(buffer,size,fmt,args)   my_vsnprintf(buffer, size, fmt, args)
#define snprintf(buffer,size,fmt,args...) my_snprintf(buffer, size, fmt, ## args)
#endif

#endif

