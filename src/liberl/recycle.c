/*
--------------------------------------------------------------------
By Bob Jenkins, September 1996.  recycle.c
You may use this code in any way you wish, and it is free.  No warranty.
--------------------------------------------------------------------
*/

#include <string.h>
#include <stdlib.h>

#ifndef STANDARD
# include "standard.h"
#endif
#ifndef RECYCLE
# include "recycle.h"
#endif

reroot *remkroot(size_t size)
{
   reroot *r = (reroot *)remalloc(sizeof(reroot));
   r->list = (recycle *)0;
   r->trash = (recycle *)0;
   r->size = align(size);
   r->logsize = RESTART;
   r->numleft = 0;
   return r;
}

void refree(struct reroot *r)
{
   while (r->list)
   {
      recycle *temp = r->list->next;
      free((char *)r->list);
      r->list = temp;
   }
   free((char *)r);
   return;
}

char *renewx(struct reroot *r)
{
   recycle *temp;
   if (r->trash)
   {
      temp = r->trash;
      r->trash = temp->next;
      (void)memset((void *)temp, 0, r->size);
   }
   else
   {
      r->numleft = r->size*((ub4)1<<r->logsize);
      if (r->numleft < REMAX) ++r->logsize;
      temp = (recycle *)remalloc(sizeof(recycle) + r->numleft);
      temp->next = r->list;
      r->list = temp;
      r->numleft-=r->size;
      temp = (recycle *)((char *)(r->list+1)+r->numleft);
   }
   return (char *)temp;
}

char *remalloc(size_t len)
{
  char *x = (char *)malloc(len);
  return x;
}
