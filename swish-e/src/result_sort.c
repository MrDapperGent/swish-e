/*
$Id$
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
**
**
** jmruiz - 02/2001 - Sorting results module
**
** 2001-05-04 jmruiz added new string comparison routines for proper sorting
**                   sw_strcasecmp and sw_strcmp
**                   also added the skeleton to initModule_ResultSort
**                   and freeModule_ResultSort
**
** 2001-05-05 rasc   just rearranged functions, to make modules look similar
**                   (makes code better to read and understand)
**
**
*/

#include "swish.h"
#include "string.h"
#include "mem.h"
#include "merge.h"
#include "list.h"
#include "docprop.h"
#include "metanames.h"
#include "compress.h"
#include "search.h"
#include "error.h"
#include "db.h"
#include "parse_conffile.h"
#include "swish_qsort.h"
#include "result_sort.h"





/*
** ----------------------------------------------
** 
**  Module management code starts here
**
** ----------------------------------------------
*/



/*
  -- init structures for this module
*/

void initModule_ResultSort (SWISH  *sw)

{
   struct MOD_ResultSort *md;

          /* Allocate structure */
    md =(struct MOD_ResultSort *) emalloc(sizeof(struct MOD_ResultSort));
    sw->ResultSort = md;

          /* Init translation sortorder tables */
    initStrCmpTranslationTable(md->iSortTranslationTable);
    initStrCaseCmpTranslationTable(md->iSortCaseTranslationTable);
          /* Init data for -s command option */
    md->numPropertiesToSort=0;
    md->currentMaxPropertiesToSort=0;
    md->propNameToSort=NULL;
    md->propModeToSort=NULL;
	md->isPreSorted=1; /* Use presorted Index by default */
	md->presortedindexlist = NULL;
}


/*
  -- release all wired memory for this module
*/

/* Resets memory of vars used by ResultSortt properties configuration */
void resetModule_ResultSort (SWISH *sw)

{
  struct MOD_ResultSort *md = sw->ResultSort;
  int i;
  IndexFILE *tmpindexlist;


                /* First the common part to all the index files */
        if (md->propNameToSort)
        {
                for(i=0;i<md->numPropertiesToSort;i++)
                        efree(md->propNameToSort[i]);
                efree(md->propNameToSort);
        }
        if (md->propModeToSort)
                efree(md->propModeToSort);
        md->propNameToSort=NULL;
        md->propModeToSort=NULL;
        md->numPropertiesToSort=0;
        md->currentMaxPropertiesToSort=0;
                /* Now free memory for the IDs of each index file */
        for(tmpindexlist=sw->indexlist;tmpindexlist;tmpindexlist=tmpindexlist->next)
        {
                if (tmpindexlist->propIDToSort)
                        efree(tmpindexlist->propIDToSort);
                tmpindexlist->propIDToSort=NULL;
        }


  return;
}

/* Frees memory of vars used by ResultSortt properties configuration */
void freeModule_ResultSort (SWISH *sw)

{
  struct MOD_ResultSort *md = sw->ResultSort;

   resetModule_ResultSort(sw);

   if(md->presortedindexlist) freeswline(md->presortedindexlist);
      /* Free Module Data Structure */
      /* should not be freed here */
   efree (md);
   sw->ResultSort = NULL;
}


/*
** ----------------------------------------------
** 
**  Module config code starts here
**
** ----------------------------------------------
*/


/*
 -- Config Directives
 -- Configuration directives for this Module
 -- return: 0/1 = none/config applied
*/

int configModule_ResultSort  (SWISH *sw, StringList *sl)

