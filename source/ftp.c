/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2019, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/ 
/* <DESC>
 * FTP wildcard pattern matching
 * </DESC>
 */ 
#include <curl/curl.h>
#include <stdio.h>
#include "files.h"
 
struct callback_data {
  FILE *output;
  uint8_t download;
  const char* filename;
};
 
static long file_is_coming(struct curl_fileinfo *finfo, struct callback_data *data, int remains);
static long file_is_downloaded(struct callback_data *data);
static size_t write_it(char *buff, size_t size, size_t nmemb, void *cb_data);
 
int ftp_util(const char *ftp_url, const char* local_name)
{
  LOG("FTPing to %s >> (%s)", ftp_url, local_name);

  /* curl easy handle */ 
  CURL *handle;
 
  /* help data */ 
  struct callback_data data = { 
    .output = NULL, 
    .filename = local_name,
    .download = strncmp(strrchr(local_name, '/'), "/[ls]", 5)
  };
 
  /* global initialization */ 
  int rc = curl_global_init(CURL_GLOBAL_ALL);
  if(rc)
    return rc;
 
  /* initialization of easy handle */ 
  handle = curl_easy_init();
  if(!handle) {
    curl_global_cleanup();
    return CURLE_OUT_OF_MEMORY;
  }

  if (!data.download)
  {
    FILE* fp = fopen(local_name, "w");
    fclose(fp);
  }

  /* turn on wildcard matching */ 
  curl_easy_setopt(handle, CURLOPT_WILDCARDMATCH, 1L);
 
  /* callback is called before download of concrete file started */ 
  curl_easy_setopt(handle, CURLOPT_CHUNK_BGN_FUNCTION, file_is_coming);
 
  /* callback is called after data from the file have been transferred */ 
  curl_easy_setopt(handle, CURLOPT_CHUNK_END_FUNCTION, file_is_downloaded);
 
  /* this callback will write contents into files */ 
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_it);
 
  /* put transfer data into callbacks */ 
  curl_easy_setopt(handle, CURLOPT_CHUNK_DATA, &data);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
 
  /* curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L); */ 
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);

/*
    // Set SSL VERSION to TLS 1.2
    curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    // Set timeout for the connection to build
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
    // Follow redirects (?)
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
*/

  /* set an URL containing wildcard pattern (only in the last part) */ 
  curl_easy_setopt(handle, CURLOPT_URL, ftp_url);
 
  /* and start transfer! */ 
  rc = curl_easy_perform(handle);

  if(rc != CURLE_OK)
    LOG("curl_easy_perform() failed: %s\n", curl_easy_strerror(rc));

  curl_easy_cleanup(handle);
  curl_global_cleanup();
  return rc;
}
 
static long file_is_coming(struct curl_fileinfo *finfo, struct callback_data *data, int remains)
{
    FILE* fp = NULL;

    if (!data->download)
    {
        fp = fopen(data->filename, "a");

        if (!fp)
            return CURL_CHUNK_BGN_FUNC_FAIL;
    }

    LOG("%3d %40s %10luB ", remains, finfo->filename, (unsigned long)finfo->size);

    switch(finfo->filetype) {
        case CURLFILETYPE_DIRECTORY:
            if (fp)
                fprintf(fp, "D=%s\n", finfo->filename);
//            LOG(" DIR %s ", finfo->filename);
            break;
        case CURLFILETYPE_FILE:
            if (fp)
                fprintf(fp, "F=%s\n", finfo->filename);
//            LOG("FILE %s ", finfo->filename);
            break;
        default:
            LOG("OTHER\n");
            break;
    }

    if (fp)
        fclose(fp);

    if(data->download && (finfo->filetype == CURLFILETYPE_FILE)) {
        data->output = fopen(data->filename, "wb");
        if(!data->output) {
            return CURL_CHUNK_BGN_FUNC_FAIL;
        }
    } else
        return CURL_CHUNK_BGN_FUNC_SKIP;    

    return CURL_CHUNK_BGN_FUNC_OK;
}
 
static long file_is_downloaded(struct callback_data *data)
{
  if(data->output) {
    LOG("DOWNLOADED\n");
    fclose(data->output);
    data->output = 0x0;
  }
  return CURL_CHUNK_END_FUNC_OK;
}
 
static size_t write_it(char *buff, size_t size, size_t nmemb, void *cb_data)
{
  struct callback_data *data = cb_data;
  size_t written = 0;
  if(data->output)
    written = fwrite(buff, size, nmemb, data->output);

  return written;
}