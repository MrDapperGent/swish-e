/*
** DocProperties.c, DocProperties.h
**
** Functions to manage the index's Document Properties 
**
** File Created.
** Mark Gaulin 11/24/98
**
** change sprintf to snprintf to avoid corruption, 
** and use MAXSTRLEN from swish.h instead of literal "200"
** SRE 11/17/99
**
** 04/00 Jose ruiz
** storeDocProperties and readNextDocPropEntry modified to store
** the int numbers compressed. This makes integers "portable"
**
** 04/00 Jose Ruiz
** Added sorting results by property
**
** 07/00 and 08/00 - Jose Ruiz
** Many modifications to make all functions thread safe
**
** 08/00 - Added ascending and descending capabilities in results sorting
**
** 2001-01    rasc    getResultPropertyByName rewritten, datatypes for properties.
** 2001-02    rasc    isAutoProperty
**                    printSearchResultProperties changed
** 2001-03-15 rasc    Outputdelimiter var name changed
** 2001-06-08 wsm     Store propValue at end of docPropertyEntry to save memory
** 
*/

#include <limits.h>     // for ULONG_MAX
#include "swish.h"
#include "string.h"
#include "file.h"
#include "hash.h"
#include "mem.h"
#include "merge.h"
#include "error.h"
#include "search.h"
#include "index.h"
#include "docprop.h"
#include "error.h"
#include "compress.h"
#include "metanames.h"
#include "result_output.h"
#include "result_sort.h"
#include "entities.h"



void freeDocProperties(docProperties *docProperties)
{
	/* delete the linked list of doc properties */
	struct propEntry *prop;
	int i;

	for(i=0;i<docProperties->n;i++)
	{
		prop = docProperties->propEntry[i];
		while (prop)
		{
			propEntry *nextOne = prop->next;
			efree(prop);

			prop = nextOne;
		}
	}
	efree(docProperties);
}


/*******************************************************************
*   Converts a property into a string, based on it's type.
*   Numbers are zero filled
*
*   Call with:
*       *metaEntry
*       *propEntry
*
*   Returns:
*       malloc's a new string.  Caller must call free().
*
*
********************************************************************/

static char *DecodeDocProperty( struct metaEntry *meta_entry, propEntry *prop )
{
    char *s;
    unsigned long i;
    
    if( is_meta_string(meta_entry) )      /* check for ascii/string data */
        return bin2string(prop->propValue,prop->propLen);


    if( is_meta_date(meta_entry) )
    {
        s=emalloc(20);
        i = *(unsigned long *) prop->propValue;  /* read binary */
        i = UNPACKLONG(i);     /* Convert the portable number */
        strftime(s,20,"%Y-%m-%d %H:%M:%S",(struct tm *)localtime((time_t *)&i));
        return s;
    }


    
    if( is_meta_number(meta_entry) )
    {
        s=emalloc(14);
        i=*(unsigned long *)prop->propValue;  /* read binary */
        i = UNPACKLONG(i);     /* Convert the portable number */
        sprintf(s,"%.013lu",i);
        return s;
    }

    progwarn("Invalid property type for property '%s'\n", meta_entry->metaName );
    return estrdup("");
}