{
  struct MOD_ResultSort *md = sw->ResultSort;
  char *w0    = sl->word[0];
  unsigned char *w1,*w2,*w3;
  int  retval = 1;
  int  incr = 0;
  int  i,j;
  struct swline *tmplist = NULL;
  struct metaEntry *m = NULL;


  if (strcasecmp(w0, "PreSortedIndex") == 0)
  {
     md->isPreSorted = sl->n - 1;    /* If n is 1 (No properties specified) - Do not create presorted indexes */
     if (sl->n > 1)
     {
        grabCmdOptions(sl, 1, &md->presortedindexlist);
                /* Go lowercase  and check with properties */
        for (tmplist = md->presortedindexlist; tmplist; tmplist = tmplist->next)
		{
            tmplist->line = strtolower(tmplist->line);
			/* Check if it is in metanames list */
			if(!(m= getMetaNameData(&sw->indexlist->header, tmplist->line)))
				progerr("%s: parameter is not a property", tmplist->line);
			/* Check if it is a property */
			if(!is_meta_property(m))
			   progerr("%s: parameter is not a property", tmplist->line);
		}
     }
  } else if (strcasecmp(w0, "ResultSortOrder")==0) { 
	if(sl->n == 4) 
	{
	   w1 = (unsigned char *)sl->word[1];
	   w2 = (unsigned char *)sl->word[2];
	   w3 = (unsigned char *)sl->word[3];

	   if(strlen(w1)!=1)
	   {
           progerr("%s: parameter 1 must be one char length", w0);
	   }
       if(strlen(w2)!=1)
	   {
           progerr("%s: parameter 2 must be one char length", w0);
	   }
       switch(w1[0])
	   {
          case '=':
			  incr = 0;
			break;
		  case '>':
			  incr = 1;
			break;
		  default:
			progerr("%s: parameter 1 must be = or >", w0);
			break;
		}
	    for(i=0;w3[i];i++)
		{
			j=(int) w2[0];
            md->iSortTranslationTable[(int) w3[i]] = 
				md->iSortTranslationTable[j] + incr * (i+1);

			md->iSortCaseTranslationTable[(int) w3[i]] = 
				md->iSortCaseTranslationTable[j] + incr * (i+1);
		}
	} else progerr("%s: requires 3 parameters (Eg: [=|>] a ���)",w0);
  }
  else {
      retval = 0;	            /* not a module directive */
  }
  return retval;
}




/*
** ----------------------------------------------
** 
**  Module code starts here
**
** ----------------------------------------------
*/


/* Routine to add the properties specified in -s to the internal structure */
void addSearchResultSortProperty(SWISH *sw, char *propName,int mode)
{
IndexFILE *indexf;
struct MOD_ResultSort *rs=sw->ResultSort;

	/* add a property to the list of properties that will be displayed */
	if (rs->numPropertiesToSort >=rs->currentMaxPropertiesToSort)
	{
                      /* Allocate memory */
		if(rs->currentMaxPropertiesToSort) {
                             /* Reallocate memory */
			rs->currentMaxPropertiesToSort+=2;
			rs->propNameToSort=(char **)erealloc(rs->propNameToSort,rs->currentMaxPropertiesToSort*sizeof(char *));
			for(indexf=sw->indexlist;indexf;indexf=indexf->next)
				indexf->propIDToSort=(int *)erealloc(indexf->propIDToSort,rs->currentMaxPropertiesToSort*sizeof(int));
			rs->propModeToSort=(int *)erealloc(rs->propModeToSort,rs->currentMaxPropertiesToSort*sizeof(int));
		} else {
                             /* Allocate memory */
			rs->currentMaxPropertiesToSort=5;
			rs->propNameToSort=(char **)emalloc(rs->currentMaxPropertiesToSort*sizeof(char *));
			rs->propModeToSort=(int *)emalloc(rs->currentMaxPropertiesToSort*sizeof(int));
		}
                    /* End allocation of memory */
	}
	rs->propNameToSort[rs->numPropertiesToSort] = estrdup(propName);
	rs->propModeToSort[rs->numPropertiesToSort++] = mode;
}




/* preprocess Sort Result Properties to get the ID */
/* If there is not a sort properties then use rank */

