#include <curl/curl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "net.h"

char *
build_url(size_t count, ...) {
	size_t i;
	int escape;
	char *str;
	char *url, *ptr = NULL;
	size_t len = 0, str_len;

	va_list list;

	CURL *curl = curl_easy_init();

	if (curl == NULL) {
		log_data(LOG_ERROR, "Error initializing curl");
		return NULL;
	}

	va_start(list, count);
	for (i = 0; i < count; i++) {
		escape = va_arg(list, int);
		str = va_arg(list, char *);

		str_len = strlen(str);

		if (escape) {
			str = curl_easy_escape(curl, str, str_len);
			str_len = strlen(str);
		}

		ptr = realloc(ptr, len + str_len + 1);
		if (ptr == NULL)
			goto error;

		url = ptr;
		memcpy(url + len, str, str_len + 1);
		len += str_len;

		if (escape)
			curl_free(str);
	}

	va_end(list);
	curl_easy_cleanup(curl);
	return url;
error:
	if (escape)
		curl_free(str);

	free(ptr);
	va_end(list);
	curl_easy_cleanup(curl);
	return NULL;
}

static size_t
cb(void *data, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct str *res = userp;

	char *ptr = realloc(res->data, res->size + realsize + 1);
	if (ptr == NULL)
		return 0;

	res->data = ptr;
	memcpy(res->data + res->size, data, realsize);
	res->size += realsize;
	res->data[res->size] = '\0';

	return realsize;
}

struct str
request(const char *url) {
	CURL *curl = NULL;
	CURLcode res;
	struct str result;

	result.data = NULL;
	result.size = 0;

	if (url == NULL)
		return result;

	curl = curl_easy_init();

	if (curl == NULL) {
		log_data(LOG_ERROR, "Error initializing curl");
		return result;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
	res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		log_data(LOG_WARNING, "Unable to retrieve data");
	}

	return result;
}