/*******************************************************************
*   Converts a string into a string for saving as a property
*   Which means will either return a duplicated string,
*   or a packed unsigned long.
*
*   Call with:
*       *metaEntry
*       **encodedStr (destination)
*       *string
*
*   Returns:
*       malloc's a new string, stored in **encodedStr.  Caller must call free().
*       length of encoded string, or zero if an error
*       (zero length strings are not for encoding anyway, I guess)
*
*   QUESTION: ???
*       should this return a *docproperty instead?
*       numbers are unsigned longs.  What if someone
*       wanted to store signed numbers?
*
*   ToDO:
*       What about convert entities here?
*
********************************************************************/
int EncodeProperty( struct metaEntry *meta_entry, char **encodedStr, char *propstring )
{
    unsigned long int num;
    char     *newstr;
    char     *badchar;
    char     *tmpnum;
    char     *string;
    
    string = propstring;

    /* skip leading white space */
    while ( isspace( (int)*string) )
        string++;

    if ( !string || !*string )
    {
        progwarn("Null string passed to EncodeProperty");
        return 0;
    }


    /* make a working copy */
    string = estrdup( string );

    /* remove trailing white space  */
    {
        int i = strlen( string );

        while ( i  && isspace( (int)string[i-1]) )
            string[--i] = '\0';
    }


    if (is_meta_number( meta_entry ) || is_meta_date( meta_entry ))
    {
        int j;

        newstr = emalloc( sizeof( num ) + 1 );
        num = strtoul( string, &badchar, 10 ); // would base zero be more flexible?
        
        if ( num == ULONG_MAX )
        {
            progwarnno("Attempted to convert '%s' to a number", string );
            efree(string);
            return 0;
        }

        if ( *badchar ) // I think this is how it works...
        {
            progwarn("Invalid char '%c' found in string '%s'", badchar[0], string);
            efree(string);
            return 0;
        }
        /* I'll bet there's an easier way */
        num = PACKLONG(num);
        tmpnum = (unsigned char *)&num;

        for ( j=0; j <= sizeof(num)-1; j++ )
            newstr[j] = (unsigned char)tmpnum[j];
        
        newstr[ sizeof(num) ] = '\0';

        *encodedStr = newstr;

        efree(string);

        return (int)sizeof(num);
    }
        

    if ( is_meta_string(meta_entry) )
    {
        *encodedStr = string;
        return (int)strlen( string );
    }


    progwarn("EncodeProperty called but doesn't know the property type :(");
    return 0;
}


/* Add the given file/metaName/propValue data to the File object */
int addDocProperty( docProperties **docProperties, struct metaEntry *meta_entry, unsigned char *propValue, int propLen, int preEncoded )
{
	struct docProperties *dp = *docProperties; 
    propEntry *docProp;
	int i;


    /* convert string to a document property, if not already encoded */
    if ( !preEncoded )
    {
        char *tmp;

        /* $$$ Just ignore null values -- would probably be better to check before calling */
        if ( !propValue || !*propValue )
            return 1;
        
        propLen = EncodeProperty( meta_entry, &tmp, propValue );
        if ( !propLen )
            return 0;
        propValue = tmp;
    }


    /* Store property if it's not zero length */

	if(propLen)
	{

	    /* First property added, allocate space for the property array */
	    
		if( !dp )
		{
			dp = (struct docProperties *) emalloc(sizeof(struct docProperties) + (meta_entry->metaID + 1) * sizeof(propEntry *));
			*docProperties = dp;

			dp->n = meta_entry->metaID + 1;

			for( i = 0; i < dp->n; i++ )
				dp->propEntry[i] = NULL;
		}

		else /* reallocate if needed */
		{
			if( dp->n <= meta_entry->metaID )
			{
				dp = (struct docProperties *) erealloc(dp,sizeof(struct docProperties) + (meta_entry->metaID + 1) * sizeof(propEntry *));

				*docProperties = dp;
				for( i = dp->n ; i <= meta_entry->metaID; i++ )
					dp->propEntry[i] = NULL;
					
				dp->n = meta_entry->metaID + 1;
			}
		}

		docProp=(propEntry *) emalloc(sizeof(propEntry) + propLen);

	    memcpy(docProp->propValue, propValue, propLen);
		docProp->propLen=propLen;
		
		docProp->next = dp->propEntry[meta_entry->metaID];	
		dp->propEntry[meta_entry->metaID] = docProp;	/* update head-of-list ptr */
	}

	if ( !preEncoded )
	    efree( propValue );

    return 1;	    
}

/*#### */

/*
* Dump the document properties into the index file 
* The format is:
*	<PropID:int><PropValueLen:int><PropValue:not-null-terminated>
*	  ...
*	<PropID:int><PropValueLen:int><PropValue:not-null-terminated>
*	<EndofList:null char>
* All ints are compressed to save space. Jose Ruiz 04/00
*
* The list is terminated with a PropID with a value of zero
*/
/* #### Modified to use propLen */
unsigned char *storeDocProperties(docProperties *docProperties, int *datalen)
{
int propID;
int len;
char *buffer,*p,*q;
int lenbuffer;
propEntry *prop;
	buffer=emalloc((lenbuffer=MAXSTRLEN));
	*datalen=0;
	for(propID=0;propID<docProperties->n;propID++)
	{
		prop = docProperties->propEntry[propID];
		while (prop)
		{
			/* the length of the property value */
			len = prop->propLen;
			/* Realloc buffer size if needed */
			if(lenbuffer<(*datalen+len+8*2))
			{
				lenbuffer +=(*datalen)+len+8*2+500;
				buffer=erealloc(buffer,lenbuffer);
			}
			p= q = buffer + *datalen;
			/* Do not store 0!! - compress does not like it */
			propID++;
			p = compress3(propID,p);
			/* including the length will make retrieval faster */
			p = compress3(len,p);
			memcpy(p,prop->propValue, len);
			*datalen += (p-q)+len;
			prop = prop->next;
		}
	}
	return buffer;
}
/* #### */

