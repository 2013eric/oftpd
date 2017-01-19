#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <pthread.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <err.h>

#include "oftpd.h"
#include "ftp_listener.h"
#include "error.h"

/* put our executable name here where everybody can see it */
static const char *exe_name = "oftpd";

static void print_usage(const char *error);

int main(int argc, char *argv[])
{
    char *endptr;
    int port;
    int max_clients;
    int log_facility;

    char *user_ptr;
    char *dir_ptr;
    char *address;
    
    char temp_buf[256];

    struct passwd *user_info;
    error_t err;

    ftp_listener_t ftp_listener;

    int detach;

    sigset_t term_signal;
    int sig;

    /* grab our executable name */
    if (argc > 0) {
        exe_name = argv[0];
    }

    /* verify we're running as root */
    if (geteuid() != 0) {
        fprintf(stderr, "%s: program needs root permission to run\n", exe_name);
        exit(1);
    }

    /* default command-line arguments */
    port = FTP_PORT;
    user_ptr = NULL;
    dir_ptr = NULL;
    address = FTP_ADDRESS;
    max_clients = MAX_CLIENTS;
    detach = 1;
    log_facility = LOG_FTP;

    extern char *optarg;
    extern int optind, optopt, opterr, optreset;

    int ch;
    while ((ch = getopt(argc, argv, "p:i:m:l:Nh")) != -1)
        switch (ch) {
        case 'p': {
            long num = strtol(optarg, &endptr, 0);
            if (num < MIN_PORT || num > MAX_PORT || *endptr != '\0') {
                snprintf(temp_buf, sizeof(temp_buf), "port must be a number between "
                    "%d and %d", MIN_PORT, MAX_PORT);
                print_usage(temp_buf);
                exit(1);
            }
            port = num;
            break;
        }
        case 'i':
            address = optarg;
            break;
        case 'm': {
            long num = strtol(optarg, &endptr, 0);
            if (num < MIN_NUM_CLIENTS || num > MAX_NUM_CLIENTS || *endptr != '\0') {
                snprintf(temp_buf, sizeof(temp_buf), "max clients must be a number "
                    "between %d and %d", MIN_NUM_CLIENTS, MAX_NUM_CLIENTS);
                print_usage(temp_buf);
                exit(1);
            }
            max_clients = num;
            break;
        }
        case 'l':
            switch (optarg[0]) {
	    case '0':
		log_facility = LOG_LOCAL0;
		break;
	    case '1':
		log_facility = LOG_LOCAL1;
		break;
	    case '2':
		log_facility = LOG_LOCAL2;
		break;
	    case '3':
		log_facility = LOG_LOCAL3;
		break;
	    case '4':
		log_facility = LOG_LOCAL4;
		break;
	    case '5':
		log_facility = LOG_LOCAL5;
		break;
	    case '6':
		log_facility = LOG_LOCAL6;
		break;
	    case '7':
		log_facility = LOG_LOCAL7;
		break;
            default:
                print_usage("unknown option");
                exit(1);
	    }
        case 'N':
            detach = 0;
            break;
        case 'h':
            print_usage(NULL);
            exit(0);
        case '?':
            exit(1);
        }

    for (int i = optind; i < argc; i++) {
        if (user_ptr == NULL) {
            user_ptr = argv[i];
        } else if (dir_ptr == NULL) {
            dir_ptr = argv[i];
        } else {
            print_usage("too many arguments on the command line");
            exit(1);
        }
    }
    if ((user_ptr == NULL) || (dir_ptr == NULL)) {
        print_usage("missing user and/or directory name");
        exit(1);
    }

    user_info = getpwnam(user_ptr);
    if (user_info == NULL) {
        fprintf(stderr, "%s: invalid user name\n", exe_name);
        exit(1);
    }

    /* become a daemon */
    if (detach)
    if (daemon(false, false) == -1) {
        fprintf(stderr, "error becoming daemon: %s\n", strerror(errno));
        exit(1);
    }

    /* avoid SIGPIPE on socket activity */
    signal(SIGPIPE, SIG_IGN);         

    /* log the start time */
    openlog(NULL, LOG_NDELAY, log_facility);
    syslog(LOG_INFO,"Starting, version %s, as PID %d", VERSION, getpid());

    /* change to root directory */
    if (chroot(dir_ptr) != 0) {
        syslog(LOG_ERR, "error with root directory; %s\n", exe_name, 
          strerror(errno));
        exit(1);
    }
    if (chdir("/") != 0) {
        syslog(LOG_ERR, "error changing directory; %s\n", strerror(errno));
        exit(1);
    }

    /* create our main listener */
    if (!ftp_listener_init(&ftp_listener, 
                           address,
                           port,
                           max_clients,
                           INACTIVITY_TIMEOUT, 
                           &err)) 
    {
        syslog(LOG_ERR, "error initializing FTP listener; %s",
          error_get_desc(&err));
        exit(1);
    }

    if (setgroups(0, NULL) == -1) {
        syslog(LOG_ERR, "error removing supplementary groups: %s", strerror(errno));
        exit(1);
    }

    /* set user to be as inoffensive as possible */
    if (setgid(user_info->pw_gid) != 0) {
        syslog(LOG_ERR, "error changing group; %s", strerror(errno));
        exit(1);
    }
    if (setuid(user_info->pw_uid) != 0) {
        syslog(LOG_ERR, "error changing group; %s", strerror(errno));
        exit(1);
    }

    /* start our listener */
    if (ftp_listener_start(&ftp_listener, &err) == 0) {
        syslog(LOG_ERR, "error starting FTP service; %s", error_get_desc(&err));
        exit(1);
    }

    /* wait for a SIGTERM and exit gracefully */
    sigemptyset(&term_signal);
    sigaddset(&term_signal, SIGTERM);
    sigaddset(&term_signal, SIGINT);
    pthread_sigmask(SIG_BLOCK, &term_signal, NULL);
    sigwait(&term_signal, &sig);
    if (sig == SIGTERM) {
        syslog(LOG_INFO, "SIGTERM received, shutting down");
    } else { 
        syslog(LOG_INFO, "SIGINT received, shutting down");
    }
    ftp_listener_stop(&ftp_listener);
    syslog(LOG_INFO, "all connections finished, FTP server exiting");
    exit(0);
}

static void print_usage(const char *error)
{
    if (error != NULL) {
        fprintf(stderr, "%s: %s\n", exe_name, error);
    }
    fprintf(stderr, "usage: %s [-N] [-p num] [-i addr] [-m num] [-l num] user path\n"
                    "       %s -h\n", exe_name, exe_name);
}
