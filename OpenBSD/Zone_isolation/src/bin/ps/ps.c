/*	$OpenBSD: ps.c,v 1.63 2015/01/16 06:39:32 deraadt Exp $	*/
/*	$NetBSD: ps.c,v 1.15 1995/05/18 20:33:25 mycroft Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>	/* MAXCOMLEN NODEV */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <zones.h>

#include "ps.h"

extern char *__progname;

struct varent *vhead;

int	eval;			/* exit value */
int	rawcpu;			/* -C */
int	sumrusage;		/* -S */
int	termwidth;		/* width of screen (0 == infinity) */
int	totwidth;		/* calculated width of requested variables */

int	needcomm, needenv, neednlist, commandonly;

enum sort { DEFAULT, SORTMEM, SORTCPU } sortby = DEFAULT;

static char	*kludge_oldps_options(char *);
static int	 pscomp(const void *, const void *);
static void	 scanvars(void);
static void	 usage(void);

char dfmt[] = "pid tt state time command";
char zfmt[] = "pid tt state time command zone";
char tfmt[] = "pid tid tt state time command";
char jfmt[] = "user pid ppid pgid sess jobc state tt time command";
char lfmt[] = "uid pid ppid cpu pri nice vsz rss wchan state tt time command";
char   o1[] = "pid";
char   o2[] = "tt state time command";
char ufmt[] = "user pid %cpu %mem vsz rss tt state start time command";
char vfmt[] = "pid state time sl re pagein vsz rss lim tsiz %cpu %mem command";

kvm_t *kd;
int kvm_sysctl_only;

zoneid_t zonename_to_zoneid(char* );
zoneid_t* full_zoneid_list(size_t* nzs);

int
main(int argc, char *argv[])
{
	struct kinfo_proc *kp, **kinfo;
	struct varent *vent;
	struct winsize ws;
	struct passwd *pwd;
	dev_t ttydev;
	pid_t pid;
	uid_t uid;
	int all, ch, flag, i, fmt, lineno, nentries, zoneinfo, zonefilter = 0;
	int prtheader, showthreads, wflag, kflag, what, Uflag, xflg;
	char *nlistf, *memf, *swapf, *cols, errbuf[_POSIX2_LINE_MAX];
	zoneid_t zoneid_filter;

	if ((cols = getenv("COLUMNS")) != NULL && *cols != '\0') {
		const char *errstr;

		termwidth = strtonum(cols, 1, INT_MAX, &errstr);
		if (errstr != NULL)
			warnx("COLUMNS: %s: %s", cols, errstr);
	}
	if (termwidth == 0) {
		if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 &&
		    ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 &&
		    ioctl(STDIN_FILENO,  TIOCGWINSZ, &ws) == -1) ||
		    ws.ws_col == 0)
			termwidth = 79;
		else
			termwidth = ws.ws_col - 1;
	}

	if (argc > 1)
		argv[1] = kludge_oldps_options(argv[1]);

	all = fmt = prtheader = showthreads = wflag = kflag = Uflag = xflg = 0;
	pid = -1;
	uid = 0;
	ttydev = NODEV;
	memf = nlistf = swapf = NULL;
	while ((ch = getopt(argc, argv,
	    "AaCcegHhjkLlM:mN:O:o:p:rSTt:U:uvW:wxZz:")) != -1)
		switch (ch) {
		case 'A':
			all = 1;
			xflg = 1;
			break;
		case 'a':
			all = 1;
			break;
		case 'C':
			rawcpu = 1;
			break;
		case 'c':
			commandonly = 1;
			break;
		case 'e':			/* XXX set ufmt */
			needenv = 1;
			break;
		case 'g':
			break;			/* no-op */
		case 'H':
			showthreads = 1;
			break;
		case 'h':
			prtheader = ws.ws_row > 5 ? ws.ws_row : 22;
			break;
		case 'j':
			parsefmt(jfmt);
			fmt = 1;
			jfmt[0] = '\0';
			break;
		case 'k':
			kflag++;
			break;
		case 'L':
			showkey();
			exit(0);
		case 'l':
			parsefmt(lfmt);
			fmt = 1;
			lfmt[0] = '\0';
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			sortby = SORTMEM;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'O':
			parsefmt(o1);
			parsefmt(optarg);
			parsefmt(o2);
			o1[0] = o2[0] = '\0';
			fmt = 1;
			break;
		case 'o':
			parsefmt(optarg);
			fmt = 1;
			break;
		case 'p':
			pid = atol(optarg);
			xflg = 1;
			break;
		case 'r':
			sortby = SORTCPU;
			break;
		case 'S':
			sumrusage = 1;
			break;
		case 'T':
			if ((optarg = ttyname(STDIN_FILENO)) == NULL)
				errx(1, "stdin: not a terminal");
			/* FALLTHROUGH */
		case 't': {
			struct stat sb;
			char *ttypath, pathbuf[PATH_MAX];

			if (strcmp(optarg, "co") == 0)
				ttypath = _PATH_CONSOLE;
			else if (*optarg != '/')
				(void)snprintf(ttypath = pathbuf,
				    sizeof(pathbuf), "%s%s", _PATH_TTY, optarg);
			else
				ttypath = optarg;
			if (stat(ttypath, &sb) == -1)
				err(1, "%s", ttypath);
			if (!S_ISCHR(sb.st_mode))
				errx(1, "%s: not a terminal", ttypath);
			ttydev = sb.st_rdev;
			break;
		}
		case 'U':
			pwd = getpwnam(optarg);
			if (pwd == NULL)
				errx(1, "%s: no such user", optarg);
			uid = pwd->pw_uid;
			endpwent();
			Uflag = xflg = 1;
			break;
		case 'u':
			parsefmt(ufmt);
			sortby = SORTCPU;
			fmt = 1;
			ufmt[0] = '\0';
			break;
		case 'v':
			parsefmt(vfmt);
			sortby = SORTMEM;
			fmt = 1;
			vfmt[0] = '\0';
			break;
		case 'W':
			swapf = optarg;
			break;
		case 'w':
			if (wflag)
				termwidth = UNLIMITED;
			else if (termwidth < 131)
				termwidth = 131;
			wflag++;
			break;
		case 'x':
			xflg = 1;
			break;
		case 'Z':
			zoneinfo = 1;
			parsefmt(zfmt);
			fmt = 1;
			zfmt[0] = '\0';
			break;
		case 'z': {
			//zonename|zoneid
			size_t zone_id = 0;
                        char *ptr;
			zone_id = strtoul(optarg, &ptr, 10);
			if (zone_id == 0){
				zone_id = zonename_to_zoneid(optarg);
			}
			zonefilter = 1;
			zoneid_filter = zone_id; 
			break;
		}
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		nlistf = *argv;
		if (*++argv) {
			memf = *argv;
			if (*++argv)
				swapf = *argv;
		}
	}
