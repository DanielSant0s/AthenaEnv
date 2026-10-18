/*
--------------------------------------------------------------------
By Bob Jenkins, 1996.  hash.h.  Public Domain.
--------------------------------------------------------------------
*/

#ifndef STANDARD
#include "standard.h"
#endif

#ifndef HASHTAB
#define HASHTAB

#ifdef __cplusplus
extern "C" {
#endif

struct hitem
{
  ub1          *key;      /* key that is hashed */
  ub4           keyl;     /* length of key */
  void         *stuff;    /* stuff stored in this hitem */
  ub4           hval;     /* hash value */
  struct hitem *next;     /* next hitem in list */
};
typedef  struct hitem  hitem;


struct htab
{
  struct hitem **table;   /* hash table, array of size 2^logsize */
  word           logsize; /* log of size of table */
  size_t         mask;    /* (hashval & mask) is position in table */
  ub4            count;   /* how many items in this hash table so far? */
  ub4            apos;    /* position in the array */
  struct hitem  *ipos;    /* current item in the array */
  struct reroot *space;   /* space for the hitems */
  ub4            bcount;  /* # hitems useable in current block */
};
typedef  struct htab  htab;

extern htab *hcreate(word logsize);
extern void  hdestroy(htab *t);

#define hcount(t) ((t)->count)
#define hkey(t)   ((t)->ipos->key)
#define hkeyl(t)  ((t)->ipos->keyl)
#define hstuff(t) ((t)->ipos->stuff)

extern word  hfind(htab *t, const ub1 *key, ub4 keyl);
extern word  hadd(htab *t, ub1 *key, ub4 keyl, void *stuff);
extern word  hdel(htab *t);
extern word  hfirst(htab *t);

#define hnext(t) \
  ((!(t)->ipos) ? FALSE :  \
   ((t)->ipos=(t)->ipos->next) ? TRUE : hnbucket(t))

extern word  hnbucket(htab *t);
extern void  hstat(htab *t);

#ifdef __cplusplus
}
#endif

#endif   /* HASHTAB */
