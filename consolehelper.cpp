// Reimplementation of consolehelper using readable code and a nicer
// toolkit.
// (c) 2002-2004 Bernhard Rosenkraenzer <bero@arklinux.org>
//
// Released under the terms of the GNU General Public License version 2,
// or if, and only if, the GPL v2 is ruled invalid in a court of law, any
// later version of the GPL published by the Free Software Foundation.
//
// The text mode version is supposed to be VERY small, therefore it should
// be compilable without libstdc++. Real C++ is used only if DISABLE_X11
// isn't set.

extern "C" {
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <grp.h>
#include <pwd.h>
}

// We need to manipulate the environment directly
extern char **environ;

// Get an environment variable, but ignore it if it contains
// potentially harmful parts.
char const * const sgetenv(char const * const name, bool slashok=true, char const * const dflt=0);
// Create a sprintf type string with the correct size
char *s_printf(char const * const format, ...);
// Parse a boolean config variable
bool parseBool(char const * const str, bool dflt=false);
// Exit with cleanups
void fail(int ret=0);
// Become normal user
static void become_normal(char const * const user);
// Become root
static void become_root();

#ifndef DISABLE_X11
#include <QApplication>
#include "consolehelperdlg.h"
#endif

// PAM stuff
static struct app_data {
	pam_handle_t *pam;
	bool fallback_allowed, fallback_chosen, cancelled, kdeinit;
} appdata = {
	NULL,
	false, false, false, false
};

#ifndef DISABLE_X11
// conversation handler for X11
static int x_converse(int num, const struct pam_message **msg, struct pam_response **resp, void *data);
static struct pam_conv pam_x = {
	x_converse,
	&appdata
};
#endif
// conversation handler for text mode
static int text_converse(int num, const struct pam_message **msg, struct pam_response **resp, void *data);
static struct pam_conv pam_text = {
	misc_conv,
	&appdata
};
// primitive conversation handler if we don't have a controlling tty or X
static int silent_converse(int num, const struct pam_message **msg, struct pam_response **resp, void *data);
static struct pam_conv pam_silent = {
	silent_converse,
	&appdata
};

char *s_printf(char const * const format, ...)
{
	va_list arg;
	char *buf=NULL;
	va_start(arg, format);
	int bufSize=vsnprintf(buf, 0, format, arg);
	va_end(arg);
	buf=(char*)malloc(bufSize+1);
	if(!buf)
		return 0;
	va_start(arg, format);
	vsnprintf(buf, bufSize+1, format, arg);
	va_end(arg);
	return buf;
}

char const * const sgetenv(char const * const name, bool slashok, char const * const dflt)
{
	char *ret=getenv(name);
	if(ret && !strstr(ret, "..") && !strchr(ret, '%') && (slashok || !strchr(ret, '/')))
		return ret;
	return dflt;
}

#ifndef DISABLE_X11
static int x_converse(int num, const struct pam_message **msg, struct pam_response **resp, void *data)
{
	bool needdlg=false;
	struct pam_response *reply=(struct pam_response*)malloc(num*sizeof(struct pam_response));
	if(!reply)
		return PAM_CONV_ERR;
	char *noecho_message, *user, *service;
	bool free_user=false;
	struct app_data *appdata=(struct app_data*)data;
	if(pam_get_item(appdata->pam, PAM_USER, (const void **)&user)!=PAM_SUCCESS) {
		user=strdup("root");
		free_user=true;
	}
	ConsoleHelperDlg dlg;
	for(int count=0; count<num; count++)
	switch(msg[count]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
		case PAM_PROMPT_ECHO_OFF:
			dlg.addPrompt(msg[count]->msg, msg[count]->msg_style==PAM_PROMPT_ECHO_OFF);
			needdlg=true;
			break;
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			reply[count].resp_retcode = PAM_SUCCESS;
			dlg.addInfo(msg[count]->msg);
			needdlg=true;
			break;
		default:
			// Must be an error - no need to go on
			return PAM_CONV_ERR;
	}
	if(needdlg) {
		dlg.addButtons();
		if(dlg.exec()!=QDialog::Accepted) {
			appdata->cancelled=true;
			return PAM_ABORT;
		} else {
			for(int count=0; count<num; count++) {
				if(msg[count]->msg_style==PAM_PROMPT_ECHO_ON || msg[count]->msg_style==PAM_PROMPT_ECHO_OFF) {
					reply[count].resp=strdup(dlg.text(count).toLocal8Bit().data());
					reply[count].resp_retcode = PAM_SUCCESS;
				}
			}
		}
	}
	*resp=reply;
	if(free_user)
		free(user);
	return PAM_SUCCESS;
}
#endif