#endif

	if (nlistf == NULL && memf == NULL && swapf == NULL) {
		kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
		kvm_sysctl_only = 1;
	} else {
		kd = kvm_openfiles(nlistf, memf, swapf, O_RDONLY, errbuf);
	}
	if (kd == NULL)
		errx(1, "%s", errbuf);

	if (!fmt) {
		if (showthreads)
			parsefmt(tfmt);
		else
			parsefmt(dfmt);
	}

	/* XXX - should be cleaner */
	if (!all && ttydev == NODEV && pid == -1 && !Uflag) {
		uid = getuid();
		Uflag = 1;
	}

	/*
	 * scan requested variables, noting what structures are needed,
	 * and adjusting header widths as appropriate.
	 */
	scanvars();

	if (neednlist && !nlistread)
		(void) donlist();

	/*
	 * get proc list
	 */
	if (Uflag) {
		what = KERN_PROC_UID;
		flag = uid;
	} else if (ttydev != NODEV) {
		what = KERN_PROC_TTY;
		flag = ttydev;
	} else if (pid != -1) {
		what = KERN_PROC_PID;
		flag = pid;
	} else if (kflag) {
		what = KERN_PROC_KTHREAD;
		flag = 0;
	} else {
		what = KERN_PROC_ALL;
		flag = 0;
	}
	if (showthreads)
		what |= KERN_PROC_SHOW_THREADS;

	/*
	 * select procs
	 */
	kp = kvm_getprocs(kd, what, flag, sizeof(*kp), &nentries);
	if (kp == NULL)
		errx(1, "%s", kvm_geterr(kd));

	/*
	 * print header
	 */
	printheader();
	if (nentries == 0)
		exit(1);
	/*
	 * sort proc list, we convert from an array of structs to an array
	 * of pointers to make the sort cheaper.
	 */
	if ((kinfo = reallocarray(NULL, nentries, sizeof(*kinfo))) == NULL)
		err(1, "failed to allocate memory for proc pointers");
	for (i = 0; i < nentries; i++)
		kinfo[i] = &kp[i];
	qsort(kinfo, nentries, sizeof(*kinfo), pscomp);
	/*
	 * for each proc, call each variable output function.
	 */
	for (i = lineno = 0; i < nentries; i++) {
		if (zonefilter) {
			if ((kinfo[i]->p_zoneid) != zoneid_filter) {
				continue;
			}
		}


		if (showthreads == 0 && (kinfo[i]->p_flag & P_THREAD) != 0)
			continue;
		if (xflg == 0 && ((int)kinfo[i]->p_tdev == NODEV ||
		    (kinfo[i]->p_psflags & PS_CONTROLT ) == 0))
			continue;
		if (showthreads && kinfo[i]->p_tid == -1)
			continue;
		for (vent = vhead; vent; vent = vent->next) {
			(vent->var->oproc)(kinfo[i], vent);
			if (vent->next != NULL)
				(void)putchar(' ');
		}
		(void)putchar('\n');
		if (prtheader && lineno++ == prtheader - 4) {
			(void)putchar('\n');
			printheader();
			lineno = 0;
		}
	}
	exit(eval);
}

