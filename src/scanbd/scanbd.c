/*
 * $Id: scanbd.c 203 2015-02-04 08:05:20Z wimalopaan $
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

#include "scanbd.h"

#ifdef USE_SCANBUTTOND
# include "scanbuttond_loader.h"
# include "scanbuttond_wrapper.h"
backend_t* backend = NULL;
#endif
#include <libgen.h>

cfg_t* cfg = NULL;

// the actual values of the command-line options
struct scanbdOptions scanbd_options = {
    /* managerMode */      false,
    /* foreground */       false,
    /* signal */	   false,
    /* config_file_name */ SCANBD_CONF
};

// the options for getopt_long()
static struct option options[] = {
    {"manager",    0, NULL, 'm'},
    {"signal",     0, NULL, 's'},
    {"debug",      2, NULL, 'd'},
    {"foreground", 0, NULL, 'f'},
    {"config",     1, NULL, 'c'},
    {"trigger",    1, NULL, 't'},
    {"action",     1, NULL, 'a'},
    { 0,           0, NULL, 0}
};

void sig_hup_handler(int signal) {
    slog(SLOG_DEBUG, "sig_hup_handler called");

    if (signal == SIGALRM) {
        slog(SLOG_INFO, "reconfiguration due to SIGALARM, device was busy?");
    }

    // stop all threads
#ifdef USE_SANE
    stop_sane_threads();
#else
    stop_scbtn_threads();
#endif

    slog(SLOG_DEBUG, "sane_exit");
#ifdef USE_SANE
    sane_exit();
#else
    scbtn_shutdown();
#endif

#ifdef SANE_REINIT_TIMEOUT
    sleep(SANE_REINIT_TIMEOUT); // TODO: don't know if this is
    // really neccessary
#endif
    slog(SLOG_DEBUG, "reread the config");
    cfg_do_parse(scanbd_options.config_file_name);

    cfg_t* cfg_sec_global = NULL;
    cfg_sec_global = cfg_getsec(cfg, C_GLOBAL);
    assert(cfg_sec_global);
    debug = cfg_getbool(cfg_sec_global, C_DEBUG);
    debug_level = cfg_getint(cfg_sec_global, C_DEBUG_LEVEL);

    slog(SLOG_DEBUG, "sane_init");
#ifdef USE_SANE
    sane_init(NULL, NULL);
#else
    if (scanbtnd_init() < 0) {
        slog(SLOG_INFO, "Could not initialize scanbuttond modules!\n");
        exit(EXIT_FAILURE);
    }
    assert(backend);

#endif

#ifdef USE_SANE
    get_sane_devices();
#else
    get_scbtn_devices();
#endif

    // start all threads
#ifdef USE_SANE
    start_sane_threads();
#else
    start_scbtn_threads();
#endif

}

void sig_usr1_handler(int signal) {
    slog(SLOG_DEBUG, "sig_usr1_handler called");
    (void)signal;
    // stop all threads
#ifdef USE_SANE
    stop_sane_threads();
#else
    stop_scbtn_threads();
#endif
}

void sig_usr2_handler(int signal) {
    slog(SLOG_DEBUG, "sig_usr2_handler called");
    (void)signal;
    // start all threads
#ifdef USE_SANE
    start_sane_threads();
#else
    start_scbtn_threads();
#endif
}

void sig_term_handler(int signal) {
    slog(SLOG_DEBUG, "sig_term/int_handler called with signal %d", signal);

    if (scanbd_options.managerMode) {
        // managerMode
        slog(SLOG_INFO, "managerMode: do nothing");
    }
    else {
        // not in manager-mode
        // stop all threads
#ifdef USE_SANE
        stop_sane_threads();
#else
        stop_scbtn_threads();
#endif
        dbus_stop_dbus_thread();

#ifdef USE_LIBUDEV
        udev_stop_udev_thread();
#endif
        // get the name of the pidfile
        const char* pidfile = NULL;
        cfg_t* cfg_sec_global = NULL;
        cfg_sec_global = cfg_getsec(cfg, C_GLOBAL);
        assert(cfg_sec_global);
        pidfile = cfg_getstr(cfg_sec_global, C_PIDFILE);
        assert(pidfile);

        if (!scanbd_options.foreground) {
            // reclaim the old uid (root) to unlink the pidfile
            // mostly neccessary if the pidfile lives in /var/run
            if (seteuid((pid_t)0) < 0) {
                slog(SLOG_WARN, "Can't acquire uid root to unlink pidfile %s : %s",
                     pidfile, strerror(errno));
                slog(SLOG_DEBUG, "euid: %d, egid: %d",
                     geteuid(), getegid());
                // not an hard error, since sometimes this isn't neccessary
            }
            if (unlink(pidfile) < 0) {
                slog(SLOG_ERROR, "Can't unlink pidfile: %s", strerror(errno));
                slog(SLOG_DEBUG, "euid: %d, egid: %d",
                     geteuid(), getegid());
                exit(EXIT_FAILURE);
            }
        }
    }
    slog(SLOG_INFO, "exiting scanbd");
    exit(EXIT_SUCCESS);
}

