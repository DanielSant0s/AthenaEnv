#ifndef ATHENA_MACROS_H
#define ATHENA_MACROS_H

#define lambda(return_type, function_body) \
({ \
      return_type __fn__ function_body \
          __fn__; \
})

#define in ,

#define fixed_each(type, ep, arr)  ((type ep) = (arr); (ep) < (arr) + (arr##_size); (ep)++)
#define autotype_each(ep, arr, size)  (typeof(&*(arr)) (ep) = (arr); (ep) < (arr) + (size); (ep)++)

#define clamp(x, minv, maxv) ((x) < (minv) ? (minv) : ((x) > (maxv) ? (maxv) : (x)))

#define xstringify(s) stringify(s)
#define stringify(s) #s

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

#endif