/*
 * Read the docProperties section that the buffer pointer is
 * currently pointing to.
 */
/* #### Added support for propLen */
docProperties *fetchDocProperties( char *buf)
{
docProperties *docProperties=NULL;
char* tempPropValue=NULL;
int tempPropLen=0;
int tempPropID;
struct metaEntry meta_entry;

    meta_entry.metaName = "(default)";

	/* read all of the properties */
    tempPropID = uncompress2((unsigned char **)&buf);
	while(tempPropID > 0)
	{
		/* Decrease 1 (it was stored as ID+1 to avoid 0 value ) */
		tempPropID--;

		/* Get the data length */
		tempPropLen = uncompress2((unsigned char **)&buf);

		/* BTW, len must no be 0 */
	    tempPropValue=buf;
		buf+=tempPropLen;

		/* add the entry to the list of properties */
		/* Flag as encoded, so won't encode again */
		meta_entry.metaID = tempPropID;
		addDocProperty(&docProperties, &meta_entry, tempPropValue, tempPropLen, 1 );
       	tempPropID = uncompress2((unsigned char **)&buf);
	}
	return docProperties;
}
/* #### */


/* #### Added propLen support */
/* removed lookupDocPropertyValue. Not used */
/* #### */

int getnumPropertiesToDisplay(SWISH *sw)
{
	if(sw)
		return sw->Search->numPropertiesToDisplay;
	return 0;
}

void addSearchResultDisplayProperty(SWISH *sw, char *propName)
{
struct MOD_Search *srch = sw->Search;

	/* add a property to the list of properties that will be displayed */
	if (srch->numPropertiesToDisplay >= srch->currentMaxPropertiesToDisplay)
	{
		if(srch->currentMaxPropertiesToDisplay) {
			srch->currentMaxPropertiesToDisplay+=2;
			srch->propNameToDisplay=(char **)erealloc(srch->propNameToDisplay,srch->currentMaxPropertiesToDisplay*sizeof(char *));
		} else {
			srch->currentMaxPropertiesToDisplay=5;
			srch->propNameToDisplay=(char **)emalloc(srch->currentMaxPropertiesToDisplay*sizeof(char *));
		}
	}
	srch->propNameToDisplay[srch->numPropertiesToDisplay++] = estrdup(propName);
}



/*
  -- print properties specified with "-p" on cmd line
  -- $$$$ routine obsolete: new method "-x fmt"   $$$$
  -- 2001-02-09 rasc    output on file descriptor
*/

void printSearchResultProperties(SWISH *sw, FILE *f, char **prop)
{
int i;
struct MOD_Search *srch = sw->Search;
	if (srch->numPropertiesToDisplay == 0)
		return;

	for (i = 0; i<srch->numPropertiesToDisplay; i++)
	{
		char* propValue;
		propValue = prop[i];
		
		if (sw->ResultOutput->stdResultFieldDelimiter)
			fprintf(f, "%s", sw->ResultOutput->stdResultFieldDelimiter);
		else
			fprintf(f, " \"");	/* default is to quote the string, with leading space */

		/* print value, handling newlines and quotes */
		while (*propValue)

		{
			if (*propValue == '\n')
				fprintf(f, " ");
			else if (*propValue == '\"')	/* should not happen */
				fprintf(f,"&quot;");
			else
				fprintf(f,"%c", *propValue);
			propValue++;
		}
		fprintf(f,"%s", propValue);

		if (!sw->ResultOutput->stdResultFieldDelimiter)
			fprintf(f,"\"");	/* default is to quote the string */
	}
}



