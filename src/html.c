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
**---------------------------------------------------------
** ** ** PATCHED 5/13/96, CJC
** Added MatchAndChange for regex in replace rule G.Hill 2/10/98
**
** change sprintf to snprintf to avoid corruption
** added safestrcpy() macro to avoid corruption from strcpy overflow
** SRE 11/17/99
**
** fixed cast to int problems pointed out by "gcc -Wall"
** SRE 2/22/00
**
** 
*/

#include "swish.h"
#include "mem.h"
#include "string.h"
#include "index.h"
#include "html.h"

/* The well known html entities */

static char *entities[] = 
 { "", "&#32", " ", "", "&#33", "!", "&quot",
"&#34", "\"", "", "&#35", "#", "", "&#36", "$", "", "&#37", "%",
"&amp", "&#38", "&", "", "&#39", "'", "", "&#43", "+", "", "&#44",
",", "", "&#45", "-", "", "&#46", ".", "", "&#47", "/", "", "&#48",
"0", "", "&#49", "1", "", "&#50", "2", "", "&#51", "3", "", "&#52",
"4", "", "&#53", "5", "", "&#54", "6", "", "&#55", "7", "", "&#56",
"8", "", "&#57", "9", "", "&#58", "", "", "&#59", ";", "&lt", "&#60",
"<", "", "&#61", "=", "&gt", "&#62", ">", "", "&#63", "?", "", "&#64",
"@", "", "&#65", "A", "", "&#66", "B", "", "&#67", "C", "", "&#68",
"D", "", "&#69", "E", "", "&#70", "F", "", "&#71", "G", "", "&#72",
"H", "", "&#73", "I", "", "&#74", "J", "", "&#75", "K", "", "&#76",
"L", "", "&#77", "M", "", "&#78", "N", "", "&#79", "O", "", "&#80",
"P", "", "&#81", "Q", "", "&#82", "R", "", "&#83", "S", "", "&#84",
"T", "", "&#85", "U", "", "&#86", "V", "", "&#87", "W", "", "&#88",
"X", "", "&#89", "Y", "", "&#90", "Z", "", "&#91", "[", "", "&#92",
"\\", "", "&#93", "]", "", "&#94", "^", "", "&#95", "-", "", "&#96",
"`", "", "&#97", "a", "", "&#98", "b", "", "&#99", "c", "", "&#100",
"d", "", "&#101", "e", "", "&#102", "f", "", "&#103", "g", "",
"&#104", "h", "", "&#105", "i", "", "&#106", "j", "", "&#107", "k",
"", "&#108", "l", "", "&#109", "m", "", "&#110", "n", "", "&#111",
"o", "", "&#112", "p", "", "&#113", "q", "", "&#114", "r", "",
"&#115", "s", "", "&#116", "t", "", "&#117", "u", "", "&#118", "v",
"", "&#119", "w", "", "&#120", "x", "", "&#121", "y", "", "&#122",
"z", "", "&#123", "{", "", "&#124", "|", "", "&#125", "}", "",
"&#126", "~", "&nbsp", "&#160", " ", "&iexcl", "&#161", "�", "&cent",
"&#162", "�", "&pound", "&#163", "�", "&curren", "&#164", "�", "&yen",
"&#165", "�", "&brvbar", "&#166", "�", "&sect", "&#167", "�", "&die",
"&#168", "�", "&copy", "&#169", "�", "&ordf", "&#170", "�", "&laquo",
"&#171", "�", "&not", "&#172", "�", "&shy", "&#173", "�", "&reg",
"&#174", "�", "&macron", "&#175", "�", "&degree", "&#176", "�",
"&plusmn", "&#177", "�", "&sup2", "&#178", "�", "&sup3", "&#179", "�",
"&acute", "&#180", "�", "&micro", "&#181", "�", "&mu", "&#182", "�",
"&middot", "&#183", "�", "&Cedilla", "&#184", "�", "&sup1", "&#185", "�",
"&ordm", "&#186", "�", "&raquo", "&#187", "�", "&frac14", "&#188", "�",
"&frac12", "&#189", "�", "&frac34", "&#190", "�", "&iquest", "&#191",
"�", "&Agrave", "&#192", "�", "&Aacute", "&#193", "�", "&Acirc",
"&#194", "�", "&Atilde", "&#195", "�", "&Auml", "&#196", "�",
"&Aring", "&#197", "�", "&AElig", "&#198", "�", "&Ccedil", "&#199",
"�", "&Egrave", "&#200", "�", "&Eacute", "&#201", "�", "&Ecirc",
"&#202", "�", "&Euml", "&#203", "�", "&Igrave", "&#204", "�",
"&Iacute", "&#205", "�", "&Icirc", "&#206", "�", "&Iuml", "&#207",
"�", "&ETH", "&#208", "�", "&Ntilde", "&#209", "�", "&Ograve", "&#210",
"�", "&Oacute", "&#211", "�", "&Ocirc", "&#212", "�", "&Otilde",
"&#213", "�", "&Ouml", "&#214", "�", "&times", "&#215", "�", "&Oslash",
"&#216", "�", "&Ugrave", "&#217", "�", "&Uacute", "&#218", "�",
"&Ucirc", "&#219", "�", "&Uuml", "&#220", "�", "&Yacute", "&#221",
"�", "&THORN", "&#222", "�", "&szlig", "&#223", "�", "&agrave", "&#224",
"�", "&aacute", "&#225", "�", "&acirc", "&#226", "�", "&atilde",
"&#227", "�", "&auml", "&#228", "�", "&aring", "&#229", "�", "&aelig",
"&#230", "�", "&ccedil", "&#231", "�", "&egrave", "&#232", "�",
"&eacute", "&#233", "�", "&ecirc", "&#234", "�", "&euml", "&#235",
"�", "&igrave", "&#236", "�", "&iacute", "&#237", "�", "&icirc",
"&#238", "�", "&iuml", "&#239", "�", "&eth", "&#240", "�", "&ntilde",
"&#241", "�", "&ograve", "&#242", "�", "&oacute", "&#243", "�",
"&ocirc", "&#244", "�", "&otilde", "&#245", "�", "&ouml", "&#246",
"�", "&divide", "&#247", "�", "&oslash", "&#248", "�", "&ugrave",
"&#249", "�", "&uacute", "&#250", "�", "&ucirc", "&#251", "�",
"&uuml", "&#252", "�", "&yacute", "&#253", "�", "&thorn", "&#254", "�",
"&yuml", "&#255", "�", NULL };

