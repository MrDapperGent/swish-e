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
**
** 2001-02-12 rasc   errormsg "print" changed...
**
*/

#include "swish.h"
#include "string.h"
#include "compress.h"
#include "mem.h"
#include "error.h"
#include "merge.h"
#include "docprop.h"
#include "index.h"
#include "search.h"
#include "hash.h"
#include "ramdisk.h"


/* 2001-05 jmruiz */
/* Routines for compressing numbers - Macros converted to routines */

/* Compress a number and writes it to a file */
void compress1(int num,FILE *fp,int (*f_putc)(int , FILE *))
{
   int _i=0,_r=num;
   unsigned char _s[5];
   while(_r)
   {
      _s[_i++] = _r & 127;
      _r >>= 7;
   }
   while(--_i >=0)
     f_putc(_s[_i] | (_i ?  128 : 0), fp);
}

/* Compress a number and writes it to a buffer */
/* buffer must be previously allocated */
/* returns the decreased buffer pointer after storing the compressed number in it */
unsigned char *compress2(int num,unsigned char *buffer)
{
   int _i = num;

   while(_i) {
      *buffer = _i & 127;
      if(_i != num)
         *buffer|=128;
      _i >>= 7;
      buffer--;
  }

  return buffer;
}

/* Compress a number and writes it to a buffer */
/* buffer must be previously allocated */
/* returns the incrmented buffer pointer after storing the compressed number in it */
unsigned char *compress3(int num,unsigned char *buffer)
{
   int _i=0,_r=num;
   unsigned char _s[5];

   while(_r)
   {
      _s[_i++] = _r & 127;
      _r >>= 7;
   } while(--_i >=0)
   *buffer++=(_s[_i] | (_i ? 128 : 0));

   return buffer;
}

/* Uncompress a number from a file */
int uncompress1(FILE *fp,int (*f_getc)(FILE *))
{
    int _c;
    int num = 0; 
    do
    {
        _c=(int)f_getc(fp); 
        num <<= 7; 
        num |= _c & 127; 
        if(!num) 
            break; 
    } while (_c & 128);

    return num;
}

/* same routine but this works with a memory forward buffer instead of file */
/* it also increases the buffer pointer */
int uncompress2(unsigned char **buffer) 
{
    int _c;
    int num = 0; 
    unsigned char *p = *buffer;

    do
    { 
        _c=(int)((unsigned char)*p++); 
        num <<= 7; 
        num |= _c & 127; 
        if(!num) 
           break; 
    } while (_c & 128);

    *buffer = p;
    return num;
}

/* Rourtines to make long integers portable */
unsigned long PACKLONG(unsigned long num)
{ 
    unsigned long _i=0L; 
    unsigned char *_s; 

    if(num) 
    { 
        _s=(unsigned char *) &_i; 
        _s[0]=(unsigned char)((num >> 24) & 0xFF); 
        _s[1]=(unsigned char)((num >> 16) & 0xFF); 
        _s[2]=(unsigned char)((num >> 8) & 0xFF); 
        _s[3]=(unsigned char)(num & 0xFF); 
        return _i;
    } 
    return num;
}

/* Same routine - Packs long in buffer */
void PACKLONG2(unsigned long num, unsigned char *buffer) 
{
   buffer[0]=(unsigned char)((num >> 24) & 0xFF); 
   buffer[1]=(unsigned char)((num >> 16) & 0xFF); 
   buffer[2]=(unsigned char)((num >> 8) & 0xFF); 
   buffer[3]=(unsigned char)(num & 0xFF);
}


unsigned long UNPACKLONG(unsigned long num)
{ 
   unsigned char *_s = (unsigned char *) &num; 

   return (_s[0]<<24)+(_s[1]<<16)+(_s[2]<<8)+_s[3]; 
}

/* Same macro - UnPacks long from buffer */
unsigned long UNPACKLONG2(unsigned char *buffer) 
{
   return (buffer[0]<<24)+(buffer[1]<<16)+(buffer[2]<<8)+buffer[3];
}