char **getResultProperties(RESULT *r)
{
int i;
char **props;      /* Array to Store properties */
IndexFILE *indexf=r->indexf;
SWISH *sw=(SWISH *)r->sw;
struct MOD_Search *srch = sw->Search;
    if (srch->numPropertiesToDisplay == 0) return NULL;
	
	props=(char **) emalloc(srch->numPropertiesToDisplay*sizeof(char *));
	for (i = 0; i<srch->numPropertiesToDisplay; i++)
	{
		props[i] = getResultPropAsString(r, indexf->propIDToDisplay[i]);
	}
	return props;
}




docProperties *swapDocPropertyMetaNames(docProperties *docProperties, struct metaMergeEntry *metaFile)
{
int metaID;
propEntry *prop;
struct docProperties *newDocProperties;

	newDocProperties = (struct docProperties *)emalloc(sizeof(struct docProperties) + docProperties->n * sizeof(propEntry *));
	newDocProperties->n = docProperties->n;

	for(metaID=0;metaID<newDocProperties->n;metaID++)
		newDocProperties->propEntry[metaID] = NULL;

	/* swap metaName values for properties */
	for(metaID=0;metaID<docProperties->n;metaID++)
	{
		prop = docProperties->propEntry[metaID];
		while (prop)
		{
			struct metaMergeEntry* metaFileTemp;
			propEntry *nextOne = prop->next;
			/* scan the metaFile list to get the new metaName value */
			metaFileTemp = metaFile;
			while (metaFileTemp)
			{
				if (metaID == metaFileTemp->oldMetaID)
				{
					prop->next = newDocProperties->propEntry[metaFileTemp->newMetaID];
					newDocProperties->propEntry[metaFileTemp->newMetaID] = prop;
					break;
				}

				metaFileTemp = metaFileTemp->next;
			}
			prop = nextOne;
		}
	}
	efree(docProperties);
	return newDocProperties;
}



/* Duplicates properties (used by merge) */
docProperties *DupProps(docProperties *docProperties)
{
struct docProperties *newDocProperties=NULL;
int metaID;
propEntry *prop = NULL, *tmp = NULL, *newProp = NULL;

	if(!docProperties) return NULL;

	newDocProperties = (struct docProperties *)emalloc(sizeof(struct docProperties) + docProperties->n * sizeof(propEntry *));
	newDocProperties->n = docProperties->n;

	for(metaID=0;metaID<newDocProperties->n;metaID++)
	{
		newDocProperties->propEntry[metaID] = NULL;
		prop = docProperties->propEntry[metaID];
		newProp = NULL;
		while(prop)
		{
			tmp = (propEntry *) emalloc(sizeof(propEntry) + prop->propLen);
			if(!newProp)
				newDocProperties->propEntry[metaID] = tmp;
			else
				newProp->next = tmp;
	
			memcpy(tmp->propValue, prop->propValue, prop->propLen);
			tmp->propLen = prop->propLen;
			tmp->next = NULL;
			newProp = tmp;
			prop = prop->next;
		}
	}
	return newDocProperties;
}


/* For faster proccess, get de ID of the properties to sort */
int initSearchResultProperties(SWISH *sw)
{
IndexFILE *indexf;
int i;
struct MOD_Search *srch = sw->Search;
	/* lookup selected property names */

	if (srch->numPropertiesToDisplay == 0)
		return RC_OK;
	for(indexf=sw->indexlist;indexf;indexf=indexf->next)
		indexf->propIDToDisplay=(int *)emalloc(srch->numPropertiesToDisplay*sizeof(int));

	for (i = 0; i<srch->numPropertiesToDisplay; i++)
	{
		makeItLow(srch->propNameToDisplay[i]);
		/* Get ID for each index file */
		for(indexf=sw->indexlist;indexf;indexf=indexf->next)
		{
			indexf->propIDToDisplay[i] = getMetaNameID(indexf, srch->propNameToDisplay[i]);
			if (indexf->propIDToDisplay[i] == 1)
			{
				progerr ("Unknown Display property name \"%s\"", srch->propNameToDisplay[i]);
				return (sw->lasterror=UNKNOWN_PROPERTY_NAME_IN_SEARCH_DISPLAY);
			}
		}
	}
	return RC_OK;
}

