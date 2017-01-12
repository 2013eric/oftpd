AC_INIT([oftpd], [0.3.7])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/oftpd.c])
AC_CONFIG_FILES([Makefile])

AM_INIT_AUTOMAKE([1.11 subdir-objects -Wall])
AM_SILENT_RULES([yes])

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h sys/time.h syslog.h unistd.h sys/types.h)

dnl Check for ss_family
AC_EGREP_CPP(__ss_family,
[#include<sys/socket.h>
],,AC_DEFINE(HAVE_NEW_SS_FAMILY))

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLOCKS
AC_STRUCT_ST_RDEV
AC_HEADER_STAT
AC_HEADER_TIME
AC_STRUCT_ST_BLOCKS
AC_STRUCT_ST_RDEV
AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(getcwd gettimeofday select socket strerror localtime_r gmtime_r)
dnl AC_CHECK_LIB(pthread, pthread_create)
dnl AC_SEARCH_LIBS(pthread_create, [ pthread pthreads thread threads ])
AC_SEARCH_LIBS(socket, socket)
AC_SEARCH_LIBS(inet_ntoa, nsl)
AC_CHECK_FUNCS(inet_aton)


dnl check for efficient file transfer mechanisms (i.e. sendfile())
have_sendfile=no
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
], [
off_t offset;
sendfile(0, 1, &offset);
], 
AC_DEFINE(HAVE_LINUX_SENDFILE) AC_DEFINE(HAVE_SENDFILE) have_sendfile=yes
)
if test $have_sendfile = no; then
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
], [
off_t offset;
off_t sbytes;
sendfile(0, 1, &offset, 0, NULL, &sbytes, 0);
],
dnl sendfile() seems broken on FreeBSD, so for now we'll just omit it
dnl AC_DEFINE(HAVE_FREEBSD_SENDFILE) AC_DEFINE(HAVE_SENDFILE) have_sendfile=yes
)
fi



dnl decipher pthread compilation model

dnl first, try linking a threaded application with "-pthread", 
dnl which will work on FreeBSD (and OpenBSD) systems
save_LDFLAGS="$LDFLAGS"
LDFLAGS="-pthread $LDFLAGS"
AC_TRY_LINK([ 
#include <pthread.h>
], [
pthread_self();
],
,
LDFLAGS="$save_LDFLAGS"
)

dnl then, try looking in various libraries, which will work on other systems
AC_SEARCH_LIBS(pthread_self, [ pthread pthreads thread threads ])

dnl add reentrant flags
CFLAGS="$CFLAGS -D_REENTRANT -D_THREAD_SAFE"

dnl Check whether to enable IPv6 support
AC_ARG_ENABLE([ipv6],[  --enable-ipv6           Enable IPv6 support (disabled by default)],
              CFLAGS="$CFLAGS -DINET6",)

AH_TOP([#undef HAVE_NEW_SS_FAMILY])
AH_TOP([#undef HAVE_SENDFILE])
AH_TOP([#undef HAVE_LINUX_SENDFILE])
AH_TOP([#undef HAVE_FREEBSD_SENDFILE])

AC_SUBST(HAVE_NEW_SS_FAMILY)
AC_OUTPUT
