#ifndef MPD_AUTO_QUEUE_LOG
#define MPD_AUTO_QUEUE_LOG

enum LOG_LEVEL{
	LOG_VERBOSE,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR
};

void
log_data(enum LOG_LEVEL level, char *format,...);

void
toggle_verbose_logging(int toggle);

int
verbose_logging_enabled();
#endif