static int text_converse(int num, const struct pam_message **msg, struct pam_response **resp, void *data)
{
	return PAM_CONV_ERR;
}

static int silent_converse(int num, const struct pam_message **msg, struct pam_response **resp, void *data)
{
	return PAM_CONV_ERR;
}

bool parseBool(char const * const s, bool dflt)
{
	if(!s)
		return dflt;
	if(!strcasecmp(s, "true") || !strcasecmp(s, "yes") || !strcasecmp(s, "t") || !strcasecmp(s, "y") || !strcasecmp(s, "1"))
		return true;
	else if(!strcasecmp(s, "false") || !strcasecmp(s, "no") || !strcasecmp(s, "f") || !strcasecmp(s, "n") || !strcasecmp(s, "0"))
		return false;
	return dflt;
}

void fail(int ret)
{
	if(appdata.pam)
		pam_end(appdata.pam, ret);
	exit(ret);
}

static void become_normal(char const * const user)
{
	initgroups(user, getgid());
	if(getegid()!=getgid())
		fail(1);
	setreuid(getuid(), getuid());
	if(geteuid()!=getuid()) // setreuid failed
		fail(1);
}

static void become_root()
{
	setgroups(0, NULL);
	setregid(0, 0);
	setreuid(0, 0);
	if(getuid() || geteuid() || getgid() || getegid())
		fail(1);
}

void shift(int &argc, char **&argv)
{
	for(int i=0; i<argc; i++)
		argv[i]=argv[i+1];
	argv[argc--]=0L;
}