int     initSortResultProperties(SWISH * sw)
{
    int     i;
    IndexFILE *indexf;

    if (sw->ResultSort->numPropertiesToSort == 0)
    {
        /* hack -> If no sort perperties have been specified then
           use rank in descending mode */
        addSearchResultSortProperty(sw, AUTOPROPERTY_RESULT_RANK, 1);
        for (indexf = sw->indexlist; indexf; indexf = indexf->next)
        {
            indexf->propIDToSort = (int *) emalloc(sizeof(int));

            indexf->propIDToSort[0] = AUTOPROP_ID__RESULT_RANK;
        }

        return RC_OK;
    }
    for (indexf = sw->indexlist; indexf; indexf = indexf->next)
        indexf->propIDToSort = (int *) emalloc(sw->ResultSort->numPropertiesToSort * sizeof(int));

    for (i = 0; i < sw->ResultSort->numPropertiesToSort; i++)
    {
        makeItLow(sw->ResultSort->propNameToSort[i]);
        /* Get ID for each index file */
        for (indexf = sw->indexlist; indexf; indexf = indexf->next)
        {
            indexf->propIDToSort[i] = getMetaNameID(indexf, sw->ResultSort->propNameToSort[i]);
            if (indexf->propIDToSort[i] == 1)
            {
                /* Check fot RANK Autoproperty */
                if (strcasecmp(sw->ResultSort->propNameToSort[i], AUTOPROPERTY_RESULT_RANK) == 0)
                {
                    indexf->propIDToSort[i] = AUTOPROP_ID__RESULT_RANK;
                }
                else
                {
                    progerr("Unknown Sort property name \"%s\" in one of the index files", sw->ResultSort->propNameToSort[i]);
                    return (sw->lasterror = UNKNOWN_PROPERTY_NAME_IN_SEARCH_SORT);
                }
            }
        }
    }
    return RC_OK;
}


/* 02/2001 Jose Ruiz */
/* function for comparing data in order to
get sorted results with qsort (including combinations of asc and descending
fields */
int     compResultsByNonSortedProps(const void *s1, const void *s2)
{
    RESULT *r1 = *(RESULT * const *) s1;
    RESULT *r2 = *(RESULT * const *) s2;
    int     i,
            rc,
            num_fields,
            sortmode;
    SWISH  *sw = (SWISH *) r1->sw;

    num_fields = sw->ResultSort->numPropertiesToSort;
    for (i = 0; i < num_fields; i++)
    {
        sortmode = sw->ResultSort->propModeToSort[i];
        if ((rc = sortmode * sw_strcasecmp(r1->PropSort[i], r2->PropSort[i], sw->ResultSort->iSortCaseTranslationTable)))
            return rc;
    }
    return 0;
}

/* 02/2001 Jose Ruiz */
/* function for comparing data in order to
get sorted results with qsort (including combinations of asc and descending
fields */
/* This routine uses the presorted tables built during the index proccess */
int     compResultsBySortedProps(const void *s1, const void *s2)
{
    RESULT *r1 = *(RESULT * const *) s1;
    RESULT *r2 = *(RESULT * const *) s2;
    register int i,
            num_fields;
    int     rc,
            sortmode;
    SWISH  *sw = (SWISH *) r1->sw;

    num_fields = sw->ResultSort->numPropertiesToSort;
    for (i = 0; i < num_fields; i++)
    {
        sortmode = sw->ResultSort->propModeToSort[i];
        rc = sortmode * (r1->iPropSort[i] - r2->iPropSort[i]);
        if (rc)
            return rc;
    }
    return 0;
}

/* Adds the results of a search, sorts them by rank.
*/

/* Jose Ruiz 04/00
** Complete rewrite
** Sort was made before calling this function !! -> FASTER!!
** This one just reverses order
*/
RESULT *addsortresult(sw, sphead, r)
     SWISH  *sw;
     RESULT *sphead;
     RESULT *r;
{
    struct MOD_Search *srch = sw->Search;

    if (r->rank > srch->bigrank)
        srch->bigrank = r->rank;


    if (sphead == NULL)
    {
        r->nextsort = NULL;
    }
    else
    {
        r->nextsort = sphead;
    }
    return r;
}

