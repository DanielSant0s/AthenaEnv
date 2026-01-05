#ifndef N32_CALL_H
#define N32_CALL_H

#include <stdint.h>
#include <stdbool.h>


#define call_function(return_type, function) ((return_type ## _return_call)function)

inline void process_float_arg(float val, int pos) {
	switch(pos)
	{
	case 0:
		asm volatile ("mov.s $f12, %0" : : "f" (val));
		break;
	case 1:  
		asm volatile ("mov.s $f13, %0" : : "f" (val));
		break;
	case 2:  
		asm volatile ("mov.s $f14, %0" : : "f" (val));
		break;
	case 3:  
		asm volatile ("mov.s $f15, %0" : : "f" (val));
		break;
	case 4:  
		asm volatile ("mov.s $f16, %0" : : "f" (val));
		break;
	case 5:  
		asm volatile ("mov.s $f17, %0" : : "f" (val));
		break;
	case 6:  
		asm volatile ("mov.s $f18, %0" : : "f" (val));
		break;
	case 7: 
		asm volatile ("mov.s $f19, %0" : : "f" (val));
		break;
	case 8:  
		asm volatile ("mov.s $f20, %0" : : "f" (val));
		break;
	case 9:  
		asm volatile ("mov.s $f21, %0" : : "f" (val));
		break;
	case 10: 
		asm volatile ("mov.s $f22, %0" : : "f" (val));
		break;
	case 11: 
		asm volatile ("mov.s $f23, %0" : : "f" (val));
		break;
	case 12: 
		asm volatile ("mov.s $f24, %0" : : "f" (val));
		break;
	case 13: 
		asm volatile ("mov.s $f25, %0" : : "f" (val));
		break;
	case 14: 
		asm volatile ("mov.s $f26, %0" : : "f" (val));
		break;
	}
}

typedef union 
{
    int64_t	    long_arg;
    uint64_t   ulong_arg;
    int32_t		 int_arg;
    uint32_t	uint_arg;
    int16_t    short_arg;
    uint16_t  ushort_arg;
    int8_t	    char_arg;
    uint8_t	   uchar_arg;

    bool	    bool_arg;

	float	   float_arg;

    void*	     ptr_arg;
    char*	  string_arg;
} func_arg;

typedef enum
{
	NONTYPE_VOID,
	TYPE_LONG,
	TYPE_ULONG,
	TYPE_INT,
	TYPE_UINT,
	TYPE_SHORT, 
	TYPE_USHORT,
	TYPE_CHAR,
	TYPE_UCHAR,
	TYPE_BOOL,
	TYPE_FLOAT,
	TYPE_PTR,
	TYPE_STRING,

	TYPE_BUFFER,
} arg_types;

typedef void (*void_return_call)(
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg
);

typedef int (*int_return_call)(
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg
);

typedef bool (*bool_return_call)(
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg
);

typedef float (*float_return_call)(
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg
);

typedef char *(*string_return_call)(
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg, 
	func_arg, func_arg, func_arg, func_arg
);


#endif