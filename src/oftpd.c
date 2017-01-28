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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>

#include "oftpd.h"
#include "ftp_listener.h"
#include "error.h"

static void print_usage(const char *error);

int main(int argc, char *argv[])
{
    char *endptr;
    int port = FTP_PORT;
    int max_clients = MAX_CLIENTS;
    int log_facility = LOG_FTP;

    char *user_ptr = NULL;
    char *dir_ptr = NULL;
    char *address = FTP_ADDRESS;
    
    char temp_buf[256];

    struct passwd *user_info;
    error_t err;

    ftp_listener_t ftp_listener;

    bool detach = true;

    sigset_t term_signal;
    int sig;

    /* verify we're running as root */
    if (geteuid() != 0) {
        fprintf(stderr, "oftpd: program needs root permission to run\n");
        exit(1);
    }

    extern char *optarg;
    extern int optind;

    int ch;
    while ((ch = getopt(argc, argv, "p:i:m:l:Nhv")) != -1)
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
        case 'i': {
            char *if_name, *if_addr;
            struct ifaddrs *ifap, *ifa;
            struct sockaddr_in *sa;

            getifaddrs(&ifap);

            for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    sa = (struct sockaddr_in *)ifa->ifa_addr;
                    if_name = ifa->ifa_name;
                    if_addr = inet_ntoa(sa->sin_addr);

                    if (strncmp(optarg, if_name, strlen(if_name)) == 0
                    ||  strncmp(optarg, if_addr, strlen(if_addr)) == 0) {
                        address = if_addr;
                        break;
                    }
                }
            }

            freeifaddrs(ifap);

            if (!address) {
                fprintf(stderr, "oftpd: cannot find specified interface\n");
                exit(1);
            }

            break;
        }
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
        case 'l': {
            int locals[] = {LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
                            LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7};
            if ('0' <= optarg[0] && optarg[0] <= '7') {
                log_facility = locals[optarg[0] - '0'];
                break;
            } else {
                print_usage("unknown option");
                exit(1);
            }
        }
        case 'N':
            detach = 0;
            break;
        case 'h':
            print_usage(NULL);
            exit(0);
        case 'v':
            puts(VERSION);
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
        fprintf(stderr, "oftpd: invalid user name\n");
        exit(1);
    }

    /* avoid SIGPIPE on socket activity */
    signal(SIGPIPE, SIG_IGN);         

    /* log the start time */
    openlog(NULL, LOG_NDELAY|LOG_PID, log_facility);
    syslog(LOG_INFO,"Starting, version %s, as PID %d", VERSION, getpid());

    /* change to root directory */
    if (chroot(dir_ptr) != 0) {
        fprintf(stderr, "oftpd: cannot chroot: %s\n", strerror(errno));
        exit(1);
    }

    if (detach && daemon(false, false) == -1) {
        fprintf(stderr, "oftpd: cannot become daemon: %s\n", strerror(errno));
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

    /* drop privilege */
    setgroups(0, NULL);
    setgid(user_info->pw_gid);
    setuid(user_info->pw_uid);

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
        fprintf(stderr, "oftpd: %s\n", error);
    }
    fprintf(stderr, "usage: oftpd [-N] [-p num] [-i arg] [-m num] [-l num] user path\n"
                    "       oftpd -h\n"
                    "       oftpd -v\n");
}