int main(int argc, char **argv)
{
	struct passwd *caller=getpwuid(getuid());
	struct pam_conv *conv=&pam_silent;
	if(isatty(STDIN_FILENO))
		conv=&pam_text;
	if(!caller || !caller->pw_name) {
		fputs("You don't have a user name.", stderr);
		fail(1);
	}
#ifdef DISABLE_X11
	// Launch X11 frontend if available
	execv("/usr/bin/consolehelper-qt", argv);
#else
	bool use_gui=false;
	char *display=getenv("DISPLAY");
	if(display && strlen(display))
		use_gui=true;
#endif
	// Find out which application we're trying to wrap
	char *app=argv[0];
	if(strchr(app, '/'))
		app=strrchr(app, '/')+1;

	if(!strcmp(app, "consolehelper") || !strcmp(app, "consolehelper-qt")) {
		// We were invoked by our own name -- shift arguments and
		// try to act more like su
		if(argc < 2) // PEBKAC
			fail(1);

		// Shift arguments... We know who we are
		shift(argc, argv);

		app=argv[0];

		if(strchr(app, '/'))
			app=strrchr(app,'/')+1;
	}
	
	// Check our console.apps file
	char *appsfile=s_printf("/etc/security/console.apps/%s", app);
	struct stat st;
	// We ignore the file if it isn't save (or doesn't exist)...
	if(stat(appsfile, &st)) {
		perror("Can't stat console app");
		fail(1);
	}
	if(!S_ISREG(st.st_mode) || (st.st_mode & S_IWOTH)) {
		fprintf(stderr, "Bad permissions on %s\n", appsfile);
		fail(1);
	}
	// Read config file
	FILE *f=fopen(appsfile, "r");
	char *user=NULL, *program=NULL;
	bool session=false;
	int retry=3;
	while(!feof(f) && !ferror(f)) {
		char buf[256];
		fgets(buf, 256, f);
		if(!buf[0] || buf[0]=='#') // ignore comments and empty lines
			continue;
		while(buf[strlen(buf)-1]=='\n')
			buf[strlen(buf)-1]=0;
		if(!strncasecmp(buf, "USER=", 5))
			user=strdup(buf+5);
		else if(!strncasecmp(buf, "PROGRAM=", 8))
			program=strdup(buf+8);
#ifndef DISABLE_X11
		else if(use_gui && !strncasecmp(buf, "GUI=", 4))
			use_gui=parseBool(buf+4);
		else if(use_gui && !strncasecmp(buf, "NOXOPTION=", 10)) {
			// Don't use the GUI if NOXOPTION was passed...
			char *noxoption=buf+10;
			if(noxoption[0]) {
				for(int i=1; i<argc; i++) {
					if(!strcmp(argv[i], noxoption)) {
						use_gui=false;
						break;
					}
				}
			}
		}
#endif
		else if(!strncasecmp(buf, "SESSION=", 8))
			session=parseBool(buf+8);
		else if(!strncasecmp(buf, "FALLBACK=", 9))
			appdata.fallback_allowed=parseBool(buf+9);
		else if(!strncasecmp(buf, "RETRY=", 6))
			retry=atoi(buf+6);
		else if(!strncasecmp(buf, "DENY=", 5)) {
			// Deny based on command line arguments...
			char *deny=strdup(buf+5);
			char *tokptr=NULL;
			char *param=strtok_r(deny, ";;", &tokptr);
			while(param) {
				if(!strlen(param))
					continue;
				for(int i=1; i<argc; i++) {
					if(!strcmp(argv[i], param)) {
						free(deny);
						fprintf(stderr, "Permission denied.\n");
						fail(1);
					}
				}
				param=strtok_r(0, ";;", &tokptr);
			}
			free(deny);
		} else if(!strncasecmp(buf, "KDEINIT=", 8)) {
			appdata.kdeinit=parseBool(buf+8);
		}
	}
	fclose(f);
	if(!program)
		fail(1);
	if(!strchr(program, '/')) {
		// YUCK.
		char *tmp=(char*)malloc(strlen(program)+128);
		sprintf(tmp, "/usr/sbin/%s", program);
		if(access(tmp, X_OK))
			sprintf(tmp, "/sbin/%s", program);
		if(!access(tmp, X_OK)) {
			free(program);
			program=tmp;
		}
	}
	if(access(program, X_OK))
		fail(1);
	
	if(!user || !strcasecmp(user, "<USER>")) {
		if(user)
			free(user);
		user=caller->pw_name;
	}

	struct passwd *target=getpwnam(user);
	if(!target) {
		fprintf(stderr, "User %s doesn't exist.\n", user);
		fail(1);
	}
#ifndef DISABLE_X11
	if(use_gui)
		conv=&pam_x;
	QCoreApplication::setSetuidAllowed(true);
	QApplication qappl(argc, argv, use_gui);
#endif
	// Save some important environment variables...
	char const * const env_display=sgetenv("DISPLAY", false);
	char const * const env_home=sgetenv("HOME");
	char const * const env_lang=sgetenv("LANG", false);
	char const * const env_lcall=sgetenv("LC_ALL", false);
	char const * const env_lcmsgs=sgetenv("LC_MESSAGES", false);
	char const * const env_shell=sgetenv("SHELL");
	char const * const env_term=sgetenv("TERM", false, "dumb");
	char const * const env_xauthority=sgetenv("XAUTHORITY", true);

	// Clear the environment, put only safe stuff back
	environ = (char**)malloc(sizeof(char*)*32);
	memset(environ, 0, sizeof(char*)*32);
	if(env_display)	setenv("DISPLAY", env_display, 1);
	if(env_lang)	setenv("LANG", env_lang, 1);
	if(env_lcall)	setenv("LC_ALL", env_lcall, 1);
	if(env_lcmsgs)	setenv("LC_MESSAGES", env_lcmsgs, 1);
	if(env_shell)	setenv("SHELL", env_shell, 1);
	if(env_term)	setenv("TERM", env_term, 1);

	// If we're going UID 0, set HOME to ~root...
	if(target->pw_uid == 0)
		setenv("HOME", target->pw_dir, 1);
	else {
		if(env_home)
			setenv("HOME", env_home, 1);
		else if(caller->pw_dir)
			setenv("HOME", caller->pw_dir, 1);
	}

	// Set a safe and sane path
	setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin:/root/bin", 1);
	// LOGNAME and USER...
	setenv("LOGNAME", "root", 1);
	setenv("USER", "root", 1);
	
	// Authenticate user with PAM...
	int retval=pam_start(app, user, conv, &appdata.pam);
	if(retval!=PAM_SUCCESS) {
		fputs("Can't run pam_start.", stderr);
		fail(1);
	}

	do {
		retval=pam_authenticate(appdata.pam, 0);
	} while((retval!=PAM_SUCCESS) && --retry && !appdata.fallback_chosen && !appdata.cancelled);
	if(retval!=PAM_SUCCESS) {
		pam_end(appdata.pam, retval);
		appdata.pam=NULL;
		if(appdata.cancelled) {
			fail(retval);
		} else if(appdata.fallback_allowed) {
			// Reset XAUTHORITY so we can open windows
			if(env_xauthority)
				setenv("XAUTHORITY", env_xauthority, 1);
			become_normal(caller->pw_name);
			execv(program, argv);
			exit(1);
		} else {
			// PEBKAC ;)
			fail(retval);
		}
	}
	// Check if we're really the user we'd like to be
	char *auth_user;
	retval=pam_get_item(appdata.pam, PAM_USER, (const void **)&auth_user);
	if(retval!=PAM_SUCCESS)
		fail(retval);
	if(strcmp(auth_user, target->pw_name)) // No
		fail(1);

	// Check if we may run this service
	retval=pam_acct_mgmt(appdata.pam, 0);
	if(retval!=PAM_SUCCESS)
		fail(retval);
	
	// Check if we need to open a session...
	if(session) {
		// We may need to run GUI apps...
		if(env_xauthority)
			setenv("XAUTHORITY", env_xauthority, 1);
		retval=pam_open_session(appdata.pam, 0);
		if(retval!=PAM_SUCCESS)
			fail(1);
		pid_t child=fork();
		if(child==-1)
			fail(1);
		if(child==0) {
			// Child process - make preparations and exec program
			char **env_pam=pam_getenvlist(appdata.pam);
			while(env_pam && *env_pam) {
				putenv(strdup(*env_pam));
				env_pam++;
			}
			become_root();

			if(appdata.kdeinit) {
				// We may need to launch a root kdeinit too
				// Unfortunately, we can't just e.g. wrap
				// "kdeinit konqueror"
				// through consolehelper because it terminates
				// before konqueror is up, killing the
				// Xauthority file. kdeinit; konqueror will
				// have to do.
				pid_t cchild=fork();
				if(cchild==0) {
					execl("/usr/bin/kdeinit", "kdeinit", NULL);
					exit(0);
				}
			}

			execv(program, argv);
			exit(1);
		}
		// Parent - wait for child.
		int status;
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		wait4(child, &status, 0, NULL);
		// End session
		retval=pam_close_session(appdata.pam, 0);
		if(retval!=PAM_SUCCESS)
			fail(retval);
		if(WIFEXITED(status)&&(WEXITSTATUS(status)==0)) {
			pam_end(appdata.pam, PAM_SUCCESS);
			exit(0);
		} else {
			pam_end(appdata.pam, PAM_SUCCESS);
			exit(status);
		}
	} else { // Not opening a session
		pam_end(appdata.pam, PAM_SUCCESS);
		execv(program, argv);
		exit(1);
	}
}