/* Extracts anything in <title> tags from an HTML file and returns it.
** Otherwise, only the file name without its path is returned.
*/

char *parsetitle(char *buffer, char *alttitle)
{
char *q,*shorttitle,*title;
	if ((q=strrchr(alttitle, '/'))) 
		q++;
	else
		q=alttitle;
	shorttitle = estrdup(q);
	
	if (!buffer) {
		return shorttitle;
	}

	if((title=parsetag("title", buffer, TITLETOPLINES, CASE_SENSITIVE_OFF)))
	{
		efree(shorttitle);
		return title;
	}

	return shorttitle;
}


/* Check if a particular title (read: file!) should be ignored
** according to the settings in the configuration file.
*/
/* This is to check "title contains" option in config file */

int isoktitle(sw,title)
SWISH *sw;
char *title;
{
	int badfile;
	struct swline *tmplist;
	
	badfile = 0;
	tmplist = sw->titconlist;
	while (tmplist != NULL) {
		if (matchARegex(title, tmplist->line)) {
			badfile = 1;
			break;
		}
		tmplist = tmplist->next;
	}
	if (badfile)
		return 0;
	else
		return 1;
}


/* This converts HTML numbered entities (such as &#169;)
** to strings (like &copy;). Much is this function is
** simply adding semicolons in the right places.
** This and the functions it calls are not very fast
** and could be made faster.
*/

char *convertentities(s)
char *s;
{
int lens, skip;
char *ent;
char *newword;
char *p,*q;

	if (!(p=strchr(s, '&'))) return s;
	lens = (int)strlen(s);

		/* Allocate enough memory */
	newword = (char *) emalloc(lens*2+1);

	q=newword;
	memcpy(q,s,p-s);
	q +=(p-s);

	for (s=p; *s != '\0'; ) {
		if (*s == '&') {
			ent = getent(s, &skip);
			if (ent[0] == '\0') {
				*q++=*s++;			
			} else {
				s += skip;
				if (*s == ';') s++;
				memcpy(q,ent,skip);
				q+=skip;
				*q++=';';
			}
			efree(ent);
		}
		else
			*q++=*s++;
	}
	*q='\0';
	if (ASCIIENTITIES) { 
			/* Jose Ruiz 06/00 Do not call to converttonamed
			** here. convertoascii do all the work 
			*/
		newword = (char *) converttoascii(newword);
	} else {
		newword = (char *) converttonamed(newword);
	}
	return newword;
}