/* 09/00 Jose Ruiz 
** Function to compress location data in memory 
** and use less memory while indexing */
unsigned char *compress_location(SWISH *sw,IndexFILE *indexf,LOCATION *l)
{
unsigned char *p,*q;
int i,index,index_structure,index_structfreq,max_size,vars[2];
struct MOD_Index *idx = sw->Index;

    /* check if the buffer is long enough */
    /* just to avoid bufferoverruns */
    /* In the worst case and integer will need 5 bytes */
    /* but fortunatelly this is very uncommon */
    max_size=((sizeof(LOCATION)/sizeof(int) + 1)+(l->frequency-1))*5;
    if(max_size>idx->len_compression_buffer)
    {
        idx->len_compression_buffer=max_size+200;
        idx->compression_buffer=erealloc(idx->compression_buffer,idx->len_compression_buffer);
    }
    
    p=idx->compression_buffer + idx->len_compression_buffer-1;
    for(i=l->frequency-1;i>=0;i--) {
        p = compress2(l->position[i],p);
    }
    p = compress2(l->filenum,p);
    /* Pack metaID frequency and structure in just one integer */
    /* Pack structure in just one smaller integer */
    vars[0]=l->structure;
    index_structure=get_lookup_index(&indexf->header.structurelookup,1,vars)+1;
    vars[0]=l->frequency;
    vars[1]=index_structure;
    index_structfreq=get_lookup_index(&indexf->header.structfreqlookup,2,vars)+1;

    /* Pack and add 1 to index to avoid problems with compress 0 */
    vars[0]=l->metaID;
    vars[1]=index_structfreq;
    index=get_lookup_index(&indexf->header.locationlookup,2,vars)+1;

    p = compress2(index,p);
        /* Get the length of all the data */
    i = idx->compression_buffer + idx->len_compression_buffer-p-1;

    if(p < idx->compression_buffer)
        progerr("Internal error in compress_location routine");

        /* Swap location info to file */
	if(idx->swap_locdata) {
		q=(unsigned char *)SwapLocData(sw,++p,i);
	}
	else {
		q=(unsigned char *)Mem_ZoneAlloc(idx->locZone, i);
		memcpy(q,++p,i);
	}
    efree(l);
    return (unsigned char *)q;
}

/* 09/00 Jose Ruiz 
** Extract compressed location
** Just needed by merge and removestops (IgnoreLimit option)
*/
LOCATION *uncompress_location(SWISH *sw,IndexFILE *indexf, unsigned char *p)
{
LOCATION *loc;
int i,tmp,metaID,filenum,structure,frequency,index,index_structfreq,index_structure;
    index = uncompress2(&p);
    filenum = uncompress2(&p);
    metaID=indexf->header.locationlookup->all_entries[index-1]->val[0];
    index_structfreq=indexf->header.locationlookup->all_entries[index-1]->val[1];
    frequency=indexf->header.structfreqlookup->all_entries[index_structfreq-1]->val[0];
    index_structure=indexf->header.structfreqlookup->all_entries[index_structfreq-1]->val[1];
    structure=indexf->header.structurelookup->all_entries[index_structure-1]->val[0];
    loc=(LOCATION *)emalloc(sizeof(LOCATION)+(frequency-1)*sizeof(int));
    loc->metaID=metaID;
    loc->filenum=filenum;
    loc->structure=structure;
    loc->frequency=frequency;
    for(i=0;i<frequency;i++)
    {
        tmp = uncompress2(&p);
        loc->position[i]=tmp;
    }
    return loc;
}


/* 09/00 Jose Ruiz
** Compress all non yet compressed location data of an entry
*/
void CompressCurrentLocEntry(SWISH *sw,IndexFILE *indexf,ENTRY *e)
{
int i;

    for(i = e->currentlocation; i < e->u1.max_locations; i++)
        e->locationarray[i] = (LOCATION *)compress_location(sw, indexf, e->locationarray[i]);

	e->currentlocation = e->u1.max_locations;
}

