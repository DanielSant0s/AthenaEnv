/*
--------------------------------------------------------------------
lookupa.c, by Bob Jenkins, December 1996.  Same as lookup2.c
Use this code however you wish.  Public Domain.  No warranty.
Source is http://burtleburtle.net/bob/c/lookupa.c
--------------------------------------------------------------------
*/
#ifndef STANDARD
#include "standard.h"
#endif
#ifndef LOOKUPA
#include "lookupa.h"
#endif

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

ub4 lookup(ub1 *k, ub4 length, ub4 level)
{
   ub4 a, b, c, len;

   len = length;
   a = b = 0x9e3779b9;
   c = level;

   while (len >= 12)
   {
      a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
      b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
      c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   c += length;
   switch(len)
   {
   case 11: c+=((ub4)k[10]<<24); /* fallthrough */
   case 10: c+=((ub4)k[9]<<16);  /* fallthrough */
   case 9 : c+=((ub4)k[8]<<8);   /* fallthrough */
   case 8 : b+=((ub4)k[7]<<24);  /* fallthrough */
   case 7 : b+=((ub4)k[6]<<16);  /* fallthrough */
   case 6 : b+=((ub4)k[5]<<8);   /* fallthrough */
   case 5 : b+=k[4];             /* fallthrough */
   case 4 : a+=((ub4)k[3]<<24);  /* fallthrough */
   case 3 : a+=((ub4)k[2]<<16);  /* fallthrough */
   case 2 : a+=((ub4)k[1]<<8);   /* fallthrough */
   case 1 : a+=k[0];             /* fallthrough */
   }
   mix(a,b,c);
   return c;
}

#define mixc(a,b,c,d,e,f,g,h) \
{ \
   a^=b<<11; d+=a; b+=c; \
   b^=c>>2;  e+=b; c+=d; \
   c^=d<<8;  f+=c; d+=e; \
   d^=e>>16; g+=d; e+=f; \
   e^=f<<10; h+=e; f+=g; \
   f^=g>>4;  a+=f; g+=h; \
   g^=h<<8;  b+=g; h+=a; \
   h^=a>>9;  c+=h; a+=b; \
}

void checksum(ub1 *k, ub4 len, ub4 *state)
{
   ub4 a,b,c,d,e,f,g,h,length;

   length = len;
   a=state[0]; b=state[1]; c=state[2]; d=state[3];
   e=state[4]; f=state[5]; g=state[6]; h=state[7];

   while (len >= 32)
   {
      a += (k[0] +(k[1]<<8) +(k[2]<<16) +(k[3]<<24));
      b += (k[4] +(k[5]<<8) +(k[6]<<16) +(k[7]<<24));
      c += (k[8] +(k[9]<<8) +(k[10]<<16)+(k[11]<<24));
      d += (k[12]+(k[13]<<8)+(k[14]<<16)+(k[15]<<24));
      e += (k[16]+(k[17]<<8)+(k[18]<<16)+(k[19]<<24));
      f += (k[20]+(k[21]<<8)+(k[22]<<16)+(k[23]<<24));
      g += (k[24]+(k[25]<<8)+(k[26]<<16)+(k[27]<<24));
      h += (k[28]+(k[29]<<8)+(k[30]<<16)+(k[31]<<24));
      mixc(a,b,c,d,e,f,g,h);
      mixc(a,b,c,d,e,f,g,h);
      mixc(a,b,c,d,e,f,g,h);
      mixc(a,b,c,d,e,f,g,h);
      k += 32; len -= 32;
   }

   h += length;
   switch(len)
   {
   case 31: h+=(k[30]<<24); /* fallthrough */
   case 30: h+=(k[29]<<16); /* fallthrough */
   case 29: h+=(k[28]<<8);  /* fallthrough */
   case 28: g+=(k[27]<<24); /* fallthrough */
   case 27: g+=(k[26]<<16); /* fallthrough */
   case 26: g+=(k[25]<<8);  /* fallthrough */
   case 25: g+=k[24];       /* fallthrough */
   case 24: f+=(k[23]<<24); /* fallthrough */
   case 23: f+=(k[22]<<16); /* fallthrough */
   case 22: f+=(k[21]<<8);  /* fallthrough */
   case 21: f+=k[20];       /* fallthrough */
   case 20: e+=(k[19]<<24); /* fallthrough */
   case 19: e+=(k[18]<<16); /* fallthrough */
   case 18: e+=(k[17]<<8);  /* fallthrough */
   case 17: e+=k[16];       /* fallthrough */
   case 16: d+=(k[15]<<24); /* fallthrough */
   case 15: d+=(k[14]<<16); /* fallthrough */
   case 14: d+=(k[13]<<8);  /* fallthrough */
   case 13: d+=k[12];       /* fallthrough */
   case 12: c+=(k[11]<<24); /* fallthrough */
   case 11: c+=(k[10]<<16); /* fallthrough */
   case 10: c+=(k[9]<<8);   /* fallthrough */
   case 9 : c+=k[8];        /* fallthrough */
   case 8 : b+=(k[7]<<24);  /* fallthrough */
   case 7 : b+=(k[6]<<16);  /* fallthrough */
   case 6 : b+=(k[5]<<8);   /* fallthrough */
   case 5 : b+=k[4];        /* fallthrough */
   case 4 : a+=(k[3]<<24);  /* fallthrough */
   case 3 : a+=(k[2]<<16);  /* fallthrough */
   case 2 : a+=(k[1]<<8);   /* fallthrough */
   case 1 : a+=k[0];        /* fallthrough */
   }
   mixc(a,b,c,d,e,f,g,h);
   mixc(a,b,c,d,e,f,g,h);
   mixc(a,b,c,d,e,f,g,h);
   mixc(a,b,c,d,e,f,g,h);

   state[0]=a; state[1]=b; state[2]=c; state[3]=d;
   state[4]=e; state[5]=f; state[6]=g; state[7]=h;
}