/* Returns a matching entity that matches the beginning of a string, if any.
*/

char *getent(s, skip)
char *s;
int *skip;
{
int i;
int lenent;
char *ent;
int lentestent;
char *testent=NULL;

	ent = (char *)emalloc((lenent=MAXENTLEN) +1);

	*skip = 0;
	ent = SafeStrCopy(ent,s,&lenent);
	if (ent[1] == '#') {
		if (isdigit((int)ent[5])) {
			ent[0]='\0';
			return ent;
		}
		for (i = 2; ent[i] != '\0' && isdigit((int)ent[i]); i++);

		while (ent[i] != '\0' && !isdigit((int)ent[i])) ent[i++] = '\0';

		*skip = strlen(ent);
		return ent;
	}
	else {
		testent = (char *)emalloc((lentestent=MAXENTLEN) +1);
		for (i = 0; entities[i] != NULL; i += 3) {
			testent = SafeStrCopy(testent, entities[i],&lentestent);
			if (testent[0] != '\0') {
				if (!strncmp(testent, ent, strlen(testent))) {
					ent = SafeStrCopy(ent, testent,&lenent);
					*skip = strlen(ent);
					efree(testent);
					return ent;
				}
			}
		}
		efree(testent);
	}
	
	ent = SafeStrCopy(ent,"\0",&lenent);
	return ent;
}

/* This is the real function called by convertentities() that
** changes numbered to named entities.
*/

char *converttonamed(str)
char *str;
{
int i, hasnumbered, ilen;
int lentestent;
char *testent;
int lennewent;
char *newent;
char *newword;
	
	newword = (char *) estrdup(str);
	efree(str);
	testent = (char *) emalloc((lentestent=MAXENTLEN) + 1);
	newent = (char *) emalloc((lennewent=MAXENTLEN) + 1);
	
	do {
	   for (i = 0, hasnumbered = 0; entities[i] != NULL; i += 3) {
		ilen=strlen(entities[i+1]);
		if((ilen+1)>=lentestent) {
			lentestent=ilen+1+100;
			testent=erealloc(testent,lentestent+1);
		}
		memcpy(testent, entities[i + 1],ilen);
		testent[ilen]=';';
		testent[ilen+1]='\0';
		if (strstr(newword, testent) != NULL &&
			(entities[i])[0] != '\0') {
			hasnumbered=1;
			ilen=strlen(entities[i]);
			if((ilen+1)>=lennewent) {
				lennewent=ilen+1+100;
				newent=erealloc(newent,lennewent+1);
			}
			memcpy(newent,entities[i],ilen);
			newent[ilen]=';';
			newent[ilen+1]='\0';
			newword = (char *) replace(newword, testent, newent);
		}
	   }
	} while (hasnumbered);
	efree(testent);
	efree(newent);
	return newword;
}

/* This function converts all convertable named and numbered
** entities to their ASCII equivalents, if they exist.
*/

