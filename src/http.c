/*
** Copyright (C) 1995, 1996, 1997, 1998 Hewlett-Packard Company
** Originally by Kevin Hughes, kev@kevcom.com, 3/11/94
**
** This program and library is free software; you can redistribute it and/or
** modify it under the terms of the GNU (Library) General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU (Library) General Public License for more details.
**
** You should have received a copy of the GNU (Library) General Public License
**  long with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**--------------------------------------------------------------------
** All the code in this file added by Ron Klachko ron@ckm.ucsf.edu 9/98
** 
** change sprintf to snprintf to avoid corruption,
** test length of spiderdirectory before strcat to avoid corruption,
** added safestrcpy() macro to avoid corruption from strcpy overflow,
** define MAXPIDLEN instead of literal "32" - assumed return length from lgetpid()
** SRE 11/17/99
**
** added buffer size arg to grabStringValue - core dumping from overrun
** SRE 2/22/00
**
** 2000-11   jruiz,rasc  some redesign
*/

/*
** http.c
*/

#ifndef _WIN32
#include <unistd.h>
#endif

#include <time.h>
#include <stdarg.h>

#include "swish.h"
#include "index.h"
#include "hash.h"
#include "string.h"
#include "mem.h"
#include "file.h"
#include "check.h"
#include "error.h"

#include "http.h"
#include "httpserver.h"

#include "xml.h"
#include "txt.h"
#include "html.h"

typedef struct urldepth {
    char *url;
    int depth;
    struct urldepth *next;
} urldepth;


int http_already_indexed(SWISH *sw, char *url);
urldepth *add_url(SWISH *sw, urldepth *list, char *url, int depth, char *baseurl);


urldepth *add_url(SWISH *sw, urldepth *list, char *url, int depth, char *baseurl)
{
    urldepth *item;
	
    if (!equivalentserver(sw, url, baseurl)) {
		if (sw->verbose == 3)
			printf( "Skipping %s:  %s\n", url, "Wrong method or server." );
		
		
    } else if (sw->maxdepth && (depth >= sw->maxdepth)) {
		if (sw->verbose == 3)
			printf( "Skipping %s:  %s\n", url, "Too deep." );
    }else if (sw->nocontentslist && isoksuffix(url, sw->nocontentslist)) {
      if (sw->verbose == 3)
	printf( "Skipping %s: %s\n", url, "Wrong suffix." );
      
    } else if (urldisallowed(sw,url)) {
      if (sw->verbose == 3)
	printf( "Skipping %s:  %s\n", url, "URL disallowed by robots.txt." );
    } else if (!http_already_indexed(sw,url)) {
      item = (urldepth *)emalloc(sizeof(urldepth));
      item->url = estrdup(url);
      item->depth = depth;
#if 0
      /* Depth first searching
      **/
      item->next = list;
      list = item;
#else
      /* Breadth first searching
      **/
      item->next = 0;
      if (!list) {
	list = item;
      } else {
	urldepth *walk;
	for (walk = list; walk->next; walk = walk->next) {
	}
	walk->next = item;
      }
#endif
    }
    
    return list;
}


/* Have we already indexed a file or directory?
** This function is used to avoid multiple index entries
** or endless looping due to symbolic links.
*/

int http_already_indexed(SWISH *sw, char *url)
{
  struct url_info *p;
  
  int len;
  unsigned hashval;
  
  /* Hash with via the uri alone.  Depending on the equivalent
  ** servers, we may or may not make the decision of the entire
  ** url or just the uri.
  */
  hashval = bighash(url_uri(url,&len)); /* Search hash for this file. */
  for ( p = sw->url_hash[hashval]; p != NULL; p = p->next )
    if ( (strcmp(url, p->url ) == 0) ||
	 (equivalentserver(sw, url, p->url) &&
	  (strcmp(url_uri(url, &len), url_uri(p->url, &len)) == 0)) )
      {                               /* We found it. */
	if (sw->verbose == 3)
	  printf( "Skipping %s:  %s\n",
		  url, "Already indexed." );
	return 1;
      }
  
	/* Not found, make new entry. */
  p = (struct url_info*)emalloc(sizeof(struct url_info));
  p->url = estrdup(url);
  p->next = sw->url_hash[hashval];
  sw->url_hash[hashval] = p;
	
  return 0;
}


