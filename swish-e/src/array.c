#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
#define STANDALONE
#define DEBUG
*/

#ifdef STANDALONE

/* Rourtines to make long integers portable */
unsigned long PACKLONG(unsigned long num)
{
    unsigned long _i = 0L;
    unsigned char *_s;

    if (num)
    {
        _s = (unsigned char *) &_i;
        _s[0] = (unsigned char) ((num >> 24) & 0xFF);
        _s[1] = (unsigned char) ((num >> 16) & 0xFF);
        _s[2] = (unsigned char) ((num >> 8) & 0xFF);
        _s[3] = (unsigned char) (num & 0xFF);
        return _i;
    }
    return num;
}


unsigned long UNPACKLONG(unsigned long num)
{
    unsigned char *_s = (unsigned char *) &num;

    return (_s[0] << 24) + (_s[1] << 16) + (_s[2] << 8) + _s[3];
}


static int num = 0;
char *emalloc(unsigned int size)
{
num++;
return malloc(size);
}

void efree(void *p)
{
num--;
free(p);
}


#else

#include "mem.h"

unsigned long PACKLONG(unsigned long num);
unsigned long UNPACKLONG(unsigned long num);
unsigned char *compress3(int num, unsigned char *buffer);
int uncompress2(unsigned char **buffer);

#endif  /* STANDALONE */


#include "array.h"


/* A ARRAY page size */
#define ARRAY_PageSize 4096

#define SizeInt32 4

/* Round to ARRAY_PageSize */
#define ARRAY_RoundPageSize(n) (((n) + ARRAY_PageSize - 1) & (~(ARRAY_PageSize - 1)))

#define ARRAY_PageHeaderSize (1 * SizeInt32) 

#define ARRAY_PageData(pg) ((pg)->data + ARRAY_PageHeaderSize)
#define ARRAY_Data(pg,i) (ARRAY_PageData((pg)) + (i) * SizeInt32)

#define ARRAY_SetNextPage(pg,num) ( *(int *)((pg)->data + 0 * SizeInt32) = PACKLONG(num))

#define ARRAY_GetNextPage(pg,num) ( (num) = UNPACKLONG(*(int *)((pg)->data + 0 * SizeInt32)))


int ARRAY_WritePageToDisk(FILE *fp, ARRAY_Page *pg)
{
    ARRAY_SetNextPage(pg,pg->next);
    fseek(fp,pg->page_number * ARRAY_PageSize,SEEK_SET);
    return fwrite(pg->data,ARRAY_PageSize,1,fp);
}

int ARRAY_WritePage(ARRAY *b, ARRAY_Page *pg)
{
int hash = pg->page_number % ARRAY_CACHE_SIZE;
ARRAY_Page *tmp;
    pg->modified =1;
    if((tmp = b->cache[hash]))
    {
        while(tmp)
        {
            if(tmp->page_number == pg->page_number)
            {
                return 0;
            }
            tmp = tmp->next_cache;
        }
    }
    pg->next_cache = b->cache[hash];
    b->cache[hash] = pg;
    return 0;
}

int ARRAY_FlushCache(ARRAY *b)
{
int i;
ARRAY_Page *tmp, *next;
    for(i = 0; i < ARRAY_CACHE_SIZE; i++)
    {
        if((tmp = b->cache[i]))
        {
            while(tmp)
            {
#ifdef DEBUG
                if(tmp->in_use)
                {
                    printf("DEBUG Error in FlushCache: Page in use\n");
                    exit(0);
                }
#endif
                next = tmp->next_cache;
                if(tmp->modified)
                {
                    ARRAY_WritePageToDisk(b->fp, tmp);
                    tmp->modified = 0;
                }
                if(tmp != b->cache[i])
                    efree(tmp);

                tmp = next;
            }
            b->cache[i]->next_cache = NULL;
        }
    }
    return 0;
}

int ARRAY_CleanCache(ARRAY *b)
{
int i;
ARRAY_Page *tmp,*next;
    for(i = 0; i < ARRAY_CACHE_SIZE; i++)
    {
        if((tmp = b->cache[i]))
        {
            while(tmp)
            {
                next = tmp->next_cache;
                efree(tmp);
                tmp = next;
            }
        }
    }
    return 0;
}

