
/*
   -- This module does metaname handling for swish-e
   -- 
   -- License: see swish licence file


   -- 2001-02-12 rasc    minor changes, concering the tolower problem 
				 (unsigned char) problem!!!

*/




#include "swish.h"
#include "mem.h"
#include "merge.h"
#include "string.h"
#include "docprop.h"
#include "metanames.h"

typedef struct 
{
char *metaName;
int metaType;   /* see metanames.h for values. All values must be "ored" */
} defaultMetaNames;

/* List of the internal metanames */
/********************************************************/
/* IMPORTANT: DO NOT REMOVE ANY OF THE DEFAULTMATANAMES */
/* Change them if you like but do not remove them       */
/********************************************************/
static defaultMetaNames SwishDefaultMetaNames[]={
    {AUTOPROPERTY_DOCPATH, META_PROP}, /* Filename No index (META_INDEX is not set) */
				/* If you want filename indexed and searchable*/
				/* just add META_INDEX to it */
    {AUTOPROPERTY_TITLE, META_PROP}, /* Title No index (META_INDEX is not set) */
				/* If you want title indexed and searchable */
				/* just add META_INDEX to it */
    {AUTOPROPERTY_LASTMODIFIED, META_PROP | META_DATE},     /* File date */
    {AUTOPROPERTY_STARTPOS, META_PROP | META_NUMBER},     /* File start */
    {AUTOPROPERTY_DOCSIZE, META_PROP | META_NUMBER},     /* File size */
    {AUTOPROPERTY_SUMMARY, META_PROP}, /* Summary No index (META_INDEX is not set) */
				/* If you want summary indexed and searchable */
				/* just add META_INDEX to it */
    {NULL,0}                    /* End mark */
};

/* Add the Internal swish metanames to the index file structure */
void add_default_metanames(IndexFILE *indexf)
{
int i,dummy,metaType;
char *metaName;
	for(i=0;SwishDefaultMetaNames[i].metaName;i++)
	{
		metaName=estrdup(SwishDefaultMetaNames[i].metaName);
		metaType=SwishDefaultMetaNames[i].metaType;
		addMetaEntry(indexf,metaName,metaType,&dummy);
		efree(metaName);
	}
}


/* Returns the structure associated with the metaName if it exists
*/

struct metaEntry * getMetaNameData(IndexFILE *indexf, char *word)
{
int i;
	for (i=0;i<indexf->metaCounter; i++) 
		if (!strcmp(indexf->metaEntryArray[i]->metaName, word))
			return indexf->metaEntryArray[i];
	return NULL;
}

/* Returns the structure associated with the metaName ID if it exists
*/

struct metaEntry * getMetaIDData(IndexFILE *indexf, int number)
{
	number--; 
	number--;
	if(number<indexf->metaCounter)
		return indexf->metaEntryArray[number];
	else
		return NULL;
}



/* Add an entry to the metaEntryArray with the given value and the
** appropriate index
*/
/* #### Changed the name isDocProp by metaType */
void addMetaEntry(IndexFILE *indexf, char *metaWord, int metaType, int *applyautomaticmetanames)
{
struct metaEntry* tmpEntry;
struct metaEntry* newEntry;
	
	if(metaWord == NULL || metaWord[0]=='\0') return;
	strtolower(metaWord);

	
	/* 06/00 Jose Ruiz - Check for automatic metanames
	*/
	if(((int)strlen(metaWord)==9) && memcmp(metaWord,"automatic",9)==0) {
		*applyautomaticmetanames =1;
		return;
	}

/* #### Jose Ruiz - New Stuff. Use metaType */
	/* See if there is a previous metaname with the same name */
	tmpEntry = getMetaNameData(indexf, metaWord);

	if(!tmpEntry)      /* metaName not found - Create a new one */
	{
		newEntry=(struct metaEntry*) emalloc(sizeof(struct metaEntry));
		newEntry->metaType = 0;
		newEntry->metaName = (char*)estrdup(metaWord);
		newEntry->metaID = indexf->metaCounter + 2;

			/* Add at the end of the list of metanames */
		if (indexf->metaCounter)
			indexf->metaEntryArray = (struct metaEntry **)erealloc(indexf->metaEntryArray,(indexf->metaCounter+1)*sizeof(struct metaEntry *));
		else
			indexf->metaEntryArray = (struct metaEntry **)emalloc(sizeof(struct metaEntry *));

		indexf->metaEntryArray[indexf->metaCounter++] = newEntry;
		tmpEntry = newEntry;
	}
	/* Add metaType info */
	tmpEntry->metaType |= metaType;

	/* Asign internal metanames if found */
	if(strcmp(metaWord,AUTOPROPERTY_DOCPATH)==0) indexf->filenameProp=tmpEntry;
	else if(strcmp(metaWord,AUTOPROPERTY_TITLE)==0) indexf->titleProp=tmpEntry;
	else if(strcmp(metaWord,AUTOPROPERTY_LASTMODIFIED)==0)indexf->filedateProp=tmpEntry;
	else if(strcmp(metaWord,AUTOPROPERTY_STARTPOS)==0) indexf->startProp=tmpEntry;
	else if(strcmp(metaWord,AUTOPROPERTY_DOCSIZE)==0) indexf->sizeProp=tmpEntry;
	else if(strcmp(metaWord,AUTOPROPERTY_SUMMARY)==0) indexf->summaryProp=tmpEntry;

	/* #### */
}


/* Returns the ID associated with the metaName if it exists
*/

int getMetaNameID(indexf, word)
IndexFILE *indexf;
char * word;
{
int i;

	for (i=0;i<indexf->metaCounter;i++) 
		if (strcmp(indexf->metaEntryArray[i]->metaName, word)==0)
			return (i+2);
	return 1;
}