char *url_method(char *url, int *plen)
{
  char *end;
    if ((end = strstr( url, "://" )) == NULL) {
      return NULL;
    }
    *plen = end - url;
    return url;
}


char *url_serverport(char *url, int *plen)
{
  int methodlen;
  char *serverstart;
  char *serverend;
  
  if (url_method(url, &methodlen) == NULL) {
    return NULL;
  }
  
  /* +3 for 
  **/
  serverstart = url + methodlen + 3;
  if ((serverend = strchr(serverstart, '/')) == NULL) {
    *plen = strlen(serverstart);
  } else {
    *plen = serverend - serverstart;
  }
  
  return serverstart;
}


char *url_uri(char *url, int *plen)
{
  if ((url = url_serverport(url, plen)) == 0) {
    return 0;
  }
  url += *plen;
  *plen = strlen(url);
  return url;
}

#ifdef _WIN32
#include <stdlib.h>		/* _sleep() */
#include <process.h>	/* _getpid() */
#endif

int get(SWISH *sw, char *contenttype_or_redirect, time_t *plastretrieval, char *url)
{
  static int lenbuffer=0;
  static char *buffer=NULL;
  int code,cmdsize;
  FILE *fp;
  char *command;
#ifdef _WIN32
  char* spiderprog = "swishspider.pl";
  char commandline[] = "perl.exe %s%s %s/swishspider@%ld \"%s\"";
#else
  char* spiderprog = "swishspider";
  char commandline[] = "perl %s%s %s/swishspider@%ld '%s'";
#endif
  
  if(!lenbuffer)buffer=emalloc((lenbuffer=MAXSTRLEN)+1);

  /* Sleep a little so we don't overwhelm the server
  **/
    if ((time(0) - *plastretrieval) < sw->delay )
      {
	int num_sec = sw->delay - (time(0) - *plastretrieval);
#ifdef _WIN32
	_sleep(num_sec);
#else
	sleep(num_sec);
#endif
	}
    *plastretrieval = time(0);
	
    /* URLs can get quite large so don't depend on a fixed size buffer.  The
    ** +MAXPIDLEN is for the pid identifier and the trailing null.
    **/
    cmdsize = strlen(sw->spiderdirectory) + strlen(url) + strlen(sw->tmpdir)
            + strlen(commandline) + strlen(spiderprog) + MAXPIDLEN;
    command = (char *)emalloc(cmdsize + 1);
    sprintf(command, commandline, sw->spiderdirectory, spiderprog, sw->tmpdir, lgetpid(), url);
    
    if (system(command) == 0) {
      if((int)(strlen(sw->tmpdir)+MAXPIDLEN+30)>=lenbuffer) {
	lenbuffer=strlen(sw->tmpdir)+MAXPIDLEN+200;
	buffer=erealloc(buffer,lenbuffer+1);
      }
      sprintf(buffer, "%s/swishspider@%ld.response", sw->tmpdir, (long) lgetpid());
      fp = fopen(buffer, "r");
      fgets(buffer, lenbuffer, fp);
      code = atoi(buffer);
      if ((code == 200) || ((code /100) == 3)) {
	/* read content-type  redirect
	**/ 
	fgets(contenttype_or_redirect, MAXSTRLEN, fp);
	*(contenttype_or_redirect + strlen(contenttype_or_redirect) - 1) = '\0';
      }
      fclose(fp);
    } else {
      code = 500;
    }
    
    efree(command);
    
    return code;
}

int cmdf(int (*cmd)(const char *), char *fmt, char *string, pid_t pid)
{
int rc;
char *buffer;

    buffer=emalloc(strlen(fmt)+strlen(string)+sizeof(pid_t)*8+1);
    
    sprintf(buffer,fmt,string,pid);
	
    rc=cmd(buffer);
    efree(buffer);
    return rc;
}