char *converttoascii(str)
char *str;
{
int i, hasnonascii,ilen;
char *c, *d;
int lenwrdent;
char *wrdent;
int lennument;
char *nument;
char *newword;

	newword = (char *) estrdup(str);
	efree(str);
	wrdent = (char *) emalloc((lenwrdent=MAXENTLEN) + 1);
	nument = (char *) emalloc((lennument=MAXENTLEN) + 1);
	
	do {
	   for (i = 0, hasnonascii = 0; entities[i] != NULL; i += 3) {
		ilen=strlen(entities[i]);
		if((ilen+1)>=lenwrdent) {
			lenwrdent=ilen+1+200;
			wrdent=erealloc(wrdent,lenwrdent+1);
		}
		memcpy(wrdent,entities[i],ilen);
		wrdent[ilen]=';';
		wrdent[ilen+1]='\0';
		ilen=strlen(entities[i+1]);
		if((ilen+1)>=lennument) {
			lennument=ilen+1+200;
			nument=erealloc(nument,lennument+1);
		}
		memcpy(nument,entities[i+1],ilen);
		nument[ilen]=';';
		nument[ilen+1]='\0';
		
		c = d = NULL;
		if ((entities[i])[0] != '\0')
			c = (char *) strstr(newword, wrdent);
		if ((entities[i + 1])[0] != '\0')
			d = (char *) strstr(newword, nument);
		if ((entities[i + 2])[0] != '\0' && (c!=NULL || d!=NULL)) {
			hasnonascii=1;
			if (c != NULL && d==NULL) { 
				newword = (char *) replace(newword, wrdent, entities[i + 2]); 
			} else if (d != NULL && c==NULL) { 
				newword = (char *) replace(newword, nument, entities[i + 2]); 
			} else {
				newword = (char *) replace(newword, wrdent, entities[i + 2]); 
				newword = (char *) replace(newword, nument, entities[i + 2]); 
			}
		}
	   }
	} while (hasnonascii);
	efree(wrdent);
	efree(nument);
	return newword;
}



/* Indexes all the words in a html file and adds the appropriate information
** to the appropriate structures.
*/

