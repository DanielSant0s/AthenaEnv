#ifndef ATHENA_MACROS_H
#define ATHENA_MACROS_H

#define lambda(return_type, function_body) \
({ \
      return_type __fn__ function_body \
          __fn__; \
})

#endif