/* #### Function to format the property of a doc as a string. Th property must be in the index file */
char *getDocPropAsString(IndexFILE *indexf, struct file *p, int ID)
{
char *s=NULL;
unsigned long i;
struct metaEntry *q;
propEntry *prop;
	if(!p) return estrdup("");
	q=getMetaIDData(&indexf->header,ID); 
	if(!q) return estrdup("");

		/* Get the first property for the ID*/
	prop = p->docProperties->propEntry[ID];

		/* If not found return */
	if(!prop) return estrdup("");

	if(is_meta_string(q))      /* check for ascii/string data */
	{
		s=bin2string(prop->propValue,prop->propLen);
	} 
	else if(is_meta_date(q))  /* check for a date */
	{
		s=emalloc(20);
		i=*(unsigned long *)prop->propValue;  /* read binary */
									  /* as unsigned long */
		i = UNPACKLONG(i);     /* Convert the portable number */
			/* Convert to ISO datetime */
		strftime(s,20,"%Y-%m-%d %H:%M:%S",(struct tm *)localtime((time_t *)&i));
	}
	else if(is_meta_number(q))  /* check for a number */
	{
		s=emalloc(14);
		i=*(unsigned long *)prop->propValue;  /* read binary */
						  /* as unsigned long */
		i = UNPACKLONG(i);     /* Convert the portable number */
				/* Convert to string */
		sprintf(s,"%.013lu",i);
	} else s=estrdup("");
	return s;
}



/* #### Function to format the property of a result as a string */
char *getResultPropAsString(RESULT *p, int ID)
{
char *s=NULL;
unsigned long i;
IndexFILE *indexf=p->indexf;
SWISH *sw=(SWISH *)p->sw;
struct metaEntry *q;
propEntry *prop;
	if(!p) return estrdup("");
	switch(ID)
	{
		case AUTOPROP_ID__REC_COUNT:   /* BTW this is nonsense - Added just to avoid memory problems */
			i=0;
			s=emalloc(14);
  			sprintf(s,"%.013lu",i);
			break;
		case AUTOPROP_ID__RESULT_RANK:
			i=p->rank;
			s=emalloc(14);
  			sprintf(s,"%.013lu",i);
			break;
		case AUTOPROP_ID__DOCPATH:
			if(!p->filename)
			{
				if(indexf->filearray[p->filenum-1] == NULL)
					indexf->filearray[p->filenum-1] = readFileEntry(sw, indexf, p->filenum);
				s=estrdup(indexf->filearray[p->filenum-1]->fi.filename);
			} 
			else
				s=estrdup(p->filename);
			break;
		case AUTOPROP_ID__STARTPOS:
			i=p->start;
			s=emalloc(14);
  			sprintf(s,"%.013lu",i);
			break;
		case AUTOPROP_ID__INDEXFILE:
			s=estrdup(p->indexf->line);
			break; 
		case AUTOPROP_ID__DOCSIZE:
			i=p->size;
			s=emalloc(14);
  			sprintf(s,"%.013lu",i);
			break;
		case AUTOPROP_ID__LASTMODIFIED:
			s=emalloc(20);
				/* Convert to ISO datetime */
			strftime(s,20,"%Y-%m-%d %H:%M:%S",(struct tm *)localtime((time_t *)&p->last_modified));
			break;
		case AUTOPROP_ID__TITLE:
			if(!p->title)
			{
				if(indexf->filearray[p->filenum-1] == NULL)
					indexf->filearray[p->filenum-1] = readFileEntry(sw, indexf, p->filenum);
				s=estrdup(indexf->filearray[p->filenum-1]->fi.title);
			} 
			else
				s=estrdup(p->title);
			break;
		case AUTOPROP_ID__SUMMARY:
			if(!p->summary)
			{
				if(indexf->filearray[p->filenum-1] == NULL)
					indexf->filearray[p->filenum-1] = readFileEntry(sw, indexf, p->filenum);
				s=estrdup(indexf->filearray[p->filenum-1]->fi.summary);
			} 
			else
				s=estrdup(p->summary);
			break;
		default:   /* User properties */
			q=getMetaIDData(&indexf->header,ID); 
			if(!q) return estrdup("");
				/* Search the property */
			if(indexf->filearray[p->filenum-1] == NULL)
				indexf->filearray[p->filenum-1] = readFileEntry(sw, indexf, p->filenum);
				/* Get the first property for this ID*/
			prop = indexf->filearray[p->filenum-1]->docProperties->propEntry[ID];
				/* If not found return */
			if(!prop) return estrdup("");

			if(is_meta_string(q))      /* check for ascii/string data */
			{
				s=bin2string(prop->propValue,prop->propLen);
			} 
			else if(is_meta_date(q))  /* check for a date */
			{
				s=emalloc(20);
				i=*(unsigned long *)prop->propValue;  /* read binary */
											  /* as unsigned long */
				i = UNPACKLONG(i);     /* Convert the portable number */
					/* Convert to ISO datetime */
				strftime(s,20,"%Y-%m-%d %H:%M:%S",(struct tm *)localtime((time_t *)&i));
			}
			else if(is_meta_number(q))  /* check for a number */
			{
				s=emalloc(14);
				i=*(unsigned long *)prop->propValue;  /* read binary */
								  /* as unsigned long */
				i = UNPACKLONG(i);     /* Convert the portable number */
						/* Convert to string */
				sprintf(s,"%.013lu",i);
			} else s=estrdup("");
	}
	return s;
}


