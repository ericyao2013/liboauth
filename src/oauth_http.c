/*
 * oAuth string functions in POSIX-C.
 *
 * Copyright 2007, 2008 Robin Gareus <robin@gareus.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"
#include "oauth.h"

#define OAUTH_USER_AGENT "liboauth-agent/" VERSION

#ifdef HAVE_CURL
#include <curl/curl.h>
#include <sys/stat.h>

struct MemoryStruct {
  char *data;
  size_t size;
};

static size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;

  mem->data = (char *)xrealloc(mem->data, mem->size + realsize + 1);
  if (mem->data) {
    memcpy(&(mem->data[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
  }
  return realsize;
}

/**
 * cURL http post function.
 * the returned string (if not NULL) needs to be freed by the caller
 *
 * @param u url to retrieve
 * @param p post parameters 
 * @return returned HTTP
 */
char *oauth_curl_post (const char *u, const char *p) {
  CURL *curl;
  CURLcode res;

  struct MemoryStruct chunk;
  chunk.data=NULL;
  chunk.size = 0;

  curl = curl_easy_init();
  if(!curl) return NULL;
  curl_easy_setopt(curl, CURLOPT_URL, u);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, p);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, OAUTH_USER_AGENT);
  res = curl_easy_perform(curl);
  if (res) {
    // error
    return NULL;
  }

  curl_easy_cleanup(curl);
  return (chunk.data);
}

/**
 * cURL http get function.
 * the returned string (if not NULL) needs to be freed by the caller
 *
 * @param u url to retrieve
 * @param q optional query parameters 
 * @return returned HTTP
 */
char *oauth_curl_get (const char *u, const char *q) {
  CURL *curl;
  CURLcode res;
  char *t1=NULL;

  if (q) {
    t1=xmalloc(sizeof(char)*(strlen(u)+strlen(q)+2));
    strcat(t1,u); strcat(t1,"?"); strcat(t1,q);
  }

  struct MemoryStruct chunk;
  chunk.data=NULL;
  chunk.size = 0;

  curl = curl_easy_init();
  if(!curl) return NULL;
  curl_easy_setopt(curl, CURLOPT_URL, q?t1:u);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, OAUTH_USER_AGENT);
  res = curl_easy_perform(curl);
  if (q) free(t1);
  if (res) {
    return NULL;
  }

  curl_easy_cleanup(curl);
  return (chunk.data);
}

/**
 * cURL http post raw data from file.
 * the returned string needs to be freed by the caller
 *
 * @param u url to retrieve
 * @param fn filename of the file to post along
 * @param len length of the file in bytes. set to '0' for autodetection
 * @param customheader specify custom HTTP header (or NULL for default)
 * @return returned HTTP or NULL on error
 */
char *oauth_curl_post_file (const char *u, const char *fn, size_t len, const char *customheader) {
  CURL *curl;
  CURLcode res;

  struct MemoryStruct chunk;
  chunk.data=NULL;
  chunk.size = 0;

  struct curl_slist *slist=NULL;
  if (customheader)
    slist = curl_slist_append(slist, customheader);
  else
    slist = curl_slist_append(slist, "Content-Type: image/jpeg;");

  if (!len) {
    struct stat statbuf;
    if (stat(fn, &statbuf) == -1) return(NULL);
    len = statbuf.st_size;
  }

  FILE *f = fopen(fn,"r");
  if (!f) return NULL;

  curl = curl_easy_init();
  if(!curl) return NULL;
  curl_easy_setopt(curl, CURLOPT_URL, u);
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist); 
  curl_easy_setopt(curl, CURLOPT_READDATA, f);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, OAUTH_USER_AGENT);
  res = curl_easy_perform(curl);
  if (res) {
    // error
    return NULL;
  }
  fclose(f);

  curl_easy_cleanup(curl);
  return (chunk.data);
}

#endif // no cURL.

#define _OAUTH_ENV_HTTPCMD "OAUTH_HTTP_CMD"
#define _OAUTH_DEF_HTTPCMD "curl -sA '"OAUTH_USER_AGENT"' -d '%p' '%u' "
// alternative: "wget -q -U 'liboauth-agent/0.1' --post-data='%p' '%u' "

#define _OAUTH_ENV_HTTPGET "OAUTH_HTTP_GET_CMD"
#define _OAUTH_DEF_HTTPGET "curl -sA '"OAUTH_USER_AGENT"' '%u' "
// alternative: "wget -q -U 'liboauth-agent/0.1' '%u' "

#include <stdio.h>

/**
 * execute command and return it's output.
 * This is used to call 'curl' or 'wget'.
 *
 * @param cmd speify the commandline to execute
 * @return stdout string that needs to be freed or NULL if there's no output
 */
