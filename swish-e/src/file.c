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
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**-------------------------------------------------------------
** Changed getdefaults to allow metaNames in the user
** configuration file
** G.Hill 4/16/97 ghill@library.berkeley.edu
**
** change sprintf to snprintf to avoid corruption, and use MAXSTRLEN from swish.h
** added safestrcpy() macro to avoid corruption from strcpy overflow
** SRE 11/17/99
**
** added buffer size arg to grabStringValue - core dumping from overrun
** fixed logical OR and other problems pointed out by "gcc -Wall"
** SRE 2/22/00
** 
** counter modulo 128 had parens typo
** SRE 2/23/00
**
** read stopwords from file
** Rainer Scherg (rasc)  2000-06-15
** 
** 2000-11-15 rasc
** file_properties retrieves last mod date, filesize, and evals some swish
** config flags for this file!
**
** 2001-02-12 rasc   errormsg "print" changed...
** 2001-03-16 rasc   truncateDoc [read_stream] (if doc to large, truncate... )
** 2001-03-17 rasc   fprop enhanced by "real_filename"
**
*/

#include "swish.h"
#include "mem.h"
#include "string.h"
#include "file.h"
#include "error.h"
#include "list.h"
#include "hash.h"
#include "check.h"
#include "index.h"
#include "filter.h"
#include "metanames.h"


/* Is a file a directory?
*/