void getSwishInternalProperties(struct file *fi, IndexFILE *indexf)
{
propEntry *p;

	if((indexf->header.titleProp->metaID < fi->docProperties->n) 
		&& (p = fi->docProperties->propEntry[indexf->header.titleProp->metaID]))
	{
		fi->fi.title=bin2string(p->propValue,p->propLen);
	}
	if((indexf->header.filedateProp->metaID < fi->docProperties->n) 
		&& (p = fi->docProperties->propEntry[indexf->header.filedateProp->metaID]))
	{
		fi->fi.mtime = *(unsigned long *)p->propValue;
		fi->fi.mtime = UNPACKLONG(fi->fi.mtime);
	}
	if((indexf->header.startProp->metaID < fi->docProperties->n) 
		&& (p = fi->docProperties->propEntry[indexf->header.startProp->metaID]))
	{
		fi->fi.start = *(unsigned long *)p->propValue;
		fi->fi.start = UNPACKLONG(fi->fi.start);
	}
	if((indexf->header.sizeProp->metaID < fi->docProperties->n) 
		&& (p = fi->docProperties->propEntry[indexf->header.sizeProp->metaID]))
	{
		fi->fi.size = *(unsigned long *)p->propValue;
		fi->fi.size = UNPACKLONG(fi->fi.size);
	}
	if((indexf->header.summaryProp->metaID < fi->docProperties->n) 
		&& (p = fi->docProperties->propEntry[indexf->header.summaryProp->metaID]))
	{
		fi->fi.summary=bin2string(p->propValue,p->propLen);
	}
}




/* ------ new property handling with data types --------- */


/* 
  -- Returns the contents of the property "propertyname" of result r
  -- This routines returns properties in their origin. representation.
  -- This routines knows all internal (auto) and user properties!

  -- Return: NULL if not found, or ptr to alloced structure
             property value and type (structure has to be freed()!)..

  -- 2000-12  jruiz -- inital coding
  -- 2001-01  rasc  -- value and type structures, AutoProperties, rewritten
 */




 /*
    -- case insensitive  check...
    -- (XML props should be case sensitive, but does a user understand this?
    --  ==> user properties are not case sensitive)
    -- 2001-02-11  rasc  (result of develop discussion)
 */

