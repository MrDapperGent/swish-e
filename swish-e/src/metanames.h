/* Jose Ruiz 2000/01 Definitions for MetaNames/Fields */

#define META_INDEX    1      /* bynary 00000001 */  /* Meta is indexed */
#define META_PROP    2      /* bynary 00000010 */ /* Also stored as property */
#define META_NUMBER    4      /* bynary 00000100 */ /* Data is binary number */
#define META_DATE    8      /* bynary 00001000 */ /* Data is binary date */


/* Macros to test the type of a MetaName */
#define is_meta_index(x)  ((x)->metaType & META_INDEX)
#define is_meta_property(x)  ((x)->metaType & META_PROP)
#define is_meta_number(x)  ((x)->metaType & META_NUMBER)
#define is_meta_date(x)  ((x)->metaType & META_DATE)
#define is_meta_string(x)  (((x)->metaType | (META_INDEX | META_PROP)) == (META_INDEX | META_PROP))

/* Macro to check if both metaNames are of the same type (number, date, ...) */
#define match_meta_type(x,y)  (((x)>>META_PROP)==((y)>>META_PROP))


void add_default_metanames(IndexFILE *);
struct metaEntry * getMetaNameData(INDEXDATAHEADER *, char *);
struct metaEntry * getMetaIDData(INDEXDATAHEADER *, int);
void addMetaEntry(INDEXDATAHEADER *, char *, int, int, int *, int *);
void freeMetaEntries( INDEXDATAHEADER * );
int getMetaNameID(IndexFILE *, char *);
int isDontBumpMetaName(SWISH *sw,char *tag);
struct metaEntry **addNewMetaEntry(struct metaEntry **metaEntryArray, int *metaCounter, int metaID, char *metaWord, int metaType, int *sort_array);

