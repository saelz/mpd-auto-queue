#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "log.h"

#define RED "\033[1;31m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

int verbose = 0;

void
log_data(enum LOG_LEVEL level, char *format,...){
	time_t raw_time;
	struct tm *timeinfo;

	va_list args;
	FILE *stream;

	const char month[][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

	time(&raw_time);
	timeinfo = localtime(&raw_time);

	switch (level) {
	case LOG_VERBOSE:
		if (!verbose)
			return;
		/* fallthrough */
	case LOG_INFO:
		stream = stdout;
		break;
	case LOG_WARNING:
		stream = stderr;
		fprintf(stream,YELLOW);
		break;
	case LOG_ERROR:
		stream = stderr;
		fprintf(stream,RED);
		break;
	}


	fprintf(stream,"<%s %d %.2d:%.2d:%.2d> ",month[timeinfo->tm_mon],timeinfo->tm_mday,
			timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);

	va_start(args,format);
	vfprintf(stream,format,args);
	va_end(args);

	fprintf(stream,RESET"\n");

}

void
toggle_verbose_logging(int toggle){
	verbose = toggle;
}

int
verbose_logging_enabled(){
	return verbose;
}
