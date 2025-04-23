#ifndef ATHENA_MACROS_H
#define ATHENA_MACROS_H

#define lambda(return_type, function_body) \
({ \
      return_type __fn__ function_body \
          __fn__; \
})

#define clamp(x, minv, maxv) ((x) < (minv) ? (minv) : ((x) > (maxv) ? (maxv) : (x)))

#define xstringify(s) str(s)
#define stringify(s) #s

#endif