int countwords_HTML(SWISH *sw, FileProp *fprop, char *buffer)
{
int ftotalwords;
int *metaName;
int metaNamelen;
int *positionMeta;    /* Position of word in file */
int tmpposition=1;    /* Position of word in file */
int currentmetanames;
int n;
char *p, *newp, *tag, *endtag;
int structure;
struct file *thisFileEntry = NULL;
int metaNameOld,metaNameXML;
int i, docPropName;
IndexFILE *indexf=sw->indexlist;
char *summary=NULL;
char *title=parsetitle(buffer,fprop->real_path);

	if(!isoktitle(sw,title))
	{
		efree(title);
		return -2;
	}
	sw->filenum++;
	
	if (fprop->index_no_content) {
		addtofilelist(sw,indexf, fprop->real_path, title, summary, 0, fprop->fsize, NULL);
		addtofwordtotals(indexf, sw->filenum, 100);
		if(sw->swap_flag)
			SwapFileData(sw, indexf->filearray[sw->filenum-1]);
		n=countwordstr(sw, title, sw->filenum);
		efree(title);
		return n;
	}


	if(fprop->stordesc)
	{
			summary=parseHtmlSummary(buffer,fprop->stordesc->field,fprop->stordesc->size);
	}

	addtofilelist(sw,indexf, fprop->real_path, title, summary, 0, fprop->fsize, &thisFileEntry);

		/* Init meta info */
	metaName=(int *)emalloc((metaNamelen=1)*sizeof(int));
	positionMeta =(int *)emalloc(metaNamelen*sizeof(int));
	currentmetanames=ftotalwords=0;
	structure=IN_FILE;
	metaName[0]=1; positionMeta[0]=1;

	for(p=buffer;p && *p;) {
		if((tag=strchr(p,'<')) && ((tag==p) || (*(tag-1)!='\\'))) {   /* Look for non escaped '<' */
				/* Index up to the tag */
			*tag++='\0';
			newp=convertentities(p);
			ftotalwords +=indexstring(sw, newp, sw->filenum, structure, currentmetanames, metaName, positionMeta);
			if(newp!=p) efree(newp);
				/* Now let us look for a not escaped '>' */
			for(endtag=tag;;) 
				if((endtag=strchr(endtag,'>'))) 
				{
					if (*(endtag-1)!='\\') break; 
					else endtag++; 
				} else break;
			if(endtag) {  
				*endtag++='\0';
				structure = getstructure(tag,structure);
				if((tag[0]=='!') && lstrstr(tag,"META") && (lstrstr(tag,"START") || lstrstr(tag,"END"))) {    /* Check for META TAG TYPE 1 */
					if(lstrstr(tag,"START")) {
						if((metaNameOld=getMeta(indexf,tag,&docPropName,&sw->applyautomaticmetanames,sw->verbose))!=1) {
							/* realloc memory if needed */
							if(currentmetanames==metaNamelen) {metaName=(int *) erealloc(metaName,(metaNamelen*=2)*sizeof(int));positionMeta=(int *) erealloc(positionMeta,metaNamelen*sizeof(int));}
							/* add netaname to array of current metanames */
							metaName[currentmetanames]=metaNameOld;
							/* Preserve position */
							if(!currentmetanames) tmpposition=positionMeta[0];
							/* Init word counter for the metaname */
							positionMeta[currentmetanames++] = 1;
							p=endtag;
							/* If it is also a property store it until a < is found */
							if(docPropName) {
					     			if((endtag=strchr(p,'<'))) *endtag='\0';
					    			 addDocProperty(&thisFileEntry->docProperties,docPropName,p);
					     			if(endtag) *endtag='<';
							} 
						}

					}
					else if(lstrstr(tag,"END")) {
						/* this will close the last metaname */
						if(currentmetanames) {
							currentmetanames--;
							if(!currentmetanames) {
						    		metaName[0] = 1;
								/* Restore position counter */
						    		positionMeta[0] = tmpposition;
							}
						}	
						p=endtag;
					}
				} /* Check for META TAG TYPE 2 */
				else if((tag[0]!='!') && lstrstr(tag,"META") && lstrstr(tag,"NAME") && lstrstr(tag,"CONTENT")) { 
					ftotalwords +=parseMetaData(sw,indexf,tag,sw->filenum,structure,thisFileEntry);
					p=endtag;
				}  /* Check for XML field */
				else if ((tag[0]!='!') && ((metaNameXML=getXMLField(indexf,tag,&docPropName,&sw->applyautomaticmetanames,sw->verbose))!=1)) {
					/* realloc memory if needed */
					if(currentmetanames==metaNamelen) {metaName=(int *) erealloc(metaName,(metaNamelen *=2) *sizeof(int));positionMeta=(int *) erealloc(positionMeta,metaNamelen*sizeof(int));}
					/* add netaname to array of current metanames */
					metaName[currentmetanames]=metaNameXML;
					/* Preserve position counter */
					if(!currentmetanames) tmpposition=positionMeta[0];
					/* Init word counter for the metaname */
					positionMeta[currentmetanames++] = 1;
					p=endtag;
					/* If it is also a property doc store it
					** Only store until a < is found */
					if(docPropName) {
					     if((endtag=strchr(p,'<'))) *endtag='\0';
					     addDocProperty(&thisFileEntry->docProperties,docPropName,p);
					     if(endtag) *endtag='<';
					} 
				}  /* Check for end of a XML field */
				else if((tag[0]=='/') && ((metaNameXML=getXMLField(indexf,tag+1,&docPropName,&sw->applyautomaticmetanames,sw->verbose))!=1)) {
					/* search for the metaname in the
				        ** list of currentmetanames */
					if(currentmetanames) {
			        	   	for(i=currentmetanames-1;i>=0;i--) if(metaName[i]==metaNameXML) break;
						if(i>=0) currentmetanames=i;
						if(!currentmetanames) {
						    metaName[0] = 1;
							/* Restore position counter */
						    positionMeta[0] = tmpposition;
						}
					}	
					p=endtag;
				}  /*  Check for COMMENT */
				else if ((tag[0]=='!') && sw->indexComments) {
					ftotalwords +=parsecomment(sw,tag,sw->filenum,structure,1,positionMeta);
					p=endtag;
				}    /* Default: Continue */
				else {    
					p=endtag;
				}
			} else p=tag;    /* tag not closed: continue */
		} else {    /* No more '<' */
			newp=convertentities(p);
			ftotalwords +=indexstring(sw, newp, sw->filenum, structure, currentmetanames, metaName, positionMeta);
			if(newp!=p) efree(newp);
			p=NULL;
		}
	}
	efree(metaName);
	efree(positionMeta);
	addtofwordtotals(indexf, sw->filenum, ftotalwords);
	if(sw->swap_flag)
		SwapFileData(sw, indexf->filearray[sw->filenum-1]);
	efree(title);
	return ftotalwords;
}



/* This returns the value corresponding to the HTML structures
** a word is in.
*/