ARRAY_Page *ARRAY_ReadPageFromDisk(FILE *fp, unsigned long page_number)
{
ARRAY_Page *pg = (ARRAY_Page *)emalloc(sizeof(ARRAY_Page) + ARRAY_PageSize);

    fseek(fp,page_number * ARRAY_PageSize,SEEK_SET);
    fread(pg->data,ARRAY_PageSize, 1, fp);

    ARRAY_GetNextPage(pg,pg->next);

    pg->page_number = page_number;
    pg->modified = 0;
    return pg;
}

ARRAY_Page *ARRAY_ReadPage(ARRAY *b, unsigned long page_number)
{
int hash = page_number % ARRAY_CACHE_SIZE;
ARRAY_Page *tmp;
    if((tmp = b->cache[hash]))
    {
        while(tmp)
        {
            if(tmp->page_number == page_number)
            {
                return tmp;
            }
            tmp = tmp->next_cache;
        }
    }

    tmp = ARRAY_ReadPageFromDisk(b->fp, page_number);
    tmp->modified = 0;
    tmp->in_use = 1;
    tmp->next_cache = b->cache[hash];
    b->cache[hash] = tmp;
    return tmp;
}

ARRAY_Page *ARRAY_NewPage(ARRAY *b)
{
ARRAY_Page *pg;
long offset;
FILE *fp = b->fp;
int hash;
int size = ARRAY_PageSize;

    /* Get file pointer */
    if(fseek(fp,0,SEEK_END) !=0)
    {
        printf("mal\n");
    }
    offset = ftell(fp);
    /* Round up file pointer */
    offset = ARRAY_RoundPageSize(offset);

    /* Set new file pointer - data will be aligned */
    if(fseek(fp,offset, SEEK_SET)!=0 || offset != ftell(fp))
    {
        printf("mal\n");
    }

    pg = (ARRAY_Page *)emalloc(sizeof(ARRAY_Page) + size);
    memset(pg,0,sizeof(ARRAY_Page) + size);
    /* Reserve space in file */
    if(fwrite(pg->data,1,size,fp)!=size || ((long)size + offset) != ftell(fp))
    {
        printf("mal\n");
    }

    pg->next = 0;

    pg->page_number = offset/ARRAY_PageSize;

    /* add to cache */
    pg->modified = 1;
    pg->in_use = 1;
    hash = pg->page_number % ARRAY_CACHE_SIZE;
    pg->next_cache = b->cache[hash];
    b->cache[hash] = pg;
    return pg;
}

void ARRAY_FreePage(ARRAY *b, ARRAY_Page *pg)
{
int hash = pg->page_number % ARRAY_CACHE_SIZE;
ARRAY_Page *tmp;

    tmp = b->cache[hash];

#ifdef DEBUG
    if(!(tmp = b->cache[hash]))
    {
        /* This should never happen!!!! */
        printf("Error in FreePage\n");
        exit(0);
    }
#endif

    while(tmp)
    {
        if (tmp->page_number != pg->page_number)
            tmp = tmp->next_cache;
        else
        {
            tmp->in_use = 0;
            break;
        }
    }
}

ARRAY *ARRAY_New(FILE *fp, unsigned int size)
{
ARRAY *b;
int i;
    b = (ARRAY *) emalloc(sizeof(ARRAY));
    b->page_size = size;
    b->fp = fp;
    for(i = 0; i < ARRAY_CACHE_SIZE; i++)
        b->cache[i] = NULL;

    return b;
}

ARRAY *ARRAY_Create(FILE *fp)
{
ARRAY *b;
ARRAY_Page *root;
int size = ARRAY_PageSize;

    b = ARRAY_New(fp , size);
    root = ARRAY_NewPage(b);

    b->root_page = root->page_number;

    ARRAY_WritePage(b, root);
    ARRAY_FreePage(b, root);

    return b;
}


ARRAY *ARRAY_Open(FILE *fp, unsigned long root_page)
{
ARRAY *b;
int size = ARRAY_PageSize;

    b = ARRAY_New(fp , size);

    b->root_page = root_page;

    return b;
}

unsigned long ARRAY_Close(ARRAY *bt)
{
unsigned long root_page = bt->root_page;
    ARRAY_FlushCache(bt);
    ARRAY_CleanCache(bt);
    efree(bt);
    return root_page;
}


