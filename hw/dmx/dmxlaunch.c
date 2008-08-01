/*
 * Copyright © 2005 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: David Reveman <davidr@novell.com>
 *          Matthias Hopf <mhopf@suse.de>
 */

#include "dmx.h"
#include "dmxlaunch.h"
#include "config/dmxconfig.h"
#include "opaque.h"

#include <X11/Xauth.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <libgen.h>
#include <fcntl.h>

typedef void (*sighandler_t) (int);

#define XBE_DIE_TIMEOUT 3
#define XBE_DEV_RANDOM  "/dev/urandom"

static char xbeAuthBuf[256];
static char *xbeAuthTempl = "/tmp/.Xdmx-auth-XXXXXX";
static char *xbeAuth      = NULL;

static char *xbeProgs[] = { "/var/X11R6/bin/Xbackend", "/usr/bin/X" };
static char *xbeProg    = NULL;

static char xbeDisplayBuf[256];
static char *xbeDisplay      = NULL;
static int  xbeDisplayOffset = 53;

static pid_t   xbePid = 0;
static int     receivedUsr1 = 0;
static jmp_buf jumpbuf;

static char **xbeArgv = 0;
static int  nXbeArgv  = 0;

static int
dmxAddXbeArguments (char **argv,
		    int  n)
{
    char **newArgv;
    int  i;

    newArgv = xrealloc (xbeArgv, sizeof (char *) * (nXbeArgv + n));
    if (!newArgv)
	return 0;

    for (i = 0; i < n; i++)
	newArgv[nXbeArgv + i] = argv[i];

    xbeArgv   = newArgv;
    nXbeArgv += n;

    return n;
}

static void
sigAlarm (int sig)
{
    ErrorF ("%s won't die, killing it\n", basename (xbeProg));

    kill (xbePid, SIGKILL);
    if (xbePid)
	while (waitpid (xbePid, NULL, 0) == -1 && errno == EINTR);
}

void
dmxAbortDisplay (void)
{
    sighandler_t oldSigAlarm;
    unsigned int oldAlarm;
    int          status = 0;
    char	 *name;

    if (!xbePid)
	return;

    name = basename (xbeProg);

    oldAlarm    = alarm (0);
    oldSigAlarm = signal (SIGALRM, sigAlarm);

    kill (xbePid, SIGTERM);

    alarm (XBE_DIE_TIMEOUT);
    while (waitpid (xbePid, &status, 0) == -1 && errno == EINTR);
    alarm (0);

    signal (SIGALRM, oldSigAlarm);
    alarm (oldAlarm);

    if (WIFEXITED (status))
    {
	if (WEXITSTATUS (status))
	    ErrorF ("%s died, exit status %d\n", name, WEXITSTATUS (status));
    }
    else if (WIFSIGNALED (status))
	ErrorF ("%s died, signal %d\n", name, WTERMSIG (status));
    else
	ErrorF ("%s died, dubious exit\n", name);

    if (xbeAuth)
	unlink (xbeAuth);
}

static void
sigUsr1Waiting (int sig)
{
    signal (sig, sigUsr1Waiting);
    receivedUsr1++;
}

static void
sigUsr1Jump (int sig)
{

#ifdef HAVE_SIGPROCMASK
    sigset_t set;
#endif

    signal (sig, sigUsr1Waiting);

#ifdef HAVE_SIGPROCMASK
    sigemptyset (&set);
    sigaddset (&set, SIGUSR1);
    sigprocmask (SIG_UNBLOCK, &set, NULL);
#endif

    longjmp (jumpbuf, 1);
}

#define AUTH_DATA_LEN 16 /* bytes of authorization data */

static Bool
dmxSetupAuth (char *name, int authFd)
{
    Xauth   auth;
    int	    randomFd, i;
    ssize_t bytes, size;
    char    authHost[256];
    char    authData[AUTH_DATA_LEN];
    char    realProg[PATH_MAX], buf[PATH_MAX];
    FILE    *file;
    int     virtualFb = FALSE;

    auth.family = FamilyLocal;

    gethostname (authHost, sizeof (authHost));

    auth.address	= authHost;
    auth.address_length = strlen (authHost);

    auth.number	= strrchr (xbeDisplay, ':');
    if (!auth.number)
    {
	ErrorF ("Bad back-end X display name: %s\n", xbeDisplay);
	return FALSE;
    }

    auth.number++;

    auth.number_length = strlen (auth.number);
    if (!auth.number_length)
    {
	ErrorF ("Bad back-end X display name: %s\n", xbeDisplay);
	return FALSE;
    }

    auth.name	     = "MIT-MAGIC-COOKIE-1";
    auth.name_length = strlen (auth.name);

    randomFd = open (XBE_DEV_RANDOM, O_RDONLY);
    if (randomFd == -1)
    {
	ErrorF ("Failed to open " XBE_DEV_RANDOM "\n");
	return FALSE;
    }

    bytes = 0;
    do {
	size = read (randomFd, authData + bytes, AUTH_DATA_LEN - bytes);
	if (size <= 0)
	    break;

	bytes += size;
    } while (bytes != AUTH_DATA_LEN);

    close (randomFd);

    if (bytes != AUTH_DATA_LEN)
    {
	ErrorF ("Failed to read %d random bytes from " XBE_DEV_RANDOM "\n",
		AUTH_DATA_LEN);
	return FALSE;
    }

    auth.data	     = authData;
    auth.data_length = AUTH_DATA_LEN;

    file = fdopen (authFd, "w");
    if (!file)
    {
	ErrorF ("Failed to open authorization file: %s\n", name);
	close (authFd);
	return FALSE;
    }

    XauWriteAuth (file, &auth);
    fclose (file);

    strcpy (realProg, xbeProg);

#define MAX_SYMLINKS 32

    for (i = 0; i < MAX_SYMLINKS; i++)
    {
	size = readlink (realProg, buf, sizeof (buf) - 1);
	if (size == -1)
	    break;
	
	memcpy (realProg, buf, size);
	realProg[size] = '\0';
    }

    /* a bit hackish but very useful */
    if (strcmp (basename (realProg), "Xvfb")  == 0 ||
	strcmp (basename (realProg), "Xfake") == 0)
	virtualFb = TRUE;
    
    dmxConfigStoreDisplay (basename (realProg),
			   xbeDisplay,
			   auth.name,
			   auth.data,
			   auth.data_length,
			   virtualFb);

    return TRUE;
}