PropValue * getResultPropertyByName (SWISH *sw, char *pname, RESULT *r)
{
  int       i;
  int       prop_id;
  PropValue *pv;




		/* Search for the property name */

	pv = (PropValue *) emalloc (sizeof (PropValue));
	pv->datatype = UNDEFINED;

	/*
	  -- check for auto properties ( == internal metanames)
	  -- for speed: use internal PropID, instead name
	 */

	prop_id = isAutoProperty (pname);

	switch (prop_id) {
		case  AUTOPROP_ID__TITLE:
 			pv->datatype = STRING;
			pv->value.v_str = r->title;
			break;

		case  AUTOPROP_ID__DOCPATH:
			pv->datatype = STRING;
			pv->value.v_str = r->filename;
			break;

		case  AUTOPROP_ID__SUMMARY:
			pv->datatype = STRING;
			pv->value.v_str = r->summary;
			break;

		case  AUTOPROP_ID__DOCSIZE:
			pv->datatype = INTEGER;
			pv->value.v_int = r->size;
			break;

		case  AUTOPROP_ID__LASTMODIFIED:
			pv->datatype = DATE;
			pv->value.v_date = r->last_modified;
			break;

		case  AUTOPROP_ID__STARTPOS:
			pv->datatype = INTEGER;
			pv->value.v_int = r->start;
			break;

		case  AUTOPROP_ID__INDEXFILE:
			pv->datatype = STRING;
			pv->value.v_str = r->indexf->line;
			break;

		case  AUTOPROP_ID__RESULT_RANK:
			pv->datatype = INTEGER;
			pv->value.v_int = r->rank;
			break;

		case  AUTOPROP_ID__REC_COUNT:
			pv->datatype = INTEGER;
			pv->value.v_int = r->count;
			break;

		default:

		    /* User defined Properties
		       -- so far we only know STRING types as user properties
		       -- $$$ ToDO: other types...
		    */

		   for(i=0;i<sw->Search->numPropertiesToDisplay;i++) {
			if(!strcasecmp(pname,sw->Search->propNameToDisplay[i])) {
				pv->datatype = STRING;
				pv->value.v_str = r->Prop[i];
				break;
			}
		   }
		   break;
	}
 
	if (pv->datatype == UNDEFINED) {	/* nothing found */
	  efree (pv);
	  pv = NULL;
	}

	return pv;
}


/*
  -- check if a propertyname is an internal property
  -- (AutoProperty). Check is case sensitive!
  -- Return: 0 (no AutoProp)  or >0 = ID of AutoProp..
  -- 2001-02-07 rasc
*/


int isAutoProperty (char *propname)

{
  static struct {
	char *name;
	int  id;
  }  *ap, auto_props[] = {
	{ AUTOPROPERTY_REC_COUNT,	AUTOPROP_ID__REC_COUNT },
	{ AUTOPROPERTY_RESULT_RANK,	AUTOPROP_ID__RESULT_RANK },
	{ AUTOPROPERTY_TITLE,		AUTOPROP_ID__TITLE },
	{ AUTOPROPERTY_DOCPATH,		AUTOPROP_ID__DOCPATH },
	{ AUTOPROPERTY_DOCSIZE,		AUTOPROP_ID__DOCSIZE },
	{ AUTOPROPERTY_SUMMARY,		AUTOPROP_ID__SUMMARY },
	{ AUTOPROPERTY_LASTMODIFIED,	AUTOPROP_ID__LASTMODIFIED },
	{ AUTOPROPERTY_INDEXFILE,	AUTOPROP_ID__INDEXFILE },
	{ AUTOPROPERTY_STARTPOS,	AUTOPROP_ID__STARTPOS },

        { NULL,				0 }
  };


  /* -- Precheck... fits start of propname? */

  if (strncasecmp (propname,AUTOPROPERTY__ID_START__,
		sizeof(AUTOPROPERTY__ID_START__)-1)) {
	return 0;
  }

  ap = auto_props;
  while (ap->name) {
	if (! strcasecmp(propname,ap->name)) {
	   return ap->id;
	}
	ap++;
  }
  return 0;
}




void dump_file_properties(IndexFILE * indexf, struct  file *fi )
{
    int j;
	propEntry *prop;
    struct metaEntry *meta_entry;
    char *propstr;

	if ( !fi->docProperties )  /* may not be any properties */
	    return;

    for (j = 0; j < fi->docProperties->n; j++)
    {
        if ( !fi->docProperties->propEntry[j] )
            continue;

        meta_entry = getMetaIDData( &indexf->header, j );

        printf("  %20s (%2d):", meta_entry->metaName, meta_entry->metaID );

        for ( prop = fi->docProperties->propEntry[j]; prop; prop = prop->next )
        {
            propstr = DecodeDocProperty( meta_entry, prop );
            
            printf(" \"%s\"", propstr );

            efree( propstr );
        }

        printf( "\n" );;
    }
}