/* Routine to get the presorted lookupdata for a result for all the specified properties */
int    *getLookupResultSortedProperties(RESULT * r)
{
    int     i,j,tmp;
    int    *props = NULL;       /* Array to Store properties Lookups */
    struct metaEntry *m = NULL;
    IndexFILE *indexf = r->indexf;
    SWISH  *sw = (SWISH *) r->sw;
    unsigned char *buffer, *s;
    int sz_buffer;

    props = (int *) emalloc(sw->ResultSort->numPropertiesToSort * sizeof(int));

    for (i = 0; i < sw->ResultSort->numPropertiesToSort; i++)
    {
        switch (indexf->propIDToSort[i])
        {
        case AUTOPROP_ID__REC_COUNT:
            props[i] = 0;
            break;
        case AUTOPROP_ID__RESULT_RANK:
            props[i] = r->rank;
            break;
        case AUTOPROP_ID__DOCPATH:
        case AUTOPROP_ID__FILENUM:
            props[i] = r->filenum;
            break;
        case AUTOPROP_ID__INDEXFILE:
            props[i] = 0;       /* Same value for all of them */
            break;
        case AUTOPROP_ID__DOCSIZE:
        case AUTOPROP_ID__LASTMODIFIED:
        case AUTOPROP_ID__STARTPOS:
        case AUTOPROP_ID__TITLE:
        case AUTOPROP_ID__SUMMARY:
        default:               /* User properties */
            m = getMetaIDData(&indexf->header, indexf->propIDToSort[i]);
            if (m)
            {
		        if(!m->sorted_data)
				{
			       DB_InitReadSortedIndex(sw, r->indexf->DB);
				        /* Get the sorted index of the property */
			       DB_ReadSortedIndex(sw, m->metaID, &buffer, &sz_buffer,r->indexf->DB);
				   if(sz_buffer)
				   {                 /* Exists a presorted table - Let's use it */
			           s = buffer;
			           m->sorted_data = (int *)emalloc(r->indexf->header.totalfiles * sizeof(int));
				       /* Unpack / decompress the numbers */
			           for( j = 0; j < r->indexf->header.totalfiles; j++)
					   {
				          tmp = uncompress2(&s);
				          m->sorted_data[j] = tmp;
					   }
    		           efree(buffer);
				   }
				   else
				   {                 /* Does not exits a presorted table */
					   efree(props);
					   DB_EndReadSortedIndex(sw, r->indexf->DB);
					   return NULL;
				   }
	               DB_EndReadSortedIndex(sw, r->indexf->DB);
				}
                props[i] = m->sorted_data[r->filenum - 1];
            }
            else
                props[i] = 0;
            break;
        }
    }
    return props;
}


char  **getResultSortProperties(RESULT * r)
{
    int     i;
    char  **props = NULL;       /* Array to Store properties */
    IndexFILE *indexf = r->indexf;
    SWISH  *sw = (SWISH *) r->sw;

    if (sw->ResultSort->numPropertiesToSort == 0)
        return NULL;

    props = (char **) emalloc(sw->ResultSort->numPropertiesToSort * sizeof(char *));

    /* $$$ -- need to pass in the max string length */
    for (i = 0; i < sw->ResultSort->numPropertiesToSort; i++)
        props[i] = getResultPropAsString(r, indexf->propIDToSort[i]);

    return props;
}