char *oauth_exec_shell (char *cmd) {
#ifdef DEBUG_OAUTH
  printf("DEBUG: executing: %s\n",cmd);
#endif
  FILE *in = popen (cmd, "r");
  size_t len = 0;
  size_t alloc = 0;
  char *data = NULL;
  int rcv = 1;
  while (in && rcv > 0 && !feof(in)) {
    alloc +=1024;
    data = xrealloc(data, alloc * sizeof(char));
    rcv = fread(data, sizeof(char), 1024, in);
    len += rcv;
  }
  pclose(in);
#ifdef DEBUG_OAUTH
  printf("DEBUG: read %i bytes\n",len);
#endif
  data[len]=0;
#ifdef DEBUG_OAUTH
  if (data) printf("DEBUG: return: %s\n",data);
  else printf("DEBUG: NULL data\n");
#endif
  return (data);
}

/**
 * send POST via a command line HTTP client.
 */
char *oauth_exec_post (const char *u, const char *p) {
  char cmd[BUFSIZ];
  char *cmdtpl = getenv(_OAUTH_ENV_HTTPCMD);
  if (!cmdtpl) cmdtpl = strdup (_OAUTH_DEF_HTTPCMD);
  else cmdtpl = strdup (cmdtpl); // clone getenv() string.

  // add URL and post param - error if no '%p' or '%u' present in definition
  char *t1,*t2;
  t1=strstr(cmdtpl, "%p");
  t2=strstr(cmdtpl, "%u");
  if (!t1 || !t2) {
	fprintf(stderr, "\nliboauth: invalid HTTP command. set the '%s' environement variable.\n\n",_OAUTH_ENV_HTTPCMD);
	return(NULL);
  }
  *(++t1)= 's'; *(++t2)= 's';
  // TODO: check if there are exactly two '%' in cmdtpl
  snprintf(cmd, BUFSIZ, cmdtpl, (t1 > t2)?u:p, (t1 > t2)?p:u);
  free(cmdtpl);
  return oauth_exec_shell(cmd);
}

/**
 * send GET via a command line HTTP client.
 */
char *oauth_exec_get (const char *u, const char *q) {
  char cmd[BUFSIZ];
  if (!u) return (NULL);
  char *cmdtpl = getenv(_OAUTH_ENV_HTTPGET);
  if (!cmdtpl) cmdtpl = strdup (_OAUTH_DEF_HTTPGET);
  else cmdtpl = strdup (cmdtpl); // clone getenv() string.

  // add URL and post param - error if no '%p' or '%u' present in definition
  char *t1;
  t1=strstr(cmdtpl, "%u");
  if (!t1) {
	fprintf(stderr, "\nliboauth: invalid HTTP command. set the '%s' environement variable.\n\n",_OAUTH_ENV_HTTPGET);
	return(NULL);
  }
  *(++t1)= 's';
  if (q) {
    t1=xmalloc(sizeof(char)*(strlen(u)+strlen(q)+2));
    strcat(t1,u); strcat(t1,"?"); strcat(t1,q);
  }
  snprintf(cmd, BUFSIZ, cmdtpl, q?t1:u);
  free(cmdtpl);
  if (q) free(t1);
  return oauth_exec_shell(cmd);
}

/**
 * do a HTTP GET request, wait for it to finish 
 * and return the content of the reply.
 * (requires libcurl or a command-line HTTP client)
 * 
 * Note: u and q are just concatenated with a '?' in between unless q is NULL. in which case only u will be used.
 *
 * @param u base url to get
 * @param q query string to send along with the HTTP request.
 * @return  In case of an error NULL is returned; otherwise a pointer to the
 * replied content from HTTP server. latter needs to be freed by caller.
 */
char *oauth_http_get (const char *u, const char *q) {
#ifdef HAVE_CURL
  return oauth_curl_get(u,q);
#else // no cURL.
  return oauth_exec_get(u,q);
#endif
}

/**
 * do a HTTP POST request, wait for it to finish 
 * and return the content of the reply.
 * (requires libcurl or a command-line HTTP client)
 *
 * @param u url to query
 * @param p postargs to send along with the HTTP request.
 * @return  In case of an error NULL is returned; otherwise a pointer to the
 * replied content from HTTP server. latter needs to be freed by caller.
 */
char *oauth_http_post (const char *u, const char *p) {
#ifdef HAVE_CURL
  return oauth_curl_post(u,p);
#else // no cURL.
  return oauth_exec_post(u,p);
#endif
}

/**
 * http post raw data from file.
 * the returned string needs to be freed by the caller
 *
 * @param u url to retrieve
 * @param fn filename of the file to post along
 * @param len length of the file in bytes. set to '0' for autodetection
 * @param customheader specify custom HTTP header (or NULL for default)
 * @return returned HTTP reply or NULL on error
 */
char *oauth_post_file (const char *u, const char *fn, const size_t len, const char *contenttype){
#ifdef HAVE_CURL
  return oauth_curl_post_file (u, fn, len, contenttype);
#else
  fprintf(stderr, "\nliboauth: oauth_post_file requires curl. curl is not available.\n\n");
  return (NULL);
#endif
}
/* vi:set ts=8 sts=2 sw=2: */

