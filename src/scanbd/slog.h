/*
 * $Id: slog.h 203 2015-02-04 08:05:20Z wimalopaan $
 *
 *  scanbd - KMUX scanner button daemon
 *
 *  Copyright (C) 2008 - 2015  Wilhelm Meier (wilhelm.meier@fh-kl.de)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef SLOG_H
#define SLOG_H

#include "common.h"

#define SLOG_ERROR 1
#define SLOG_WARN  2
#define SLOG_INFO  3
#define SLOG_DEBUG 4

extern bool debug;
extern unsigned int debug_level;

void slog(unsigned int level, const char *format, ...);
void slog_init(const char *string);

#endif