int getstructure(tag, structure)
char *tag;
int structure;
{

        /* int len; */ /* not used - 2/22/00 */
        char oldChar = 0;
        char* endOfTag = NULL;
        char* pos;

	pos = tag;
        while (*pos)
	  {
                if (isspace((int)((unsigned char)*pos)))
		  {
                        endOfTag = pos; /* remember where we are... */
                        oldChar = *pos; /* ...and what we saw */
                        *pos = '\0';    /* truncate string, for now */
		      }
		else 
		  pos++;
	      }
       /*      Store Word Context
       **      Modified DLN 1999-10-24 - Comments and Cleaning
       **  TODO: Make sure that these allow for HTML attributes
       **/

       /* HEAD  */
        if (strcasecmp(tag, "/head") == 0)
                structure &= ~IN_HEAD;                        /* Out  */
        else if (strcasecmp(tag, "head") == 0)
                structure |= IN_HEAD;                 /* In  */
       /* TITLE  */
        else if (strcasecmp(tag, "/title") == 0)
                structure &= ~IN_TITLE;
        else if (strcasecmp(tag, "title") == 0)
                structure |= IN_TITLE;
	/* BODY */
        else if (strcasecmp(tag, "/body") == 0)
                structure &= ~IN_BODY;			/* In */
        else if (strcasecmp(tag, "body") == 0)
                structure |= IN_BODY;			/* Out */
	/* H1, H2, H3, H4, H5, H6  */
        else if (tag[0] == '/' && tolower(tag[1]) == 'h' && isdigit((int)tag[2])) /* cast to int - 2/22/00 */
                structure &= ~IN_HEADER;              /* In */
        else if (tolower(tag[0]) == 'h' && isdigit((int)tag[1])) /* cast to int - 2/22/00 */
                structure |= IN_HEADER;			/* Out */
	/* EM, STRONG  */
        else if ((strcasecmp(tag, "/em") == 0) || (strcasecmp(tag, "/strong") == 0))
                structure &= ~IN_EMPHASIZED; 		/* Out */
        else if ((strcasecmp(tag, "em") == 0) || (strcasecmp(tag, "strong") == 0))
                structure |= IN_EMPHASIZED;		/* In */
	/* B, I are seperate for semantics  */
        else if ((strcasecmp(tag, "/b") == 0) || (strcasecmp(tag, "/i") == 0))
                structure &= ~IN_EMPHASIZED;		/* Out */
        else if ((strcasecmp(tag, "b") == 0) || (strcasecmp(tag, "i") == 0))
                structure |= IN_EMPHASIZED;		/* In */
	/* The End  */	

        if (endOfTag != NULL)
	  {
                *endOfTag = oldChar;
	      }
        return structure;
}



/* Get the MetaData index when the whole tag is passed */

/* Patch by Tom Brown */
/* TAB, this routine is/was somewhat pathetic... but it was pathetic in
 1.2.4 too ... someone needed a course in defensive programming... there are
 lots of tests below for temp != NULL, but what is desired is *temp != '\0'
 (e.g. simply *temp) ... I'm going to remove some strncmp(temp,constant,1)
 which are must faster as *temp != constant ...

 Anyhow, the test case I've got that's core dumping is:
    <META content=3D"MSHTML 5.00.2614.3401" name=3DGENERATOR>
 no trailing quote, no trailing space... and with the missing/broken check for+  end of string it scribbles over the stack...

*/