/* Jose Ruiz 04/00
** Sort results by property
*/
int     sortresults(SWISH * sw, int structure)
{
    int     i,
            j,
            TotalResults;
    RESULT **ptmp;
    RESULT *rtmp;
    RESULT *rp,
           *tmp;
    struct DB_RESULTS *db_results;
    int     (*compResults) (const void *, const void *) = NULL;
    struct MOD_ResultSort *rs = sw->ResultSort;
	int    presorted_data_not_available = 0;

    /* Sort each index file's resultlist */
    for (TotalResults = 0, db_results = sw->Search->db_results; db_results; db_results = db_results->next)
    {
        db_results->sortresultlist = NULL;
        db_results->currentresult = NULL;
        rp = db_results->resultlist;

		if(rs->isPreSorted)
		{
             /* Asign comparison routine to be used by qsort */
            compResults = compResultsBySortedProps;

                /* As we are sorting a unique index file, we can use the presorted data in the index file */
            for (tmp = rp; tmp; tmp = tmp->next)
			{
                /* Load the presorted data */
                tmp->iPropSort = getLookupResultSortedProperties(tmp);

				/* If some of the properties is not presorted, use the
				** old method (ignore presorted index)
				*/
				if(!tmp->iPropSort)
				{
					presorted_data_not_available = 1;
					break;
				}
			}
		} 


		if(!rs->isPreSorted || presorted_data_not_available)
		{
  			/* We do not have presorted tables or do not want to use them */
            /* Assign comparison routine to be used by qsort */
            compResults = compResultsByNonSortedProps;


            /* Read the property value string(s) for all the sort properties */
            for (tmp = rp; tmp; tmp = tmp->next)
                tmp->PropSort = getResultSortProperties(tmp);
		}


        /* Compute number of results */
        for (i = 0, rtmp = rp; rtmp; rtmp = rtmp->next)
            if (rtmp->structure & structure)
                i++;


        if (i)                  /* If there is something to sort ... */
        {
            /* Compute array size */
            ptmp = (RESULT **) emalloc(i * sizeof(RESULT *));

            /* Build an array with the elements to compare and pointers to data */
            for (j = 0, rtmp = rp; rtmp; rtmp = rtmp->next)
                if (rtmp->structure & structure)
                    ptmp[j++] = rtmp;

            /* Sort them */
            swish_qsort(ptmp, i, sizeof(RESULT *), compResults);

            /* Build the list */
            for (j = 0; j < i; j++)
                db_results->sortresultlist = (RESULT *) addsortresult(sw, db_results->sortresultlist, ptmp[j]);


            /* Free the memory of the array */
            efree(ptmp);


            if (db_results->sortresultlist)
            {
                db_results->currentresult = db_results->sortresultlist;
                TotalResults += countResults(db_results->sortresultlist);
            }
        }
    }
    return TotalResults;
}

#ifdef RESORTFILES
int     compFilenums(const void *s1, const void *s2)
{
    struct file *r1 = *(struct file * const *) s1;
    struct file *r2 = *(struct file * const *) s2;

    return r1->filenum - r2->filenum;
}

#endif

/* 01/2001 Jose Ruiz */
/* function for comparing data in order to
get sorted results with qsort (including combinations of asc and descending
fields */

int     compFileProps(const void *s1, const void *s2)
{
    struct file *r1 = *(struct file * const *) s1;
    struct file *r2 = *(struct file * const *) s2;
    propEntry *p1,
           *p2;
    int     metaID = r1->currentSortProp->metaID;

#ifdef PROPFILE
    return Compare_Properties( r1->currentSortProp ,r1->SortProp, r2->SortProp );
#endif

    /* Find the current metaID */
	if(metaID < r1->docProperties->n)
		p1 = r1->docProperties->propEntry[metaID];
	else
		p1 = NULL;

	if(metaID < r2->docProperties->n)
		p2 = r2->docProperties->propEntry[metaID];
	else
		p2 = NULL;

    return Compare_Properties( r1->currentSortProp ,p1, p2 );
}

int is_presorted_prop(SWISH *sw, char *name)
{
  struct MOD_ResultSort *md = sw->ResultSort;
  struct swline *tmplist = NULL;

  if(!md->isPreSorted)
	  return 0;    /* Do not sort any property */
  else 
  {
	 if(!md->presortedindexlist)
	    return 1;    /* All properties must be indexed */
     else
	 {
        for(tmplist = md->presortedindexlist;tmplist;tmplist = tmplist->next)
		   if(strcmp(name,tmplist->line) == 0)
			  return 1;
		return 0;
	 }
  }
  return 0;
}