/* 09/00 Jose Ruiz
** Function to swap location data to a temporal file or ramdisk to save
** memory. Unfortunately we cannot use it with IgnoreLimit option
** enabled.
** The data has been compressed previously in memory.
** Returns the pointer to the file.
*/
long SwapLocData(SWISH *sw,unsigned char *buf,int lenbuf)
{
long pos;
struct MOD_Index *idx = sw->Index;


        /* Check if we have IgnoreLimit active */
        /* If it is active, we can not swap the location info */
        /* to disk because it can be modified later */
    if(idx->plimit!=NO_PLIMIT)
        return (long)buf;  /* do nothing */

    if(!idx->fp_loc_write)
    {
        if(!(idx->fp_loc_write = fopen(idx->swap_location_name,FILEMODE_WRITE)))
            progerr("Could not create temp file %s",idx->swap_location_name);
        idx->swap_tell = ftell;
        idx->swap_write = fwrite;
        idx->swap_close = fclose;
        idx->swap_seek = fseek;
        idx->swap_read = fread;
        idx->swap_getc = fgetc;
        idx->swap_putc = fputc;
    }

    pos=idx->swap_tell(idx->fp_loc_write);
    compress1(lenbuf,idx->fp_loc_write,idx->swap_putc);
    if(idx->swap_write(buf,1,lenbuf,idx->fp_loc_write)!=(unsigned int)lenbuf)
    {
        progerr("Cannot write to swap file");
    }
    return pos;
}

/* 09/00 Jose Ruiz
** Gets the location data from the swap file
** Returns a memory compressed location data
*/
unsigned char *unSwapLocData(SWISH *sw,long pos)
{
unsigned char *buf;
int lenbuf;
struct MOD_Index *idx = sw->Index;

        /* Check if we have IgnoreLimit active */
        /* If it is active, we can not swap the location info */
        /* to disk because it can be modified later */
    if(idx->plimit!=NO_PLIMIT)
        return (unsigned char *)pos;  /* do nothing */

    if(!idx->fp_loc_read)
    {
        idx->swap_close(idx->fp_loc_write);
        idx->fp_loc_write = NULL;
        if(!(idx->fp_loc_read=fopen(idx->swap_location_name,FILEMODE_READ)))
            progerr("Could not open temp file %s",idx->swap_location_name);
    }

    idx->swap_seek(idx->fp_loc_read,pos,SEEK_SET);
    lenbuf = uncompress1(idx->fp_loc_read,idx->swap_getc);
    buf=(unsigned char *)emalloc(lenbuf);
    idx->swap_read(buf,lenbuf,1,idx->fp_loc_read);
    return buf;
}

/* 09/00 Jose Ruiz
** Function to save all file info and properties of a document to disk
** to save memory while indexing
*/
void SwapFileData(SWISH *sw, struct file *filep)
{
unsigned char *buffer;
int sz_buffer;
struct MOD_Index *idx = sw->Index;

    if (!idx->fp_file_write)
    {
        idx->fp_file_write=fopen(idx->swap_file_name,FILEMODE_WRITE);
    }
    if (!idx->fp_file_write)    {
        progerr("Could not create temp file %s",idx->swap_file_name);
    }
        
    buffer=buildFileEntry(filep,&sz_buffer);

    compress1(sz_buffer,idx->fp_file_write,fputc);   /* Write len */
    fwrite(buffer,sz_buffer,1,idx->fp_file_write);


    efree(buffer);
    freefileinfo(filep);
}


/* 09/00 Jose Ruiz
** Function to get all the file info and properties of a document
** from the swap file
*/
struct file *unSwapFileData(SWISH *sw)
{
struct file *fi;
int len,len1,lookup_path;
unsigned char *buffer,*p;
char *buf1;
struct MOD_Index *idx = sw->Index;


    /* should use a central location to create this! */
    fi=(struct file *)emalloc(sizeof(struct file));

    fi->docProperties   = NULL;
    fi->currentSortProp = NULL;

    if (!idx->fp_file_read)
    {
        fclose(idx->fp_file_write);
        idx->fp_file_write = NULL;
        idx->fp_file_read=fopen(idx->swap_file_name,FILEMODE_READ);
        if (!idx->fp_file_read)
        {
           progerr("Could not open temp file %s",idx->swap_file_name);
        }
    }
    len = uncompress1(idx->fp_file_read,fgetc);