int getMeta(indexf, tag, docPropName, applyautomaticmetanames, verbose)
IndexFILE *indexf;
char* tag;
int* docPropName;
int *applyautomaticmetanames;
int verbose;
{
char* temp;
static int lenword=0;
static char *word=NULL;
int i;
struct metaEntry* list=NULL;
	
	if(!lenword) word =(char *)emalloc((lenword=MAXWORDLEN)+1);

	if (docPropName != NULL)
	{
		*docPropName = 0;
	}
	
	temp = (char*) lstrstr((char*)tag,(char*) "NAME");
	if (temp == NULL)
		return 1;
	
	temp += strlen("NAME");
	
	/* Get to the '=' sign disreguarding blanks */
	while (temp != NULL && *temp) {
		if (*temp && (*temp != '='))  /* TAB */
			temp++;
		else {
			temp++;
			break;
		}
	}
	
	/* Get to the beginning of the word disreguarding blanks and quotes */
	/* TAB */
	while (temp != NULL && *temp) {
		if (*temp == ' ' || *temp == '"' )
			temp++;
		else
			break;
	}
	
	/* Copy the word and convert to lowercase */
	/* TAB */
	/* while (temp !=NULL && strncmp(temp," ",1) */
	/*	&& strncmp(temp,"\"",1) && i<= MAXWORDLEN ) { */

	/* and the above <= was wrong, should be < which caused the
	   null insertion below to be off by two bytes */

	for (i=0;temp !=NULL && *temp && *temp != ' '
		&& *temp != '"' ;) {
		if (i==lenword) {
			lenword *=2;
			word= (char *) erealloc(word,lenword+1);
		}
		word[i] = *temp++;
		word[i] = tolower(word[i]);
		i++;
	}
	if (i==lenword) {
		lenword *=2;
		word= (char *) erealloc(word,lenword+1);
	}
	word[i] = '\0';

	while(1) {
		for (list = indexf->metaEntryList; list != NULL; list = list->next)
		{
			if (!strcmp(list->metaName, word) )
			{
				if ((docPropName != NULL) && (list->isDocProperty))
				{
					*docPropName = list->index;
				}
				if (list->isOnlyDocProperty)
				{
					if (*applyautomaticmetanames) list->isOnlyDocProperty=0;
					else 
					/* property is not for indexing, so return generic metaName value */
						return 1;
				}
				return list->index;
			}
		}
		/* 06/00 Jose Ruiz
		** If automatic MetaNames enabled add the metaName
		** else break
		*/
		if(*applyautomaticmetanames) {
			if (verbose) 
				printf("\nAdding automatic MetaName %s\n",word);
			addMetaEntry(indexf,word,0,applyautomaticmetanames); 
		} else break;
	}
	/* If it is ok not to have the name listed, just index as no-name */
	if (OKNOMETA) {
		/*    printf ("\nwarning: metaName %s does not exiest in the user config file", word); */
		return 1;
	}
	else {
		printf ("\nerr: INDEXING FAILURE\n");
		printf ("err: The metaName %s does not exist in the user config file\n", word);
		exit(0);
	}
	
}

/* Parses the Meta tag */
int parseMetaData(sw, indexf, tag, filenum, structure, thisFileEntry)
SWISH *sw;
IndexFILE *indexf;
char* tag;
int filenum;
int structure;
struct file* thisFileEntry;
{
int metaName, jstart;
char *temp, *convtag;
int docPropName = 0;
int position=1; /* position of word */
int wordcount=0; /* Word count */
	temp = NULL;
	metaName= getMeta(indexf, tag, &docPropName, &sw->applyautomaticmetanames,sw->verbose);

	/* 10/11/99 - Bill Moseley - don't index meta tags not specified in MetaNames */
	if ( REQMETANAME && metaName == 1 ) return 0;

	temp = (char*) lstrstr((char*) tag,(char*) "CONTENT");
	
	/* if there is no  CONTENT is another tag so just ignore the whole thing
	* the check is done here istead of before because META tags do not have
	* a fixed length that can be checked
	*/
	if (temp != NULL && *temp) {
		temp += strlen("CONTENT");
		
		/* Get to the " sign disreguarding other characters */
		while (temp != NULL && *temp) {
			if (*temp != '"')
				temp++;
			else {
				temp++;
				break;
			}
		}
		
		jstart = strlen(tag) - strlen(temp);
		
		structure |= IN_COMMENTS;

		/* Locate the end of MetaTag */
		temp = strchr(tag + jstart, '\"'); /* first quote after start of CONTENT */
		/* Jump escaped \" */
		while(temp && *temp) 
			if(*(temp-1)=='\\') temp=strchr(temp+1,'\"');
			else break;
		if (temp != NULL) {
			*temp = '\0';	/* terminate CONTENT, temporarily */
			if(docPropName)
				addDocProperty(&thisFileEntry->docProperties, docPropName, tag+jstart);
			convtag = (char *)convertentities(tag + jstart);
			
			wordcount = indexstring(sw, convtag , filenum, structure, 1, &metaName, &position);
			if(convtag!=(tag + jstart)) efree(convtag);
			*temp = '\"';	/* restore string */
		} else {
			convtag = (char *)convertentities(tag + jstart);
			wordcount=indexstring(sw, convtag, filenum, structure, 1, &metaName, &position);
			if(convtag!=(tag + jstart)) efree(convtag);
		}
	}
	return wordcount;
}