/* 02/2001 jmruiz */
/* Routine to sort properties at index time */
void    sortFileProperties(SWISH *sw, IndexFILE * indexf)
{
    int     i,
            j,
            k;
    int    *sortFilenums;
    struct metaEntry *m;


    /* Execute for each property */
    for (j = 0; j < indexf->header.metaCounter; j++)
    {
        m = getMetaIDData(&indexf->header, indexf->header.metaEntryArray[j]->metaID);
		m->sorted_data = NULL;


		/* Check if this property must be in a presorted index */
		if( !is_presorted_prop(sw,m->metaName) )
		    continue;

       switch (indexf->header.metaEntryArray[j]->metaID)
	   {

	   /* "internal" properties are sorted at runtime */
       case AUTOPROP_ID__REC_COUNT:
       case AUTOPROP_ID__RESULT_RANK:
       case AUTOPROP_ID__DOCPATH:
       case AUTOPROP_ID__FILENUM:
       case AUTOPROP_ID__INDEXFILE:
           break;

       default:

            /* User properties */

            /* only sort properties */
            if ( !(m->metaType & META_PROP) )
                break;

	       /* Array of filenums to store the sorted docs (referenced by its filenum) */
	       sortFilenums = emalloc(indexf->filearray_cursize * sizeof(int));


	       /* Save the metaEntry in ALL the files */
           for (i = 0; i < indexf->filearray_cursize; i++)
            {
               indexf->filearray[i]->currentSortProp = m; /* waste of space */
#ifdef PROPFILE
               indexf->filearray[i]->SortProp = ReadSingleDocPropertiesFromDisk(sw, indexf, i+1, m->metaID, MAX_SORT_STRING_LEN );
#endif;               
            }


           /* Sort them using qsort. The main work is done by compFileProps */

           /* NOTE: This messes up the order of file entries, so you cannot lookup
            * properties by filenum after this point.
            */

           swish_qsort(indexf->filearray, indexf->filearray_cursize, sizeof(struct file *), &compFileProps);


           /* Build the sorted table */

           for (i = 0, k = 1; i < indexf->filearray_cursize; i++)
		   {
               /* 02/2001 We can have duplicated values - So all them may have the same number asigned  - qsort justs sorts */
               if (i)
			   {
                   /* If consecutive elements are different increase the number */
                   if ((compFileProps(&indexf->filearray[i - 1], &indexf->filearray[i])))
                       k++;
			   }
               sortFilenums[indexf->filearray[i]->filenum - 1] = k;
		   }


#ifdef PROPFILE
        /* Now free properties */
        for (i = 0; i < indexf->filearray_cursize; i++)
            if ( indexf->filearray[i]->SortProp )
            {
                freeProperty( indexf->filearray[i]->SortProp );
                indexf->filearray[i]->SortProp = NULL;
            }
#endif            


           /* Store the integer array of presorted data */
           m->sorted_data = sortFilenums;

#ifdef RESORTFILES
            /* put the file array back in order */
            /* This is done so that the property file can be read sequentially */
           swish_qsort(indexf->filearray, indexf->filearray_cursize, sizeof(struct file *), &compFilenums);
#endif           

           
           break;
		}
    }


}


/* Routines to get the proper sortorder of chars to be called when sorting */
/* sw_strcasecmp sw_strcmp */


/* Exceptions to the standard translation table for sorting strings */
/* See initStrCaseCmpTranslationTable to see how it works */
/* The table shows the equivalences in the following way: */
/*     val(from) = val(order) + offset */
/* where val is asciivalue * 256 */

