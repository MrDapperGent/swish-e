/* */

#define WORDDATA_MAX_REUSABLE_PAGES 16

typedef struct WORDDATA_Reusable_Page
{
    unsigned long      page_number;
    int                page_size;
} WORDDATA_Reusable_Page;

typedef struct WORDDATA_Page
{
    unsigned long      page_number;
    int                used_blocks;
    int                n;
    int                modified;
    int                in_use;

    struct WORDDATA_Page  *next_cache;

    unsigned char data[0];        /* Page data */
} WORDDATA_Page;

#define WORDDATA_CACHE_SIZE 97

typedef struct WORDDATA
{
    WORDDATA_Page      *last_put_page;  /* last page after an insert (put) */
    WORDDATA_Page      *last_del_page;  /* last page after a delete (del) */
    WORDDATA_Page      *last_get_page;  /* last page after a read (get) */
    struct WORDDATA_Page *cache[WORDDATA_CACHE_SIZE];
    int                page_counter;
    unsigned long      lastid;
    int                num_Reusable_Pages;
    WORDDATA_Reusable_Page Reusable_Pages[WORDDATA_MAX_REUSABLE_PAGES];
    FILE *fp;
} WORDDATA;

WORDDATA *WORDDATA_Open(FILE *fp);
void WORDDATA_Close(WORDDATA *bt);
unsigned long WORDDATA_Put(WORDDATA *b, unsigned int len, unsigned char *data);
unsigned char * WORDDATA_Get(WORDDATA *b, unsigned long global_id, unsigned int *len);
void WORDDATA_Del(WORDDATA *b, unsigned long global_id, unsigned int *len);