char *readline (FILE *fp)
{
static char *buffer = 0;
static int buffersize = 512;
    if (buffer == 0) {
		buffer = (char *)emalloc(buffersize);
    }
    /*
	*Try to read in the line
	*/
	
    if (fgets(buffer, buffersize, fp) == NULL) {
		return NULL;
    }
	
    /*
	* Make sure we read the entire line.  If not, double the buffer
	* size and try to read the rest
	*/
    while (buffer[strlen(buffer) - 1] != '\n') {
		buffer = (char *)erealloc( buffer, buffersize * 2 );
		
		/*
		* The easiest way to verify that this line is okay is to consider
		* the situation where the buffer is 2 bytes longs.  Since fgets()
		* always guarantees to put the trailing NULL, it will have essentially
		* used only 1 bytes.  We double it to four, so we now have the left
		* over byte (that currently contains NULL) in addition to the doubling
		* which gets us to read buffersize + 1.
		*/ 
		if (fgets(buffer + buffersize - 1, buffersize + 1, fp) == 0) {
			break;
		}
		buffersize *= 2;
    }
	
    return buffer;
}


/* A local version of getpid() so that we don't have to suffer
** a system call each time we need it.
*/
pid_t lgetpid()
{
    static pid_t pid = -1;
    if (pid == -1) {
#ifdef _WIN32
		pid = _getpid();
#else
		pid = getpid();
#endif
    }
    return pid;
}

#if 0

/* Testing the robot rules parsing code...
**/
void http_indexpath(char *url)
{
    httpserverinfo *server = getserverinfo(url);
    robotrules *robotrule;
	
    printf("User-agent: %s\n", server->useragent ? server->useragent : "(none)");
    for (robotrule = server->robotrules; robotrule; robotrule = robotrule->next ) {
		printf("Disallow: %s\n", robotrule->disallow);
    }
}

#else

/********************************************************/
/*					"Public" functions					*/
/********************************************************/

/* The main entry point for the module.  For fs.c, decides whether this
** is a file or directory and routes to the correct routine.
*/
void http_indexpath(SWISH *sw,char *url)
{
urldepth *urllist = 0;
urldepth *item;
static int lenbuffer=0;
static char *buffer=NULL;
static int lentitle=0;
static char *title=NULL;
char *tmptitle;
static int lencontenttype=0;
static char *contenttype=NULL;
int code;

httpserverinfo *server;
char *link;
char *p;
FileProp *fprop;
FILE *fp;


    if(!lenbuffer)buffer=emalloc((lenbuffer=MAXFILELEN)+1);
    if(!lentitle)title=emalloc((lentitle=MAXSTRLEN)+1);
    if(!lencontenttype)contenttype=emalloc((lencontenttype=MAXSTRLEN)+1);
	
    /* prime the pump with the first url
    **/
    urllist = add_url(sw,urllist, url, 0, url);
	
    /* retrieve each url and add urls to a certain depth
    **/
    while (urllist) {
		item = urllist;
		urllist = urllist->next;
		
		if (sw->verbose >= 2) {
			printf("retrieving %s (%d)...\n", item->url, item->depth);
			fflush(stdout);
		}
		
		/* We don't check if this url is legal here, because we do that
		** before adding to the list.
		**/
		server = getserverinfo(sw,item->url);
		
		if ((code = get(sw, contenttype, &server->lastretrieval, item->url)) == 200) {
			/* Patch from Steve van der Burg */
			/* change from strcmp to strncmp */
			if (strncmp(contenttype, "text/html",9) == 0) {
				if((int)(strlen(sw->tmpdir)+MAXPIDLEN+30)>=lenbuffer) {
					lenbuffer=strlen(sw->tmpdir)+MAXPIDLEN+200;
					buffer=erealloc(buffer,lenbuffer+1);
				}
				sprintf(buffer, "%s/swishspider@%ld.contents", sw->tmpdir, (long)lgetpid());
				title=SafeStrCopy(title, (char *) (tmptitle=parsetitle(buffer, item->url)),&lentitle);
				efree(tmptitle);
			} else {
				if ((p=strrchr(item->url, '/')))
					{ title=SafeStrCopy(title, p + 1,&lentitle); }
				else
					{ title=SafeStrCopy(title, item->url,&lentitle); }
			}
		


	  		if((int)(strlen(sw->tmpdir)+MAXPIDLEN+30)>=lenbuffer) {
				lenbuffer=strlen(sw->tmpdir)+MAXPIDLEN+200;
				buffer=erealloc(buffer,lenbuffer+1);
			}
			sprintf(buffer, "%s/swishspider@%ld.contents", sw->tmpdir, (long)lgetpid());


			/* -- to retrieve last modification date of the
			   -- http document, the "get"-process has set to
			   -- the last mod timestamp on the tmp file
			   -- or we need to parse the server response...
			   -- $$ still to be done! (rasc) to then,
			   -- we are setting mtime to zero!
			*/


			fprop = file_properties (item->url, buffer, sw);
			fprop->mtime = 0;  /* $$ see above */

			/* Check http header for default filter 
 			   $$ original code:
			   $$  indextitleonly=strncmp(contenttype, "text/", 5);
			   $$ what this for?
  			   $$ seems to me historical and has to be removed...
			   $$ Why no getting a txt file?
			 */
			 fprop->index_no_content = strncmp(contenttype, "text/", 5);
			  /* $$ Jose: to discuss... and eventually be removed... */


			do_index_file(sw,fprop);
	



			free_file_properties (fprop);
                	if (fprop->fp) {



			
	
			}
			
			/* add new links
			**/
			if((int)(strlen(sw->tmpdir)+MAXPIDLEN+30)>=lenbuffer) {
				lenbuffer=strlen(sw->tmpdir)+MAXPIDLEN+200;
				buffer=erealloc(buffer,lenbuffer+1);
			}
			sprintf (buffer, "%s/swishspider@%ld.links", sw->tmpdir, (long)lgetpid());
			if ((fp = fopen(buffer, "r")) != NULL) {
				
			/* URLs can get quite large so don't depend on a fixed size buffer
				**/
				while ((link = readline(fp)) != NULL) {
					*(link + strlen(link) - 1) = '\0';
					urllist = add_url(sw, urllist, link, item->depth + 1, url);
				}
				fclose (fp);
			}
		} else if ((code / 100) == 3) {
			urllist = add_url(sw, urllist, contenttype, item->depth, url );
		}
		
		/* Clean up the files left by swishspider
		**/
		cmdf(unlink, "%s/swishspider@%ld.response", sw->tmpdir, lgetpid());
		cmdf(unlink, "%s/swishspider@%ld.contents", sw->tmpdir, lgetpid());
		cmdf(unlink, "%s/swishspider@%ld.links", sw->tmpdir, lgetpid());
    }
}