/* Some comments about ��� ...
** In french and spanish this chars are equivalent to
** � -> a   (french)
** � -> o   (french)
** � -> u   (french + spanish)
** In the other hand, in german:
** � -> a + 1  (german)
** � -> o + 1  (german)
** � -> u + 1  (german)
** I have put the german default. I think that in spanish we can live with that
** If you cannot modify them (change 1 by 0)
** Any comments about other languages are always welcome
*/
struct {
  unsigned char from;
  unsigned char order;
  int offset;
} iTranslationTableExceptions[]=  { 
   { '�', 'A' , 1},    /* >>> german sort order of umlauts */
   { '�', 'O' , 1},    /*     2001-05-04 rasc */
   { '�', 'U' , 1},
   { '�', 'a' , 1},
   { '�', 'o' , 1},
   { '�', 'u' , 1},
   { '�', 's' , 1},    /* <<< german */
   { '�', 'a' , 0},    /* >>> spanish sort order exceptions */
   { '�', 'A' , 0},    /*     2001-05-04 jmruiz */
   { '�', 'e' , 0}, 
   { '�', 'E' , 0}, 
   { '�', 'i' , 0}, 
   { '�', 'I' , 0}, 
   { '�', 'o' , 0}, 
   { '�', 'O' , 0}, 
   { '�', 'u' , 0}, 
   { '�', 'U' , 0}, 
   { '�', 'n' , 1},
   { '�', 'N' , 1},   /* <<< spanish */
   { '�', 'a' , 0},   /* >>> french sort order exceptions */
   { '�', 'A' , 0},   /*     2001-05-04 jmruiz */
   { '�', 'a' , 0},   /*     Taken from the list - Please check */
   { '�', 'A' , 0},   /*     ���������� added in the spanish part */
   { '�', 'c' , 0},   /*     ������ added in the german part */
   { '�', 'C' , 0},
   { '�', 'e' , 0},
   { '�', 'E' , 0},
   { '�', 'e' , 0},
   { '�', 'E' , 0},
   { '�', 'i' , 0},
   { '�', 'I' , 0},
   { '�', 'i' , 0},
   { '�', 'I' , 0},
   { '�', 'o' , 0},
   { '�', 'O' , 0},
   { '�', 'u' , 0},
   { '�', 'U' , 0},    /* >>> french */
   { 0, 0, 0}
};

/* Initialization routine for the comparison table (ignoring case )*/
/* This routine should be called once  at the start of the module */
void initStrCaseCmpTranslationTable(int *iCaseTranslationTable)
{
int i;
      /* Build default table using tolower(asciival) * 256 */
      /* The goal of multiply by 256 is having holes to put values inside
         eg: � is between n and o */
   for(i=0;i<256;i++)
      iCaseTranslationTable[i] = tolower(i) * 256;

      /* Exceptions */
   for(i=0; iTranslationTableExceptions[i].from;i++)
      iCaseTranslationTable[iTranslationTableExceptions[i].from] = tolower(iTranslationTableExceptions[i].order) * 256 + iTranslationTableExceptions[i].offset;
}

/* Initialization routine for the comparison table (case sensitive) */
/* This routine should be called once at the start of the module */
void initStrCmpTranslationTable(int *iCaseTranslationTable)
{
int i;
      /* Build default table using asciival * 256 */
      /* The goal of multiply by 10 is having holes to put values inside
         eg: � is between n and o */
   for(i=0;i<256;i++)
      iCaseTranslationTable[i] = i * 256;

      /* Exceptions */
   for(i=0; iTranslationTableExceptions[i].from;i++)
      iCaseTranslationTable[iTranslationTableExceptions[i].from] = iTranslationTableExceptions[i].order * 256 + iTranslationTableExceptions[i].offset;
}

/* Comparison string routine function. 
** Similar to strcasecmp but using our own translation table
*/
int sw_strcasecmp(unsigned char *s1,unsigned char *s2,int *iTranslationTable)
{
   while ( iTranslationTable[*s1] == iTranslationTable[*s2])
      if (! *s1++) return 0;
      else s2++;
   return iTranslationTable[*s1] - iTranslationTable[*s2];
}

/* Comparison string routine function. 
** Similar to strcmp but using our own translation table
*/
int sw_strcmp(unsigned char *s1,unsigned char *s2,int *iTranslationTable)
{
   while ( iTranslationTable[*s1] == iTranslationTable[*s2])
      if (! *s1++) return 0;
      else s2++;
   return iTranslationTable[*s1] - iTranslationTable[*s2];
}
