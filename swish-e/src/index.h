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
*/


void do_index_file (SWISH *sw, FileProp *fprop, char *title);

int getMeta (IndexFILE *, char *tag, int* docPropName, int *, int);
int parseMetaData (SWISH *, IndexFILE *, char *, int, int, struct file*);
void printMetaNames (IndexFILE *);
DOCENTRYARRAY *addsortentry (DOCENTRYARRAY *, char*, char*);
ENTRYARRAY *addentry (SWISH *, ENTRYARRAY *, char*, int, int, int, int );
void addtofilelist (SWISH *,IndexFILE *indexf, char *filename, char *title, int start, int size, struct file ** newFileEntry);
int getfilecount (IndexFILE *);
char *getthedate (void);
int countwords (SWISH *, FileProp *, char *title, char *buffer);
int countwordstr (SWISH *, char *, int);
int getstructure (char *, int);
int parsecomment (SWISH *, char *, int, int, int, int *);
int removestops (SWISH *, ENTRYARRAY *, int);
int getrank _AP ((SWISH *, int, int, int, int));
void printheader _AP ((INDEXDATAHEADER *, FILE *, char *, int, int, int));
void printindex _AP ((SWISH *, IndexFILE *));
void printword _AP ((SWISH *, ENTRY *, IndexFILE *));
void printworddata _AP ((SWISH *, ENTRY *, IndexFILE *));
void printhash _AP ((ENTRY **, IndexFILE *));
void printfilelist _AP ((SWISH *, IndexFILE *));
void printstopwords _AP ((IndexFILE *));
void printfileoffsets _AP ((IndexFILE *));
void printlocationlookuptables _AP ((IndexFILE *));
void printpathlookuptable _AP ((IndexFILE *));
void decompress _AP ((SWISH *, IndexFILE *));
char *ruleparse _AP ((SWISH *, char *));
void stripIgnoreFirstChars _AP ((INDEXDATAHEADER, char *));
int stripIgnoreLastChars _AP ((INDEXDATAHEADER, char *));

#define isIgnoreFirstChar(header,c) header.ignorefirstcharlookuptable[(int)((unsigned char)c)]
#define isIgnoreLastChar(header,c) header.ignorelastcharlookuptable[(int)((unsigned char)c)]
#define isBumpPositionCounterChar(header,c) header.bumpposcharslookuptable[(int)((unsigned char)c)]

unsigned char *buildFileEntry _AP ((char *,char *,int, int, FILE *, struct docPropertyEntry **, int, int *));
struct file *readFileEntry _AP ((IndexFILE *,int, int));

void computehashentry _AP ((ENTRY **,ENTRY *));
void TranslateChars _AP ((INDEXDATAHEADER, char *));

void sortentry _AP ((SWISH *, IndexFILE *, ENTRY *));

int indexstring _AP ((SWISH*, char *, int, int, int, int *, int *));

void addtofwordtotals _AP ((IndexFILE *, int, int));