#endif


#ifdef _WIN32
#define strncasecmp	strnicmp
#endif


int http_parseconfline(SWISH *sw,char *line)
{
    int rv = 0;
    static char es[] = "equivalentserver";
    char *word;
    int skiplen,len;
    struct multiswline *list;
    struct swline *slist;
    char *StringValue;

	if (grabIntValueField(line, "maxdepth", &sw->maxdepth, 0))	{ rv = 1; }
	else if (grabIntValueField(line, "delay", &sw->delay, 0))	{ rv = 1; }
	else if ((StringValue=grabStringValueField(line, "spiderdirectory")))
	{
		sw->spiderdirectory = SafeStrCopy(sw->spiderdirectory,StringValue,&sw->lenspiderdirectory);
		len = strlen(sw->spiderdirectory);
		rv = 1;
		
		/* Make sure the directory has a trailing slash
		**/
		if (len && (sw->spiderdirectory[len - 1] != '/')) 
		{
			if(len == sw->lenspiderdirectory) 
				sw->spiderdirectory=erealloc(sw->spiderdirectory,++sw->lenspiderdirectory+1);
			strcat(sw->spiderdirectory, "/");
		}
		if(!isdirectory(sw->spiderdirectory))
		{
			sw->errorstr=BuildErrorString(sw->errorstr,&sw->lenerrorstr,"Error in SpiderDirectory. %s is not a directory\n.\n",sw->spiderdirectory);
			progerr(sw->errorstr);
		}
    	}
	else if (strncasecmp(line, es, sizeof(es) - 1) == 0) 
	{
		rv = 1;
		
		/* Add a new list of equivalent servers
		**/
		list = (struct multiswline *)emalloc(sizeof(struct multiswline));
		list->next = sw->equivalentservers;
		list->list = 0;
		sw->equivalentservers = list;
		
		line += (sizeof(es) - 1);
		while (*(word = getword(line, &skiplen)) != '\0') {
			/* Add a new entry to this list
			**/
			slist = (struct swline *)emalloc(sizeof(struct swline));
			slist->line = word;
			slist->next = list->list;
			list->list = slist;
			
			/* Move to the next word
			**/
			line += skiplen;
		}
    }
    return rv;
}

struct _indexing_data_source_def HTTPIndexingDataSource = {
  "HTTP-Crawler",
  "http",
  http_indexpath,
  http_parseconfline
};

