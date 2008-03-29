/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

/* error.c stolen nearly verbatim from
    "Unix Network Programming Volume 1 The Sockets Networking API (3rd Ed)"
   by W. Richard Stevens, Bill Fenner, Andrew M Rudoff
  Addison-Wesley Professional Computing Series
  Addison-Wesley, Boston 2003
  http://www.unpbook.com
*/

#include <config.h>
#include <owfs_config.h>
#include <ow.h>
#include <stdarg.h>

/* See man page for explanation */
int log_available = 0;

/* Print message and return to caller
 * Caller specifies "errnoflag" and "level" */
#define MAXLINE     1023
void err_msg(enum e_err_type errnoflag, enum e_err_level level, const char *fmt, ...)
{
	int errno_save = errno;		/* value caller might want printed */
	int n;
	char buf[MAXLINE + 1];
	enum e_err_print sl;		// 2=console 1=syslog
	va_list ap;

	/* Print where? */
	switch (Global.error_print) {
	case e_err_print_mixed:
		sl = Global.now_background ? e_err_print_syslog : e_err_print_console;
		break;
	case e_err_print_syslog:
		sl = e_err_print_syslog;
		break;
	case e_err_print_console:
		sl = e_err_print_console;
		break;
	default:
		return;
	}

	va_start(ap, fmt);
	UCLIBCLOCK;
	/* Create output string */
#ifdef    HAVE_VSNPRINTF
	vsnprintf(buf, MAXLINE, fmt, ap);	/* safe */
#else
	vsprintf(buf, fmt, ap);		/* not safe */
#endif
	n = strlen(buf);
	if (errnoflag == e_err_type_error) {
		snprintf(buf + n, MAXLINE - n, ": %s", strerror(errno_save));
	}
	UCLIBCUNLOCK;
	va_end(ap);

	if (sl == e_err_print_syslog) {	/* All output to syslog */
		strcat(buf, "\n");
		if (!log_available) {
			openlog("OWFS", LOG_PID, LOG_DAEMON);
			log_available = 1;
		}
		syslog(level <= e_err_default ? LOG_INFO : LOG_NOTICE, buf);
	} else {
		fflush(stdout);			/* in case stdout and stderr are the same */
		switch (level) {
		case e_err_default:
			fputs("DEFAULT: ", stderr);
			break;
		case e_err_connect:
			fputs("CONNECT: ", stderr);
			break;
		case e_err_call:
			fputs("   CALL: ", stderr);
			break;
		case e_err_data:
			fputs("   DATA: ", stderr);
			break;
		case e_err_detail:
			fputs(" DETAIL: ", stderr);
			break;
		case e_err_debug:
		case e_err_beyond:
		default:
			fputs("  DEBUG: ", stderr);
			break;
		}
		fputs(buf, stderr);
		fflush(stderr);
	}
	return;
}

/* Purely a debugging routine -- print an arbitrary buffer of bytes */
void _Debug_Bytes(const char *title, const unsigned char *buf, int length)
{
	int i;
	/* title line */
	printf("Byte buffer %s, length=%d", title ? title : "anonymous", (int) length);
	if (length < 0) {
		printf("\n-- Attempt to write with bad length\n");
		return;
	}
	if (buf == NULL) {
		printf("\n-- NULL buffer\n");
		return;
	}
#if 0
	/* hex lines -- 16 bytes each */
	for (i = 0; i < length; ++i) {
		if ((i & 0x0F) == 0) {	// devisible by 16
			printf("\n--");
		}
		printf(" %.2X", buf[i]);
	}

#endif
	/* char line -- printable or . */
	printf("\n   <");
	for (i = 0; i < length; ++i) {
		char c = buf[i];
		printf("%c", isprint(c) ? c : '.');
	}
	printf(">\n");
}