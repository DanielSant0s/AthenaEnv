/*
--------------------------------------------------------------------
By Bob Jenkins, 1996.  hashtab.c.  Public Domain.
--------------------------------------------------------------------
*/

#include <string.h>
#include <stdlib.h>

#ifndef STANDARD
#include "standard.h"
#endif
#ifndef LOOKUPA
#include "lookupa.h"
#endif
#ifndef HASHTAB
#include "hashtab.h"
#endif
#ifndef RECYCLE
#include "recycle.h"
#endif

static void hgrow(htab *t)
{
  ub4     newsize = (ub4)1<<(++t->logsize);
  ub4     newmask = newsize-1;
  ub4     i;
  hitem **oldtab = t->table;
  hitem **newtab = (hitem **)malloc(newsize*sizeof(hitem *));

  for (i=0; i<newsize; ++i) newtab[i] = (hitem *)0;
  t->table = newtab;
  t->mask = newmask;

  for (i=newsize>>1; i--;)
  {
    hitem *curr, *that;
    for (curr = oldtab[i]; curr;)
    {
      hitem **newplace;
      that = curr;
      curr = curr->next;
      newplace = &newtab[(that->hval & newmask)];
      that->next = *newplace;
      *newplace = that;
    }
  }

  hfirst(t);
  free((char *)oldtab);
}

htab *hcreate(word logsize)
{
  ub4 i, len;
  htab *t = (htab *)malloc(sizeof(htab));

  len = ((ub4)1<<logsize);
  t->table = (hitem **)malloc(sizeof(hitem *)*(ub4)len);
  for (i=0; i<len; ++i) t->table[i] = (hitem *)0;
  t->logsize = logsize;
  t->mask = len-1;
  t->count = 0;
  t->apos = (ub4)0;
  t->ipos = (hitem *)0;
  t->space = remkroot(sizeof(hitem));
  t->bcount = 0;
  return t;
}

void hdestroy(htab *t)
{
  refree(t->space);
  free((char *)t->table);
  free((char *)t);
}

word hfind(htab *t, const ub1 *key, ub4 keyl)
{
  hitem *h;
  ub4    x = lookup((ub1 *)key, keyl, 0);
  ub4    y;
  for (h = t->table[y=(x&t->mask)]; h; h = h->next)
  {
    if ((x == h->hval) &&
        (keyl == h->keyl) &&
        !memcmp(key, h->key, keyl))
    {
      t->apos = y;
      t->ipos = h;
      return TRUE;
    }
  }
  return FALSE;
}

word hadd(htab *t, ub1 *key, ub4 keyl, void *stuff)
{
  hitem  *h, **hp;
  ub4     y, x = lookup(key, keyl, 0);

  for (h = t->table[(y=(x&t->mask))]; h; h = h->next)
  {
    if ((x == h->hval) &&
        (keyl == h->keyl) &&
        !memcmp(key, h->key, keyl))
    {
      t->apos = y;
      t->ipos = h;
      return FALSE;
    }
  }

  h = (hitem *)renew(t->space);

  if (++t->count > (ub4)1<<(t->logsize))
  {
    hgrow(t);
    y = (x&t->mask);
  }

  h->key   = key;
  h->keyl  = keyl;
  h->stuff = stuff;
  h->hval  = x;
  hp = &t->table[y];
  h->next = *hp;
  *hp = h;
  t->ipos = h;
  t->apos = y;

  return TRUE;
}

word hdel(htab *t)
{
  hitem  *h;
  hitem **ip;

  if (!(h = t->ipos)) return FALSE;

  for (ip = &t->table[t->apos]; *ip != h; ip = &(*ip)->next)
    ;
  *ip = (*ip)->next;
  --(t->count);

  if (!(t->ipos = h->next)) hnbucket(t);

  redel(t->space, h);

  return TRUE;
}

word hfirst(htab *t)
{
  t->apos = t->mask;
  (void)hnbucket(t);
  return (t->ipos != (hitem *)0);
}

word hnbucket(htab *t)
{
  ub4  oldapos = t->apos;
  ub4  end = (ub4)1<<(t->logsize);
  ub4  i;

  for (i=oldapos+1; i<end; ++i)
  {
    if (t->table[i&t->mask])
    {
      t->apos = i;
      t->ipos = t->table[i];
      return TRUE;
    }
  }

  for (i=0; i<=oldapos; ++i)
  {
    if (t->table[i])
    {
      t->apos = i;
      t->ipos = t->table[i];
      return FALSE;
    }
  }

  return FALSE;
}
