/*
 * $Id: daemonize.c 203 2015-02-04 08:05:20Z wimalopaan $
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

#include "common.h"
#include "slog.h"

void daemonize(void) {
    pid_t pid;

    if ((pid = fork()) < 0) {
        slog(SLOG_ERROR, "fork: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) { // Parent
        exit(EXIT_SUCCESS);
    }
    // Child
    if (setsid() < 0) {
        slog(SLOG_ERROR, "setsid: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if ((pid = fork()) < 0) {
        slog(SLOG_ERROR, "fork: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) { // Parent
        exit(EXIT_SUCCESS);
    }
    // Child-Child
    int ofd;
    if ((ofd = open("/dev/null", O_RDWR)) < 0) {
        slog(SLOG_ERROR, "open /dev/null: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (dup2(ofd, STDIN_FILENO) < 0) {
        slog(SLOG_ERROR, "dup2: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (dup2(ofd, STDOUT_FILENO) < 0) {
        slog(SLOG_ERROR, "dup2: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (dup2(ofd, STDERR_FILENO) < 0) {
        slog(SLOG_ERROR, "dup2: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (chdir("/") < 0) {
        slog(SLOG_ERROR, "chdir: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