int ARRAY_Put(ARRAY *b, int index, unsigned long value)
{
unsigned long next_page; 
ARRAY_Page *root_page, *tmp = NULL, *prev; 
int i, hash, page_reads, page_index;

    page_reads = index / ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);
    hash = page_reads % ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);
    page_reads /= ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);
    page_index = index % ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);

    root_page = ARRAY_ReadPage(b, b->root_page);
    next_page = UNPACKLONG(*(unsigned long *)ARRAY_Data(root_page, hash));

    prev = NULL;
    for(i = 0; i <= page_reads; i++)
    {
        if(!next_page)
        {
            tmp = ARRAY_NewPage(b);
            ARRAY_WritePage(b,tmp);
            if(!i)
            {
                *(unsigned long *)ARRAY_Data(root_page,hash) = PACKLONG(tmp->page_number);
                 ARRAY_WritePage(b,root_page);
            }
            else
            {
                prev->next = tmp->page_number;
                ARRAY_WritePage(b,prev);
            }
        } 
        else
        {
            tmp = ARRAY_ReadPage(b, next_page);
        }
        if(prev)
            ARRAY_FreePage(b,prev);
        prev = tmp;
        next_page = tmp->next;
    }
    *(unsigned long *)ARRAY_Data(tmp,page_index) = PACKLONG(value);
    ARRAY_WritePage(b,tmp);
    ARRAY_FreePage(b,tmp);
    ARRAY_FreePage(b,root_page);

    return 0;
}


unsigned long ARRAY_Get(ARRAY *b, int index)
{
unsigned long next_page, value; 
ARRAY_Page *root_page, *tmp;
int i, hash, page_reads, page_index;

    page_reads = index / ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);
    hash = page_reads % ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);
    page_reads /= ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);
    page_index = index % ((ARRAY_PageSize - ARRAY_PageHeaderSize) / SizeInt32);

    root_page = ARRAY_ReadPage(b, b->root_page);
    next_page = UNPACKLONG(*(unsigned long *)ARRAY_Data(root_page, hash));

    tmp = NULL;
    for(i = 0; i <= page_reads; i++)
    {
        if(tmp)
            ARRAY_FreePage(b, tmp);
        if(!next_page)
        {
            ARRAY_FreePage(b,root_page);
            return 0L;
        } 
        else
        {
            tmp = ARRAY_ReadPage(b, next_page);
        }
        next_page = tmp->next;
    }
    value = UNPACKLONG(*(unsigned long *)ARRAY_Data(tmp,page_index));
    ARRAY_FreePage(b,tmp);
    ARRAY_FreePage(b,root_page);

    return value;
}



#ifdef DEBUG

#include <time.h>

#define N_TEST 50000000

#ifdef _WIN32
#define FILEMODE_READ           "rb"
#define FILEMODE_WRITE          "wb"
#define FILEMODE_READWRITE      "rb+"
#elif defined(__VMS)
#define FILEMODE_READ           "rb"
#define FILEMODE_WRITE          "wb"
#define FILEMODE_READWRITE      "rb+"
#else
#define FILEMODE_READ           "r"
#define FILEMODE_WRITE          "w"
#define FILEMODE_READWRITE      "r+"
#endif

int main()
{
FILE *fp;
ARRAY *bt;
int i;
static unsigned long nums[N_TEST];
unsigned long root_page;
    srand(time(NULL));



    fp = fopen("kkkkk",FILEMODE_WRITE);
    fclose(fp);
    fp = fopen("kkkkk",FILEMODE_READWRITE);

    fwrite("aaa",1,3,fp);

printf("\n\nIndexing\n\n");

    bt = ARRAY_Create(fp);
    for(i=0;i<N_TEST;i++)
    {
        nums[i] = rand();
//        nums[i]=i;
        ARRAY_Put(bt,i,nums[i]);
        if(nums[i]!= ARRAY_Get(bt,i))
            printf("\n\nmal %d\n\n",i);
        if(!(i%1000))
        {
            ARRAY_FlushCache(bt);
            printf("%d            \r",i);
        }
    }

    root_page = ARRAY_Close(bt);
    fclose(fp);

printf("\n\nUnfreed %d\n\n",num);
printf("\n\nSearching\n\n");

    fp = fopen("kkkkk",FILEMODE_READ);
    bt = ARRAY_Open(fp, root_page);

    for(i=0;i<N_TEST;i++)
    {
        if(nums[i] != ARRAY_Get(bt,i))
            printf("\n\nmal %d\n\n",i);
        if(!(i%1000))
            printf("%d            \r",i);
    }

    root_page = ARRAY_Close(bt);

    fclose(fp);
printf("\n\nUnfreed %d\n\n",num);

}

#endif