char *parseHtmlSummary(char *buffer,char *field,int size)
{
char *p,*q,*tag,*endtag,c;
char *summary,*convsummary,*beginsum,*endsum,*tmp,*tmp2,*tmp3;
int found,lensummary;
		/* Get the summary if no metaname/field is given */
	if(!field && size)
	{
		/* Jump title if it exists */
		if((p=lstrstr(buffer,"</title>")))
		{
			p+=8;
		} else p=buffer;
		/* Let us try to find <body> */
		if((q=lstrstr(p,"<body>")))
		{
			q+=6;
		} else q=p;
		tmp=estrdup(p);
		remove_newlines(tmp);
		remove_tags(tmp);
		convsummary=convertentities(tmp);
		if(convsummary!=tmp) efree(tmp); 
		tmp=convsummary;

		/* use only the required memory -save those not used */
		summary=estrdup(tmp);
		efree(tmp);
		return summary;
	}
	for(p=buffer,summary=NULL,found=0,beginsum=NULL,endsum=NULL;p && *p;)
	{
		if((tag=strchr(p,'<')) && ((tag==p) || (*(tag-1)!='\\'))) {   /* Look for non escaped '<' */
			tag++;
			for(endtag=tag;;) 
				if((endtag=strchr(endtag,'>'))) 
				{
					if (*(endtag-1)!='\\') break; 
					else endtag++; 
				} else break;
			if(endtag) {  
				c=*endtag;
				*endtag++='\0';
				if((tag[0]=='!') && lstrstr(tag,"META") && (lstrstr(tag,"START") || lstrstr(tag,"END"))) 
				{    /* Check for META TAG TYPE 1 */
					if(lstrstr(tag,"START")) {
						if((tmp=lstrstr(tag,"NAME")))
						{
							tmp+=4;
							if(lstrstr(tmp,field))
							{
								beginsum=endtag;
								found=1;
							}
							p=endtag;
						}
					}
					else if(lstrstr(tag,"END")) {
						if(!found)
						{
							p=endtag;
						}
						else
						{
							endsum=tag-1;	
							*(endtag-1)=c;
							break;
						}
					}
				} /* Check for META TAG TYPE 2 */
				else if((tag[0]!='!') && lstrstr(tag,"META") && (tmp=lstrstr(tag,"NAME")) && (tmp2=lstrstr(tag,"CONTENT"))) { 
					tmp+=4;
					tmp3=lstrstr(tmp,field);
					if(tmp3 && tmp3<tmp2)
					{
						tmp2+=7;
						if((tmp=strchr(tmp2,'=')))
						{
							for(++tmp;isspace((int)((unsigned char)*tmp));tmp++);
							if(*tmp=='\"')
							{
								beginsum=tmp+1;
								for(tmp=endtag-1;tmp>beginsum;tmp--) if(*tmp=='\"') break;
								if(tmp==beginsum) endsum=endtag-1;
								else endsum=tmp;
							}
							else 
							{
								beginsum = tmp;
								endsum = endtag-1;
							}
							found=1;
							*(endtag-1)=c;
							break;

						}
					}
					p=endtag;
				}    /* Default: Continue */
				else 
				{    
					p=endtag;
				}
			} else p=NULL;    /* tag not closed ->END */
			*(endtag-1)=c;
		} else {    /* No more '<' */
			p=NULL;
		}
	}
	if(found && beginsum && endsum && endsum>beginsum)
	{
		lensummary=endsum-beginsum;
		summary=emalloc(lensummary+1);
		memcpy(summary,beginsum,lensummary);
		summary[lensummary]='\0';
	}
		/* If field is set an no metaname is found, let us search */
		/* for something like <field>bla bla </field> */
	if(!summary && field)
        {
                summary=parsetag(field, buffer, 0,CASE_SENSITIVE_OFF);
	}
		/* Finally check for something after title (if exists) and */
		/* after <body> (if exists) */
	if(!summary)
	{
		/* Jump title if it exists */
		if((p=lstrstr(buffer,"</title>")))
		{
			p+=8;
		} else p=buffer;
		/* Let us try to find <body> */
		if((q=lstrstr(p,"<body>")))
		{
			q+=6;
		} else q=p;
		summary=estrdup(q);
	}
	if(summary)
	{
		remove_newlines(summary);
		remove_tags(summary);
		convsummary=convertentities(summary);
		if(convsummary!=summary) efree(summary);
		summary=convsummary;
        }
        if(summary && size && strlen(summary)>size) 
                summary[size]='\0';
	return summary;
}