    p=buffer=emalloc(len);
    fread(buffer,len,1,idx->fp_file_read);   /* Read all data */

   
    lookup_path = uncompress2(&p);
    lookup_path--;
    len1 = uncompress2(&p);   /* Read length of filename */
    buf1 = emalloc(len1);
    memcpy(buf1,p,len1);   /* Read filename */
    p+=len1;

    fi->lookup_path=lookup_path;
    fi->filename = buf1;


#ifdef PROPFILE
    p = UnPackPropLocations( fi, p );
#else    
    /* read the document properties section  */
    p = fetchDocProperties(fi, p);
#endif    

    efree(buffer);
    return fi;
}

int get_lookup_index(struct int_lookup_st **lst,int n,int *values)
{
int i,hash;
struct int_st *is=NULL,*tmp=NULL;
    for(i=0,hash=0;i<n;i++)
        hash=hash*17+(values[i]-1);
    hash=hash%HASHSIZE;
    
    if (!*lst)
    {
        *lst=(struct int_lookup_st *)emalloc(sizeof(struct int_lookup_st));
        (*lst)->n_entries=1;
        for(i=0;i<HASHSIZE;i++) (*lst)->hash_entries[i]=NULL;
        is=(struct int_st *)emalloc(sizeof(struct int_st)+sizeof(int)*(n-1));
        is->index=0;
        is->next=NULL;
        for(i=0;i<n;i++) is->val[i]=values[i];
        (*lst)->all_entries[0]=is;
        (*lst)->hash_entries[hash]=is;
    } else {
        for(tmp=(*lst)->hash_entries[hash];tmp;tmp=tmp->next)
        {
            for(i=0;i<n;i++)
                if(tmp->val[i]!=values[i]) break;
            if(i==n) break;
        }
        if(tmp)
        {
            return tmp->index;
        } else {
            is=(struct int_st *)emalloc(sizeof(struct int_st)+sizeof(int)*(n-1));
            is->index=(*lst)->n_entries;
            for(i=0;i<n;i++) is->val[i]=values[i];
            is->next=(*lst)->hash_entries[hash];
            (*lst)->hash_entries[hash]=is;
            *lst=(struct int_lookup_st *)erealloc(*lst,sizeof(struct int_lookup_st)+(*lst)->n_entries*sizeof(struct int_st *));
            (*lst)->all_entries[(*lst)->n_entries++]=is;
        }
    }
    return ((*lst)->n_entries-1);
}

    
int get_lookup_path(struct char_lookup_st **lst,char *path)
{
unsigned int i,hashval;
struct char_st *is=NULL,*tmp=NULL;
    hashval=hash(path);
    
    if (!*lst)
    {
        *lst=(struct char_lookup_st *)emalloc(sizeof(struct char_lookup_st));
        (*lst)->n_entries=1;
        for(i=0;i<HASHSIZE;i++) (*lst)->hash_entries[i]=NULL;
        is=(struct char_st *)emalloc(sizeof(struct char_st));
        is->index=0;
        is->next=NULL;
        is->val=estrdup(path);
        (*lst)->all_entries[0]=is;
        (*lst)->hash_entries[hashval]=is;
    } else {
        for(tmp=(*lst)->hash_entries[hashval];tmp;tmp=tmp->next)
            if((strlen(tmp->val)==strlen(path)) && (strcmp(tmp->val,path)==0)) break;
        if(tmp)
        {
            return tmp->index;
        } else {
            is=(struct char_st *)emalloc(sizeof(struct char_st));
            is->index=(*lst)->n_entries;
            is->val=estrdup(path);
            is->next=(*lst)->hash_entries[hashval];
            (*lst)->hash_entries[hashval]=is;
            *lst=(struct char_lookup_st *)erealloc(*lst,sizeof(struct char_lookup_st)+(*lst)->n_entries*sizeof(struct char_st *));
            (*lst)->all_entries[(*lst)->n_entries++]=is;
        }
    }
    return ((*lst)->n_entries-1);
}