static void
scanvars(void)
{
	struct varent *vent;
	VAR *v;
	int i;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		i = strlen(v->header);
		if (v->width < i)
			v->width = i;
		totwidth += v->width + 1;	/* +1 for space */
		if (v->flag & COMM)
			needcomm = 1;
		if (v->flag & NLIST)
			neednlist = 1;
	}
	totwidth--;
}

static int
pscomp(const void *v1, const void *v2)
{
	const struct kinfo_proc *kp1 = *(const struct kinfo_proc **)v1;
	const struct kinfo_proc *kp2 = *(const struct kinfo_proc **)v2;
	int i;
#define VSIZE(k) ((k)->p_vm_dsize + (k)->p_vm_ssize + (k)->p_vm_tsize)

	if (sortby == SORTCPU && (i = getpcpu(kp2) - getpcpu(kp1)) != 0)
		return (i);
	if (sortby == SORTMEM && (i = VSIZE(kp2) - VSIZE(kp1)) != 0)
		return (i);
	if ((i = kp1->p_tdev - kp2->p_tdev) == 0 &&
	    (i = kp1->p_ustart_sec - kp2->p_ustart_sec) == 0)
		i = kp1->p_ustart_usec - kp2->p_ustart_usec;
	return (i);
}

/*
 * ICK (all for getopt), would rather hide the ugliness
 * here than taint the main code.
 *
 *  ps foo -> ps -foo
 *  ps 34 -> ps -p34
 *
 * The old convention that 't' with no trailing tty arg means the users
 * tty, is only supported if argv[1] doesn't begin with a '-'.  This same
 * feature is available with the option 'T', which takes no argument.
 */
static char *
kludge_oldps_options(char *s)
{
	size_t len;
	char *newopts, *ns, *cp;

	len = strlen(s);
	if ((newopts = ns = malloc(2 + len + 1)) == NULL)
		err(1, NULL);
	/*
	 * options begin with '-'
	 */
	if (*s != '-')
		*ns++ = '-';	/* add option flag */

	/*
	 * gaze to end of argv[1]
	 */
	cp = s + len - 1;
	/*
	 * if last letter is a 't' flag with no argument (in the context
	 * of the oldps options -- option string NOT starting with a '-' --
	 * then convert to 'T' (meaning *this* terminal, i.e. ttyname(0)).
	 */
	if (*cp == 't' && *s != '-')
		*cp = 'T';
	else {
		/*
		 * otherwise check for trailing number, which *may* be a
		 * pid.
		 */
		while (cp >= s && isdigit((unsigned char)*cp))
			--cp;
	}
	cp++;
	memmove(ns, s, (size_t)(cp - s));	/* copy up to trailing number */
	ns += cp - s;
	/*
	 * if there's a trailing number, and not a preceding 'p' (pid) or
	 * 't' (tty) flag, then assume it's a pid and insert a 'p' flag.
	 */
	if (isdigit((unsigned char)*cp) &&
	    (cp == s || (cp[-1] != 't' && cp[-1] != 'p' &&
	    (cp - 1 == s || cp[-2] != 't'))))
		*ns++ = 'p';
	/* and append the number */
	(void)strlcpy(ns, cp, newopts + len + 3 - ns);

	return (newopts);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-AaCceHhjkLlmrSTuvwxZz] [-M core] [-N system] [-O fmt] [-o fmt] [-p pid]\n",
	    __progname);	
	(void)fprintf(stderr,
	    "%-*s[-t tty] [-U username] [-W swap]\n", (int)strlen(__progname) + 8, "");
	exit(1);
}

/*
* Go through the list of returned zone_id_t 
* and check if any zone name matches the argument
*/
zoneid_t
zonename_to_zoneid(char* search_name)
{
	int error,i;
	size_t nzs = 0;
	zoneid_t* zone_list;
	zone_list = full_zoneid_list(&nzs);
	
	size_t string_len = MAXZONENAMELEN;
	char* string_buffer = malloc(sizeof(char)*MAXZONENAMELEN);
	memset(string_buffer, 0, sizeof(char)*MAXZONENAMELEN);
	for(i = 0; i<nzs; i++){
		zone_name(zone_list[i],string_buffer,string_len);
		if (strcmp(search_name, string_buffer)==0) {
			return zone_list[i];
		} else {
			memset(string_buffer, 0, sizeof(char)*MAXZONENAMELEN);
		}
	}
	return -1;
}

/*
*  Return a full list so ERANGE is not an issue. 
*/
zoneid_t*
full_zoneid_list(size_t* nzs)
{
	int error;
	size_t buffer_size = 1;
	zoneid_t* buffer = malloc(sizeof(zoneid_t)*buffer_size);
	while (1){
		error = zone_list(buffer, &buffer_size);

		if ((error == -1) && (errno == ERANGE)) {
			buffer_size = buffer_size*2;
			buffer = realloc(buffer, sizeof(zoneid_t)*buffer_size);
		} else {
			break;
		}
	}
	*nzs = buffer_size;
	return buffer;
}

