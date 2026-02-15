/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzargentina = "America/Buenos_Aires";
char *tzutc = "UTC";
char *tzberlin = "Europe/Berlin";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

/*
 * loadavg: return formatted string with load averages as percentages
 * relative to the number of CPU cores, with time labels.
 * e.g. "12% (1m) 34% (5m) 20% (15m)"
 * Caller should free().
 */
char *
loadavg(void)
{
    double avgs[3];
    long ncores;

    if (getloadavg(avgs, 3) < 0)
        return smprintf("");

    /* get number of CPU cores (fallback to 1 if unavailable) */
    ncores = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncores < 1)
        ncores = 1;

    /* convert to percentages, cap at 100% */
    int pct1 = (int)((avgs[0] / ncores) * 100.0);
    int pct5 = (int)((avgs[1] / ncores) * 100.0);
    int pct15 = (int)((avgs[2] / ncores) * 100.0);

    if (pct1 > 100) pct1 = 100;
    if (pct5 > 100) pct5 = 100;
    if (pct15 > 100) pct15 = 100;

    return smprintf("%d%% (1m) %d%% (5m) %d%% (15m)", pct1, pct5, pct15);
}
char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base);

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
execscript(char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;

	memset(retval, 0, sizeof(retval));

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");
	retval[strlen(retval)-1] = '\0';

	return smprintf("%s", retval);
}

char *
getdiskfree(char *path)
{
    char *command = smprintf("df -h %s | awk 'NR==2 {print $6 \": \" $5}'", path);

    return execscript(command);
}

int
main(void)
{
	char *status;
	char *avgs;
	char *bat;
	char *tmar;
	char *tmutc;
	char *tmbln;
	char *t0;
	char *t1;
	char *t2;
	char *kbmap;
	// char *surfs;
    char *vol;
    char *ram;
    char *diskfree_home;
    char *diskfree_root;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		avgs = loadavg();
		bat = getbattery("/sys/class/power_supply/BAT0");
        tmar = mktimes("%d/%m/%Y %H:%M", tzargentina);
		tmutc = mktimes("%H:%M", tzutc);
		tmbln = mktimes("KW %W %a %d %b %H:%M %Z %Y", tzberlin);
		kbmap = execscript("setxkbmap -query | grep layout | cut -d':' -f 2- | tr -d ' '");
		// surfs = execscript("surf-status");
		t0 = gettemperature("/sys/class/hwmon/hwmon0", "temp1_input");
		t1 = gettemperature("/sys/class/hwmon/hwmon1", "temp1_input");
		t2 = gettemperature("/sys/class/hwmon/hwmon2", "temp1_input");
        vol = execscript("amixer get Master | tail -1 | sed 's/.*\\[\\([0-9]*%\\)\\].*/\\1/'");
        ram = execscript("echo \"$(free -h | awk 'NR==2 {print $3}')B/$(free -h | awk 'NR==2 {print $2}')B\"");
        diskfree_home = getdiskfree("/dev/nvme0n1p4");
        diskfree_root = getdiskfree("/dev/nvme0n1p3");

		status = smprintf("%s | %s VOL: %s | BAT: %s | Load:%s | RAM: %s | %s",
				// surfs,
                diskfree_home, diskfree_root, vol, bat, avgs, ram, tmar);
		setstatus(status);

		// free(surfs);
		free(kbmap);
		free(t0);
		free(t1);
		free(avgs);
		free(bat);
		free(tmar);
		free(tmutc);
		free(tmbln);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