Bool
dmxLaunchDisplay (int argc, char *argv[], int index, char *vt)
{
    sighandler_t oldSigUsr1;
    pid_t	 pid;
    char	 *name;
    char	 *auth[] = { "-auth", xbeAuthBuf };
    char	 *defs[] = { "-terminate", "-nolisten", "tcp" };
    char	 *endArg = NULL;
    int		 authFd;
    int		 mask;

    if (xbePid)
	return TRUE;

    strcpy (xbeAuthBuf, xbeAuthTempl);
    mask = umask (0077);
    authFd = mkstemp (xbeAuthBuf);
    umask (mask);
    if (authFd == -1)
	FatalError ("Failed to generate unique authorization file\n");

    xbeAuth = xbeAuthBuf;

    if (index && index < argc)
    {
        xbeProg = argv[index];
    }
    else
    {
        struct stat buf;
	int         i;

	for (i = 0; i < sizeof (xbeProgs) / sizeof (char *); i++)
	{
	    if (stat (xbeProgs[i], &buf) == 0)
	    {
		xbeProg = xbeProgs[i];
		break;
	    }
	}

	if (!xbeProg)
            FatalError ("Can't find X server executable\n");
    }

    if (!dmxAddXbeArguments (&xbeProg, 1))
	return FALSE;

    if (!dmxAddXbeArguments (auth, sizeof (auth) / sizeof (char *)))
	return FALSE;

    xbeDisplay = xbeDisplayBuf;
    sprintf (xbeDisplay, ":%d", xbeDisplayOffset + atoi (display));
    
    if (index)
    {
	int i;

	for (i = index + 1; i < argc; i++)
	{
	    if (*argv[i] == ':')
	    {
		xbeDisplay = argv[i];
		break;
	    }
	}

	if (i >= argc)
	    if (!dmxAddXbeArguments (&xbeDisplay, 1))
		return FALSE;

        if (argc > index)
            if (!dmxAddXbeArguments (&argv[index + 1], argc - index))
                return FALSE;
    }
    else
    {
	if (!dmxAddXbeArguments (&xbeDisplay, 1))
	    return FALSE;

        if (!dmxAddXbeArguments (defs, sizeof (defs) / sizeof (char *)))
            return FALSE;
    }

    if (vt)
	if (!dmxAddXbeArguments (&vt, 1))
	    return FALSE;

    if (!dmxAddXbeArguments (&endArg, 1))
	return FALSE;

    name = basename (xbeProg);

    if (!dmxSetupAuth (xbeAuth, authFd))
	FatalError ("Failed to set up authorization: %s\n", xbeAuth);

    oldSigUsr1 = signal (SIGUSR1, sigUsr1Waiting);

    pid = fork ();

    switch (pid) {
    case -1:
	perror ("fork");
	FatalError ("fork");
	break;
    case 0:
	signal (SIGUSR1, SIG_IGN);
	execv (xbeArgv[0], xbeArgv);
	perror (xbeArgv[0]);
	exit (2);
	break;
    default:
	xbePid = pid;
	break;
    }

    for (;;)
    {
	int status;

	signal (SIGUSR1, sigUsr1Waiting);
	if (setjmp (jumpbuf))
	    break;

	signal (SIGUSR1, sigUsr1Jump);
	if (receivedUsr1)
	    break;

	if (waitpid (xbePid, &status, 0) != -1)
	{
	    if (WIFEXITED (status))
	    {
                FatalError ("%s died, exit status %d\n", name,
                            WEXITSTATUS (status));
	    }
	    else if (WIFSIGNALED (status))
		FatalError ("%s died, signal %d\n", name, WTERMSIG (status));
	    else
		FatalError ("%s died, dubious exit\n", name);
	}
    }

    signal (SIGUSR1, oldSigUsr1);

    return TRUE;
}