int     isdirectory(char *path)
{
    struct stat stbuf;

    if (stat(path, &stbuf))
        return 0;
    return ((stbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
}

/* Is a file a regular file?
*/

int     isfile(char *path)
{
    struct stat stbuf;

    if (stat(path, &stbuf))
        return 0;
    return ((stbuf.st_mode & S_IFMT) == S_IFREG) ? 1 : 0;
}

/* Is a file a link?
*/

int     islink(char *path)
{
#ifndef NO_SYMBOLIC_FILE_LINKS
    struct stat stbuf;

    if (lstat(path, &stbuf))
        return 0;
    return ((stbuf.st_mode & S_IFLNK) == S_IFLNK) ? 1 : 0;
#else
    return 0;
#endif
}

/* Get the size, in bytes, of a file.
** Return -1 if there's a problem.
*/

int     getsize(char *path)
{
    struct stat stbuf;

    if (stat(path, &stbuf))
        return -1;
    return stbuf.st_size;
}


/* Checks that all the regex in the replace list are correct */
void    checkReplaceList(SWISH * sw)
{
    struct swline *tmpReplace;
    char   *rule;
    int     lenpatt;
    char   *patt;
    regex_t re;
    int     status;

/*$$$ lstrstr is certainly wrong...  check if strcasecmp should be used */

    patt = (char *) emalloc((lenpatt = MAXSTRLEN) + 1);
    tmpReplace = sw->replacelist;
    while (tmpReplace)
    {
        rule = tmpReplace->line;

        /*  If it is not replace, just do nothing */
        if (lstrstr(rule, "append") || lstrstr(rule, "prepend"))
        {
            if (tmpReplace->next)
            {
                tmpReplace = tmpReplace->next;
            }
            else
            {
                efree(patt);
                return;
            }
        }
        if (lstrstr(rule, "replace"))
        {
            tmpReplace = tmpReplace->next;
            patt = SafeStrCopy(patt, tmpReplace->line, &lenpatt);
            status = regcomp(&re, patt, REG_EXTENDED);
            regfree(&re);
                 /** Marc Perrin ## 18Jan99 **/
            if (status != 0)
            {
                printf("Illegal regular expression %s\n", patt);
                efree(patt);
                exit(0);
            }

            if (tmpReplace->next)
                tmpReplace = tmpReplace->next;
            else
            {
                efree(patt);
                return;
            }
        }
        tmpReplace = tmpReplace->next;
    }
    efree(patt);
}


FILE   *openIndexFILEForWrite(char *filename)
{
    return fopen(filename, FILEMODE_WRITE);
}

FILE   *openIndexFILEForRead(char *filename)
{
    return fopen(filename, FILEMODE_READ);
}

FILE   *openIndexFILEForReadAndWrite(char *filename)
{
    return fopen(filename, FILEMODE_READWRITE);
}

void    CreateEmptyFile(char *filename)
{
    FILE   *fp;

    if (!(fp = openIndexFILEForWrite(filename)))
    {
        progerr("Couldn't write the file \"%s\".", filename);
    }
    fclose(fp);
}

/*
 * Invoke the methods of the current Indexing Data Source
 */
void    indexpath(SWISH * sw, char *path)
{
    /* invoke routine to index a "path" */
    (*IndexingDataSource->indexpath_fn) (sw, path);
}


/*
  -- read file into a buffer
  -- truncate file if necessary (truncateDocSize)
  -- return: buffer
  -- 2001-03-16 rasc    truncateDoc
*/

char   *read_stream(FILE * fp, long filelen, long max_size)
{
    long    c,
            offset;
    long    bufferlen;
    unsigned char *buffer;


    if (filelen)
    {

        /* truncate doc? */
        if (max_size && (filelen > max_size))
        {
            filelen = max_size;
        }

        buffer = emalloc(filelen + 1);
        *buffer = '\0';
        fread(buffer, 1, filelen, fp);

    }
    else
    {                           /* if we are reading from a popen call, filelen is 0 */

        buffer = emalloc((bufferlen = RD_BUFFER_SIZE) + 1);
        *buffer = '\0';
        for (offset = 0; (c = fread(buffer + offset, 1, RD_BUFFER_SIZE, fp)) == RD_BUFFER_SIZE; offset += RD_BUFFER_SIZE)
        {
            /* truncate? break if to much read */
            if (max_size && (bufferlen > max_size))
            {
                break;
            }
            bufferlen += RD_BUFFER_SIZE;
            buffer = erealloc(buffer, bufferlen + 1);
        }
        filelen = offset + c;

        if (max_size && (filelen > max_size))
        {
            filelen = max_size;
        }
    }
    buffer[filelen] = '\0';
    return (char *) buffer;
}

/* Mar 27, 2001 - moseley
 * Separate out the creation of the file properties
 *
 */

FileProp *init_file_properties(SWISH * sw)
{
    FileProp *fprop;

    fprop = (FileProp *) emalloc(sizeof(FileProp));
    /* emalloc checks fail and aborts... */

    /* -- init -- */
    fprop->fp = (FILE *) NULL;
    fprop->real_path = (char *) NULL;  /* path to real file, or URL */
    fprop->work_path = (char *) NULL;  /* path to work file (can be real file with fs, or local tmp file for http) */
    fprop->real_filename = (char *) NULL;
    fprop->fsize = 0;
    fprop->mtime = (time_t) 0;
    fprop->doctype = sw->DefaultDocType;
    fprop->hasfilter = NULL;    /* Default = No Filter */
    fprop->stordesc = NULL;     /* Default = No summary */
    fprop->index_no_content = 0; /* former: was indextitleonly! */

    return fprop;
}


/* Mar 27, 2001 - moseley
 * Separate out the adjusting of file properties by config settings
 * 2001-04-09  rasc changed filters
 */

void    init_file_prop_settings(SWISH * sw, FileProp * fprop)
{

    /* Basename of document path => document filename */
    fprop->real_filename = str_basename(fprop->real_path);


    /* -- get Doc Type as is in IndexContents or Defaultcontents
       -- doctypes by jruiz
     */

    fprop->doctype = getdoctype(fprop->real_path, sw->indexcontents);
    if (fprop->doctype == NODOCTYPE && sw->DefaultDocType != NODOCTYPE)
    {
        fprop->doctype = sw->DefaultDocType;
    }


    /* -- index just the filename (or doc title tags)?
       -- this param was "wrongly" named indextitleonly */

    fprop->index_no_content = (sw->nocontentslist != NULL) && isoksuffix(fprop->real_path, sw->nocontentslist);

    /* -- Any filter for this file type?
       -- NULL = No Filter, (char *) path to filter prog.
     */

    fprop->hasfilter = hasfilter(sw, fprop->real_path);

    fprop->stordesc = hasdescription(fprop->doctype, sw->storedescription);

}



/*
  -- file_properties
  -- Get/eval information about a file and return it.
  -- Some flags are calculated from swish configs for this "real_path"
  -- Structure has to be freed using free_file_properties
  -- 2000-11-15 rasc
  -- return: (FileProp *)
  -- A failed stat returns an empty (default) structure

  -- 2000-12
  -- Added StoreDescription
*/

FileProp *file_properties(char *real_path, char *work_file, SWISH * sw)
{
    FileProp *fprop;
    struct stat stbuf;

    /* create an initilized fprop structure */
    
    fprop = init_file_properties(sw);


    fprop->real_path = real_path;
    fprop->work_path = (work_file) ? work_file : real_path;


    /* Stat the file */
    /* This is really the wrong place for this, as it's really only useful for fs.c method */
    /* for http.c it means the last mod date is the temp file date */
    /* Probably this entire function isn't needed - moseley */

    if (!stat(fprop->work_path, &stbuf))
    {
        fprop->fsize = (long) stbuf.st_size;
        fprop->mtime = stbuf.st_mtime;
    }


    /* Now set various fprop settings based mostly on file name */
    
    init_file_prop_settings(sw, fprop);



#ifdef DEBUG
    fprintf(stderr, "file_properties: path=%s, (workpath=%s), fsize=%ld, last_mod=%ld Doctype: %d Filter: %p\n",
            fprop->real_path, fprop->work_path, (long) fprop->fsize, (long) fprop->mtime, fprop->doctype, fprop->filterprog);
#endif

    return fprop;
}


/* -- Free FileProp structure
   -- unless no alloc for strings simple free structure
*/

void    free_file_properties(FileProp * fprop)
{
    efree(fprop);
}