bool isNumber(const char* string) {
    if (!string) {
        return false;
    }
    while(*string) {
        if (!isdigit((int)*string)) {
            return false;
        }
        ++string;
    }
    return true;
}

int main(int argc, char** argv) {
    // init the logging feature
    slog_init(argv[0]);

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#ifdef USE_SANE
    sane_init_mutex();
#endif
#ifdef USE_SCANBUTTOND
    scbtn_init_mutex();
#endif
#endif

    // install all the signalhandlers early
    // SIGHUP rereads the config as usual
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_hup_handler;
    sigemptyset(&sa.sa_mask);
    // prevent interleaving with stop/start_sane_threads
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaddset(&sa.sa_mask, SIGUSR2);
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        slog(SLOG_ERROR, "Can't install signalhandler for SIGHUP: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
#ifdef USE_SCANBUTTOND
    // install the SIGHUP handler also for SIGALARM
    // SIGALARM is used if there is a open failure of the
    // scanbuutond backends, possible cause is a device scanning from other
    // processes like cupsd
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        slog(SLOG_ERROR, "Can't install signalhandler for SIGALARM: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
#endif

    // SIGUSR1 is used to stop all polling threads
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_usr1_handler;
    sigemptyset(&sa.sa_mask);
    // prevent interleaving the stop_sane_threads with start_sane_threads
    sigaddset(&sa.sa_mask, SIGUSR2);
    sigaddset(&sa.sa_mask, SIGHUP);
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        slog(SLOG_ERROR, "Can't install signalhandler for SIGUSR1: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // SIGUSR2 is used to restart all polling threads
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_usr2_handler;
    sigemptyset(&sa.sa_mask);
    // prevent interleaving the start_sane_threads with stop_sane_threads
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaddset(&sa.sa_mask, SIGHUP);
    if (sigaction(SIGUSR2, &sa, NULL) < 0) {
        slog(SLOG_ERROR, "Can't install signalhandler for SIGUSR2: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // SIGTERM and SIGINT terminates the process gracefully
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_term_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        slog(SLOG_ERROR, "Can't install signalhandler for SIGTERM: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        slog(SLOG_ERROR, "Can't install signalhandler for SIGINT: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int trigger_device = -1;
    int trigger_action = -1;

    // read the options of the commandline
    while(true) {
        int option_index = 0;
        int c = 0;
        if ((c = getopt_long(argc, argv, "mc:d::ft:a:", options, &option_index)) < 0) {
            break;
        }
        switch(c) {
        case 'm':
            slog(SLOG_INFO, "manager-mode");
            scanbd_options.managerMode = true;
            break;
        case 's':
            slog(SLOG_INFO, "signal-mode");
            scanbd_options.signal = true;
            break;
        case 'd':
            slog(SLOG_INFO, "debug on");
            debug = true;
            if (optarg) {
                if (isNumber(optarg)) {
                  debug_level = atoi(optarg);
                }
                else {
                    slog(SLOG_WARN, "use numerical argument for option -d");
                }
            }
            break;
        case 'f':
            slog(SLOG_INFO, "foreground");
            scanbd_options.foreground = true;
            break;
        case 'c':
            slog(SLOG_INFO, "config-file: %s", optarg);
            scanbd_options.config_file_name = strdup(optarg);
            break;
        case 't':
            slog(SLOG_INFO, "trigger for device number: %d", atoi(optarg));
            if (isNumber(optarg)) {
                trigger_device = atoi(optarg);
                scanbd_options.foreground = true;
            }
            else {
                slog(SLOG_WARN, "use numerical argument for option -t");
            }
            break;
        case 'a':
            slog(SLOG_INFO, "trigger action number: %d", atoi(optarg));
            if (isNumber(optarg)) {
                trigger_action = atoi(optarg);
                scanbd_options.foreground = true;
            }
            else {
                slog(SLOG_WARN, "use numerical argument for option -a");
            }
            break;
        default:
            break;
        }
    }

    // read & parse scanbd.conf
    cfg_do_parse(scanbd_options.config_file_name);

    cfg_t* cfg_sec_global = NULL;
    cfg_sec_global = cfg_getsec(cfg, C_GLOBAL);
    assert(cfg_sec_global);

    debug |= cfg_getbool(cfg_sec_global, C_DEBUG);

    if (debug_level == 0 ) {
        // not set from command-line
        debug_level = cfg_getint(cfg_sec_global, C_DEBUG_LEVEL);
    }

    // We do this here as debugging is only completely initialized here
    char prog_path[PATH_MAX] = "";
    strncpy(prog_path, argv[0], PATH_MAX); 
    char *my_name = basename(prog_path);
    if ( strncmp(my_name, NAME_MANAGER_MODE, PATH_MAX) == 0 ) {
        slog(SLOG_INFO, "We are called as %s - setting manager-mode", NAME_MANAGER_MODE);
        scanbd_options.managerMode = true;
    }
    
    if (debug) {
        slog(SLOG_INFO, "debug on: level: %d", debug_level);
    }
    else {
        slog(SLOG_INFO, "debug off");
    }

    // manager-mode in signal-mode
    // stops all polling threads in a running scanbd by sending
    // SIGUSR1
    // then starts saned
    // afterwards restarts all polling threads in a running scanbd by
    // sending SIGUSR2

    // manager-mode in dbus-mode
    // stops all polling threads in a running scanbd by calling
    // a dbus message of the running scanbd
    // then starts saned
    // afterwards restarts all polling threads in a running scanbd by
    // another dbus message

    // this is usefull for using scanbd in manager-mode from inetd
    // starting saned indirectly
    // in inetd.conf:
    //
    if (scanbd_options.managerMode) {
        slog(SLOG_DEBUG, "Entering manager mode");

        if ((trigger_device >= 0) && (trigger_action >= 0)) {
            slog(SLOG_DEBUG, "Entering trigger mode");
            dbus_call_trigger(trigger_device, trigger_action);
            exit(EXIT_SUCCESS);
        }

        pid_t scanbd_pid = -1;
        // get the name of the saned executable
        const char* saned = NULL;
        cfg_t* cfg_sec_global = NULL;
        cfg_sec_global = cfg_getsec(cfg, C_GLOBAL);
        assert(cfg_sec_global);
        saned = cfg_getstr(cfg_sec_global, C_SANED);
        assert(saned);

        if (scanbd_options.signal) {
            slog(SLOG_DEBUG, "manager mode: signal");
            // get the path of the pid-file of the running scanbd
            const char* scanbd_pid_file = NULL;
            scanbd_pid_file = cfg_getstr(cfg_sec_global, C_PIDFILE);
            assert(scanbd_pid_file);

            // get the pid of the running scanbd out of the pidfile
            FILE* pidfile;
            if ((pidfile = fopen(scanbd_pid_file, "r")) == NULL) {
                slog(SLOG_WARN, "Can't open pidfile %s", scanbd_pid_file);
            }
            else {
                char pida[NAME_MAX];
                if (fgets(pida, NAME_MAX, pidfile) == NULL) {
                    slog(SLOG_WARN, "Can't read pid from pidfile %s", scanbd_pid_file);
                }
                if (fclose(pidfile) < 0) {
                    slog(SLOG_WARN, "Can't close pidfile %s", scanbd_pid_file);
                }
                scanbd_pid = atoi(pida);
                slog(SLOG_DEBUG, "found scanbd with pid %d", scanbd_pid);
                // set scanbd to sleep mode
                slog(SLOG_DEBUG, "sending SIGUSR1", scanbd_pid);
                if (kill(scanbd_pid, SIGUSR1) < 0) {
                    slog(SLOG_WARN, "Can't send signal SIGUSR1 to pid %d: %s",
                         scanbd_pid, strerror(errno));
                    slog(SLOG_DEBUG, "uid=%d, euid=%d", getuid(), geteuid());
                    //exit(EXIT_FAILURE);
                }
            }
            // sleep some time to give the other scanbd to close all the
            // usb-connections
            sleep(1);
        } // signal-mode
        else {
            slog(SLOG_DEBUG, "dbus signal saned-start");
            dbus_send_signal(SCANBD_DBUS_SIGNAL_SANED_BEGIN, NULL);
            slog(SLOG_DEBUG, "manager mode: dbus");
            slog(SLOG_DEBUG, "calling dbus method: %s", SCANBD_DBUS_METHOD_ACQUIRE);
            dbus_call_method(SCANBD_DBUS_METHOD_ACQUIRE, NULL);
        }
        // start the real saned
        slog(SLOG_DEBUG, "forking subprocess for saned");
        pid_t spid = 0;
        if ((spid = fork()) < 0) {
            slog(SLOG_ERROR, "fork for saned subprocess failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (spid > 0) { // parent
            // wait for the saned process to finish
            int status = 0;
            slog(SLOG_DEBUG, "waiting for saned");
            if (waitpid(spid, &status, 0) < 0) {
                slog(SLOG_ERROR, "waiting for saned failed: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (WIFEXITED(status)) {
                slog(SLOG_INFO, "saned exited with status: %d", WEXITSTATUS(status));
            }
            if (WIFSIGNALED(status)) {
                slog(SLOG_INFO, "saned exited due to signal: %d", WTERMSIG(status));
            }
            // saned finished and now
            // reactivate scandb
            if (scanbd_options.signal) {
                // sleep some time to give the other scanbd to close all the
                // usb-connections
                sleep(1);
                if (scanbd_pid > 0) {
                    slog(SLOG_DEBUG, "sending SIGUSR2");
                    if (kill(scanbd_pid, SIGUSR2) < 0) {
                        slog(SLOG_INFO, "Can't send signal SIGUSR1 to pid %d: %s",
                             scanbd_pid, strerror(errno));
                    }
                }
            } // signal-mode
            else {
                slog(SLOG_DEBUG, "calling dbus method: %s", SCANBD_DBUS_METHOD_RELEASE);
                dbus_call_method(SCANBD_DBUS_METHOD_RELEASE, NULL);
                slog(SLOG_DEBUG, "dbus signal saned-end");
                dbus_send_signal(SCANBD_DBUS_SIGNAL_SANED_END, NULL);
            }
        }
        else { // child
            // Ensure that saned gets the systemd fd's
            // We do not call sd_listen_fds() as that sets FD_CLOEXEC on the fd's
            char listen_fds[64] = "";
            if (getenv("LISTEN_PID") != NULL) {
                snprintf(listen_fds, 64, "LISTEN_PID=%ld", (long) getpid());
                putenv(listen_fds);
                slog(SLOG_DEBUG, "Systemd detected: Updating LISTEN_PID env. variable");
            }
            size_t numberOfEnvs = cfg_size(cfg_sec_global, "saned_env");
            for(size_t i = 0; i < numberOfEnvs; i += 1) {
                const char* e = cfg_getnstr(cfg_sec_global, "saned_env", i);
                assert(e);
                if (putenv((char*)e) < 0) { // const-cast should not be neccessary
                    slog(SLOG_WARN, "Can't set environment %s: %s", e, strerror(errno));
                }
                else {
                    slog(SLOG_DEBUG, "Setting environment: %s", e);
                }
            }
            if (setsid() < 0) {
                slog(SLOG_WARN, "setsid: %s", strerror(errno));
            }
            if (execl(saned, "saned", (char*)NULL) < 0) {
                slog(SLOG_ERROR, "exec of saned failed: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS); // not reached
        }
        exit(EXIT_SUCCESS);
    }
    else { // not in manager mode

        // daemonize,
        if (!scanbd_options.foreground) {
            slog(SLOG_DEBUG, "daemonize");
            daemonize();
        }

        cfg_t* cfg_sec_global = NULL;
        cfg_sec_global = cfg_getsec(cfg, C_GLOBAL);
        assert(cfg_sec_global);

        // drop the privilegies
        const char* euser = NULL;
        euser = cfg_getstr(cfg_sec_global, C_USER);
        assert(euser);
        const char* egroup = NULL;
        egroup = cfg_getstr(cfg_sec_global, C_GROUP);
        assert(egroup);

        slog(SLOG_INFO, "dropping privs to uid %s", euser);
        struct passwd* pwd = NULL;
        if ((pwd = getpwnam(euser)) == NULL) {
            if (errno != 0) {
                slog(SLOG_ERROR, "No user %s: %s", euser, strerror(errno));
            }
            else {
                slog(SLOG_ERROR, "No user %s", euser);
            }
            exit(EXIT_FAILURE);
        }
        assert(pwd);

        slog(SLOG_INFO, "dropping privs to gid %s", egroup);
        struct group* grp = NULL;
        if ((grp = getgrnam(egroup)) == NULL) {
            if (errno != 0) {
                slog(SLOG_ERROR, "No group %s: %s", egroup, strerror(errno));
            }
            else {
                slog(SLOG_ERROR, "No group %s", egroup);
            }
            exit(EXIT_FAILURE);
        }
        assert(grp);

        // write pid file
        const char* pidfile = NULL;
        pidfile = cfg_getstr(cfg_sec_global, C_PIDFILE);
        assert(pidfile);

        if (!scanbd_options.foreground) {
            // don't try to write pid-file if forgrounded
            int pid_fd = 0;
            if ((pid_fd = open(pidfile, O_RDWR | O_CREAT | O_EXCL,
                               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
                slog(SLOG_ERROR, "Can't create pidfile %s : %s", pidfile, strerror(errno));
                exit(EXIT_FAILURE);
            }
            else {
                if (ftruncate(pid_fd, 0) < 0) {
                    slog(SLOG_ERROR, "Can't clear pidfile: %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                char buf[LINE_MAX];
                snprintf(buf, LINE_MAX - 1, "%d\n", getpid());
                if (write(pid_fd, buf, strlen(buf)) < 0) {
                    slog(SLOG_ERROR, "Can't write pidfile: %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if (close(pid_fd)) {
                    slog(SLOG_ERROR, "Can't close pidfile: %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if (chown(pidfile, pwd->pw_uid, grp->gr_gid) < 0) {
                    slog(SLOG_ERROR, "Can't chown pidfile: %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
        }

        // drop the privileges
        // first change our effective gid
        if (grp != NULL) {
            int i = 0;
            while(grp->gr_mem[i] != 0) {
                if (i == 0) {
                    slog(SLOG_INFO, "group %s has member: ", grp->gr_name);
                }
                slog(SLOG_INFO, "%s", grp->gr_mem[i]);
                ++i;
            }
            slog(SLOG_DEBUG, "drop privileges to gid: %d", grp->gr_gid);
            if (setegid(grp->gr_gid) < 0) {
                slog(SLOG_WARN, "Can't set the effective gid to %d", grp->gr_gid);
            }
            else {
                slog(SLOG_INFO, "Running as effective gid %d", grp->gr_gid);
            }
        }
        // then change our effective uid
        if (pwd != NULL) {
            slog(SLOG_DEBUG, "drop privileges to uid: %d", pwd->pw_uid);
            if (seteuid(pwd->pw_uid) < 0) {
                slog(SLOG_WARN, "Can't set the effective uid to %d", pwd->pw_uid);
            }
            else {
                slog(SLOG_INFO, "Running as effective uid %d", pwd->pw_uid);
            }
        }

        // Init DBus well known interface
        // must be possible with the user from config file
        dbus_init();

#ifdef USE_SANE
        if (getenv("SANE_CONFIG_DIR") != NULL) {
            slog(SLOG_DEBUG, "SANE_CONFIG_DIR=%s", getenv("SANE_CONFIG_DIR"));
        }
        else {
            slog(SLOG_WARN, "SANE_CONFIG_DIR not set");
        }
        // Init SANE
        SANE_Int sane_version = 0;
        sane_init(&sane_version, 0);
        slog(SLOG_INFO, "sane version %d.%d",
             SANE_VERSION_MAJOR(sane_version),
             SANE_VERSION_MINOR(sane_version));
#else
        if (scanbtnd_init() < 0) {
            slog(SLOG_INFO, "Could not initialize scanbuttond modules!\n");
            exit(EXIT_FAILURE);
        }
        assert(backend);
#endif
        // get all devices locally connected to the system
#ifdef USE_SANE
        get_sane_devices();
#else
        get_scbtn_devices();
#endif
        // start the polling threads
#ifdef USE_SANE
        start_sane_threads();
#else
        start_scbtn_threads();
#endif

        // start dbus thread
        dbus_start_dbus_thread();

#ifdef USE_LIBUDEV
        udev_start_udev_thread();
#endif

        // well, sit here and wait ...
        // this thread executes the signal handlers
        while(true) {
            if (pause() < 0) {
                slog(SLOG_DEBUG, "pause: %s", strerror(errno));
            }
        }
    }
    exit(EXIT_SUCCESS); // never reached
}
