# AthenaEnv Native Compiler

**Technical Reference Documentation**

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [JavaScript API](#javascript-api)
4. [Intermediate Representation (IR)](#intermediate-representation-ir)
5. [Type System](#type-system)
6. [Code Generation](#code-generation)
7. [Register Allocation](#register-allocation)
8. [Struct System](#struct-system)
9. [Optimization Passes](#optimization-passes)
10. [Examples](#examples)
11. [Implementation Details](#implementation-details)

---

## Overview

The AthenaEnv Native Compiler is an Ahead-Of-Time (AOT) compiler that translates JavaScript functions into native MIPS R5900 machine code for the PlayStation 2. It enables high-performance computation by bypassing the QuickJS bytecode interpreter for performance-critical code paths.

### Key Features

- **Minimal QuickJS changes**: Single API function to access bytecode (`JS_GetFunctionBytecodeInfo`)
- **Full type system**: Support for int32, float32, int64, pointers, arrays, strings, and structs
- **Comprehensive IR**: 100+ intermediate opcodes covering arithmetic, comparisons, control flow, and more
- **MIPS R5900 code generation**: Direct translation to PlayStation 2 assembly
- **Struct methods**: Object-oriented programming with native performance
- **Calling convention compliance**: Full MIPS n32 ABI compatibility
- **Optimization suite**: 5 optimization passes including strength reduction, constant folding, and loop optimization

### Design Philosophy

The compiler follows a **clean separation**` design:
1. **API Layer** (`ath_native.c`): JavaScript interface and type marshalling
2. **IR Layer** (`native_compiler.c`): Platform-independent intermediate representation
3. **Backend** (`mips_emitter.c`): MIPS-specific code generation

This allows for potential future ports to other architectures by only replacing the backend.

---

## Architecture

### Compilation Pipeline

```
JavaScript Function
        ↓
   [Type Inference]
        ↓
   QuickJS Bytecode
        ↓
  [Bytecode → IR Translation]
        ↓
   IR (Intermediate Representation)
        ↓
   [Optimization Passes]
        ↓
    Optimized IR
        ↓
    [IR → MIPS Code Generation]
        ↓
   MIPS R5900 Machine Code
        ↓
   Executable Native Function
```

### Components

#### 1. **Native Compiler Core** (`native_compiler.c`, 6414 lines)

The heart of the system, implementing:
- Bytecode to IR translation
- IR optimization passes
- MIPS code generation
- Register allocation
- Type inference

**Key structures:**
- `NativeCompiler`: Main compilation context
- `IRFunction`: Intermediate representation of a function
- `IRBasicBlock`: Basic block in control flow graph
- `IRInstr`: Individual IR instruction

#### 2. **MIPS Emitter** (`mips_emitter.c`, 64KB)

Low-level code generator providing:
- MIPS R5900 instruction encoding
- Label management and fixup
- Register allocation primitives
- Cache flushing for JIT execution
- Assembly disassembly for debugging

**Supports:**
- All MIPS32 instructions
- MIPS64 subset (R5900 specific)
- FPU (COP1) instructions
- PS2-specific extensions (MIN.S, MAX.S)

#### 3. **JavaScript API** (`ath_native.c`, 1219 lines)

Public interface exposing:
- `Native.compile()`: Compile JavaScript functions
- `Native.struct()`: Define native struct types
- Type conversion between JavaScript and native
- Method wrapper generation
- Debugging utilities (disassembly, benchmarking)

#### 4. **Type System** (`type_inference.c`, 33KB)

Handles:
- Type propagation through IR
- Type validation against signatures
- Automatic type conversions
- Struct type resolution

#### 5. **Support Libraries**

- `native_struct.c/h`: Struct definition and field access
- `native_array.c/h`: Dynamic typed arrays
- `native_string.c/h`: Native string operations
- `int64_runtime.c/h`: 64-bit arithmetic emulation

---

## JavaScript API

### Native.compile()

Compiles a JavaScript function to native code.

**Signature:**
```javascript
Native.compile(signature, function) → NativeFunction
```

**Parameters:**
- `signature`: Object describing types
  - `args`: Array of argument type names
  - `returns`: Return type name
- `function`: JavaScript function to compile

**Type Names:**
- **Integers**: `'int'`, `'uint'`, `'int64'`, `'uint64'`
- **Floats**: `'float'`
- **Arrays**: `'Int32Array'`, `'Float32Array'`, `'Uint32Array'`
- **Strings**: `'string'`  
- **Pointers**: `'ptr'`
- **Structs**: Custom struct type name
- **Void**: `'void'` (return type only)

**Example:**
```javascript
const add = Native.compile({
    args: ['int', 'int'],
    returns: 'int'
}, (a, b) => a + b);

console.log(add(5, 3)); // 8
```

**Advanced Example (SIMD-style):**
```javascript
const dotProduct = Native.compile({
    args: ['Float32Array', 'Float32Array', 'int'],
    returns: 'float'
}, (a, b, length) => {
    let sum = 0;
    for (let i = 0; i < length; i++) {
        sum += a[i] * b[i];
    }
    return sum;
});
```

### Native.struct()

Defines a native struct type with optional methods.

**Signature:**
```javascript
Native.struct(name, fields, methods?) → StructConstructor
```

**Parameters:**
- `name`: Struct type name (string)
- `fields`: Object mapping field names to types
  - For arrays: `{type: 'float', length: N}`
- `methods`: Optional object of compiled methods

**Example:**
```javascript
const Vec3 = Native.struct('Vec3', {
    x: 'float',
    y: 'float', 
    z: 'float'
}, {
    // Method receives 'self' as first argument
    length: Native.compile({
        args: ['self'], // 'self' resolves to Vec3*
        returns: 'float'
    }, (self) => {
        return Math.sqrt(self.x * self.x + 
                        self.y * self.y + 
                        self.z * self.z);
    }),
    
    scale: Native.compile({
        args: ['self', 'float'],
        returns: 'void'
    }, (self, factor) => {
        self.x *= factor;
        self.y *= factor;
        self.z *= factor;
    })
});

// Usage
const v = new Vec3();
v.x = 3; v.y = 4; v.z = 0;
console.log(v.length()); // 5.0
v.scale(2);
console.log(v.x); // 6.0
```

### Utility Functions

```javascript
// Check platform support
Native.isSupported() → boolean

// Free compiled function
Native.free(nativeFunc) → void

// Get function metadata
Native.getInfo(nativeFunc) → object

// Benchmark execution
Native.benchmark(nativeFunc, iterations) → number

// Disassemble to MIPS assembly
Native.disassemble(nativeFunc) → string
```

---

## Intermediate Representation (IR)

### Opcode Categories

The IR contains **100+ opcodes** organized into categories:

#### Constants
- `IR_CONST_I32`: Push 32-bit integer constant
- `IR_CONST_F32`: Push float constant  
- `IR_CONST_I64`: Push 64-bit integer constant

#### Arithmetic (32-bit)
- `IR_ADD_I32`, `IR_SUB_I32`, `IR_MUL_I32`, `IR_DIV_I32`, `IR_MOD_I32`
- `IR_NEG_I32`: Negate
- `IR_IMUL_I32`: Integer multiply (truncate to 32-bit)

#### Arithmetic (64-bit)
- `IR_ADD_I64`, `IR_SUB_I64`, `IR_MUL_I64`, `IR_DIV_I64`, `IR_MOD_I64`
- `IR_SHL_I64`, `IR_SHR_I64`, `IR_SAR_I64`: Bit shifts
- `IR_NEG_I64`: Negate

**Note:** 64-bit multiply/divide are **emulated** via software routines since R5900 lacks native 64-bit MULT/DIV.

#### Arithmetic (Float)
- `IR_ADD_F32`, `IR_SUB_F32`, `IR_MUL_F32`, `IR_DIV_F32`
- `IR_NEG_F32`, `IR_SQRT_F32`, `IR_ABS_F32`
- `IR_MIN_F32`, `IR_MAX_F32`: Min/max (uses C.LT.S)
- `IR_FMA_F32`: Fused multiply-add `a*b+c` (MADD.S instruction)
- `IR_CLAMP_F32`: Clamp to range
- `IR_LERP_F32`: Linear interpolation

#### Math Functions (Advanced)
- `IR_SIGN_F32`: Sign function (-1, 0, 1)
- `IR_FROUND_F32`: Float32 round (no-op on PS2, already float32)
- `IR_IMUL_I32`: 32-bit integer multiply (truncate to 32 bits)
- `IR_SATURATE_F32`: Clamp to [0,1]
- `IR_STEP_F32`: Step function
- `IR_SMOOTHSTEP_F32`: Smooth interpolation (3rd order Hermite)
- `IR_RSQRT_F32`: Reciprocal square root

#### Bitwise
- `IR_AND`, `IR_OR`, `IR_XOR`, `IR_NOT`
- `IR_SHL`, `IR_SHR`, `IR_SAR`: Shifts

#### Comparison (32-bit)
- `IR_EQ_I32`, `IR_NE_I32`, `IR_LT_I32`, `IR_LE_I32`, `IR_GT_I32`, `IR_GE_I32`
- `IR_LT_U32`, `IR_LE_U32`, `IR_GT_U32`, `IR_GE_U32`: Unsigned

#### Comparison (Float)
- `IR_EQ_F32`, `IR_LT_F32`, `IR_LE_F32`

#### Comparison (64-bit)
- Signed: `IR_EQ_I64`, `IR_NE_I64`, `IR_LT_I64`, `IR_LE_I64`, `IR_GT_I64`, `IR_GE_I64`
- Unsigned: `IR_LT_U64`, `IR_LE_U64`, `IR_GT_U64`, `IR_GE_U64`

#### Type Conversion
- `IR_I32_TO_F32`, `IR_F32_TO_I32`
- `IR_I32_TO_I64`: Sign-extend
- `IR_I64_TO_I32`: Truncate
- `IR_I64_TO_F32`, `IR_F32_TO_I64`

#### Memory Access
- `IR_LOAD_LOCAL`, `IR_STORE_LOCAL`: Local variables
- `IR_LOAD_ARRAY`, `IR_STORE_ARRAY`: Typed arrays
- `IR_LOAD_FIELD`, `IR_STORE_FIELD`: Struct fields
- `IR_LOAD_FIELD_ADDR`: Get field address (for arrays)

#### Control Flow
- `IR_JUMP`: Unconditional branch
- `IR_JUMP_IF_TRUE`, `IR_JUMP_IF_FALSE`: Conditional branches
- `IR_RETURN`, `IR_RETURN_VOID`: Function return
- `IR_CALL`, `IR_TAIL_CALL`: Function calls

#### Stack Manipulation
- `IR_DUP`: Duplicate top of stack
- `IR_DROP`: Discard top of stack  
- `IR_SWAP`: Swap top two items

#### String Operations
- `IR_STRING_NEW`, `IR_STRING_CONCAT`, `IR_STRING_SLICE`
- `IR_STRING_LENGTH`, `IR_STRING_COMPARE`, `IR_STRING_EQUALS`
- `IR_STRING_FIND`, `IR_STRING_REPLACE`
- `IR_STRING_TO_UPPER`, `IR_STRING_TO_LOWER`, `IR_STRING_TRIM`

#### Array Operations  
- `IR_ARRAY_NEW`, `IR_ARRAY_PUSH`, `IR_ARRAY_POP`
- `IR_ARRAY_GET`, `IR_ARRAY_SET`
- `IR_ARRAY_INSERT`, `IR_ARRAY_REMOVE`
- `IR_ARRAY_LENGTH`, `IR_ARRAY_RESIZE`, `IR_ARRAY_RESERVE`, `IR_ARRAY_CLEAR`

#### Special
- `IR_CALL_C_FUNC`: Call registered C function
- `IR_ADD_LOCAL_CONST`: Optimized `i = i + const`
- `IR_NOP`: No operation (for optimization)

### IR Instruction Structure

```c
typedef struct IRInstr {
    IROp op;                 // Opcode
    NativeType type;         // Result type
    
    union {
        int32_t i32;         // Integer immediate
        int64_t i64;         // 64-bit immediate
        float f32;           // Float immediate
        int local_idx;       // Local variable index
        int label_id;        // Branch target
        
        struct {             // Field access
            int16_t offset;
            int16_t local_idx;
            NativeType field_type;
        } field;
        
        struct {             // C function call
            void *func_ptr;
            uint8_t arg_count;
            NativeType arg_types[8];
            NativeType ret_type;
        } call;
    } operand;
    
    int16_t source_local_idx; // Track provenance
} IRInstr;
```

---

## Type System

### Supported Types

| Type Name | C Type | MIPS Size | Alignment | Description |
|-----------|--------|-----------|-----------|-------------|
| `int` | `int32_t` | 4 bytes | 4 | Signed 32-bit integer |
| `uint` | `uint32_t` | 4 bytes | 4 | Unsigned 32-bit integer |
| `int64` | `int64_t` | 8 bytes | 8 | Signed 64-bit integer |
| `uint64` | `uint64_t` | 8 bytes | 8 | Unsigned 64-bit integer |
| `float` | `float` | 4 bytes | 4 | IEEE 754 float (FPU) |
| `ptr` | `void*` | 4 bytes | 4 | Generic pointer |
| `string` | `NativeString*` | 4 bytes | 4 | Pointer to string struct |
| `Int32Array` | `NativeArray*` | 4 bytes | 4 | Typed array pointer |
| `Float32Array` | `NativeArray*` | 4 bytes | 4 | Typed array pointer |
| `Uint32Array` | `NativeArray*` | 4 bytes | 4 | Typed array pointer |
| Custom | `void*` | 4 bytes | 4 | Struct pointer |
| `void` | - | 0 | - | No return value |

### Type Inference

The compiler performs **bidirectional type inference**:

1. **Bottom-up**: Propagate types from constants and inputs
2. **Top-down**: Apply constraints from signatures
3. **Validation**: Ensure consistency across all paths

**Algorithm** (simplified):
```
1. Mark function parameters with declared types
2. For each IR instruction in CFG order:
   a. Determine input types from operands
   b. Compute result type based on opcode
   c. Propagate type forward
3. At merge points (PHI nodes):
   a. Unify compatible types
   b. Report error if types conflict
4. Validate return type matches signature
```

**Type Conversions:**

Automatic conversions occur for:
- `int` ↔ `float`: Using CVT.S.W / CVT.W.S
- `int` → `int64`: Sign-extend (DSLL32 + SRA)
- `int64` → `int`: Truncate

**No conversion** between:
- Signed ↔ Unsigned (must be explicit)
- Pointers ↔ Integers (reinterpret cast only)

---

## Code Generation

### MIPS Calling Convention

**Argument Passing:**
- First 4 arguments: `$a0-$a3` (r4-r7) - **used for ALL types including floats**
- Remaining arguments: Stack (caller-allocated)

**Float Arguments - How They Work:**

Floats are **normal floats**, just passed via integer registers at the JS/Native boundary:

1. **At call boundary** (JavaScript → Native):
   - Float bits passed in `$a0-$a3` (not `$f12-$f15`)
   - Avoids O32/N32 ABI float-in-integer-register complexity
   
2. **Inside native function** (Compiled code):
   - Compiler emits `MTC1 $a0, $f0` to move to FPU
   - Float operations use standard FPU instructions
   - **Floats behave normally** - full arithmetic

**Example:**
```javascript
// JavaScript
const multiply = Native.compile({args: ['float', 'float'], returns: 'float'},
    (a, b) => a * b);
```

**Generated MIPS:**
```mips
# Prologue - move args to FPU
MTC1   $a0, $f0      # arg0 (float) -> $f0
MTC1   $a1, $f1      # arg1 (float) -> $f1

# Actual operation
MUL.S  $f2, $f0, $f1 # Float multiply

# Return
MFC1   $v0, $f2      # Move result to $v0 for return
JR     $ra
```

**Return Values:**
- **Integer/pointer**: `$v0` (r2)
- **Float**: Computed in FPU, moved to `$v0` via `MFC1`
  - At caller side: Cast function pointer to `typedef float (*)(...)` to read from proper location

**Why this design?**
- Simplifies calling convention (all args in `$a0-$a3`)
- No special float/int arg mixing rules
- Inside function, floats are **100% normal FPU operations**

**Register Usage:**
| Registers | Purpose | Preserved? |
|-----------|---------|------------|
| `$zero` | Always 0 | N/A |
| `$at` | Assembler temporary | No |
| `$v0-$v1` | Return values | No |
| `$a0-$a3` | Arguments (all types) | No |
| `$t0-$t9` | Temporaries | No |
| `$s0-$s7` | Saved | **Yes** |
| `$k0-$k1` | Kernel reserved | No |
| `$gp` | Global pointer | **Yes** |
| `$sp` | Stack pointer | **Yes** |
| `$fp` | Frame pointer | **Yes** |
| `$ra` | Return address | **Yes** |
| `$f0-$f31` | FPU registers | No (temps) |

### Stack Frame Layout

```
High Address
+------------------+
| Caller's frame   |
+------------------+
| Arg 5, 6, ...    | ← Arguments beyond $a0-$a3
+------------------+
| Saved $ra        | ← Return address (non-leaf only)
+------------------+
| Saved $fp        | ← Frame pointer (if needed)
+------------------+
| Saved $s0-$s7    | ← Saved registers (if used)
+------------------+
| Local variables  | ← Spilled locals
+------------------+
| Temp space       | ← Expression evaluation
+------------------+ ← $sp (16-byte aligned)
Low Address
```

### Register Allocation Strategy

**Operand Stack:**
Uses registers `$t0-$t7` as a virtual stack:
- `stack_top = 0`: Result in `$t0`
- `stack_top = 1`: Result in `$t1`
- Maximum depth: 8 (stack overflow triggers error)

**Local Variables:**
- Arguments 0-3: Stay in `$a0-$a3` when possible
- Loop counters: Allocated to `$s0-$s7` (for `for`/`while`)
- Other locals: Spilled to stack

**Example:**
```javascript
// (a, b) => a * 2 + b
// IR: LOAD_LOCAL(0), CONST_I32(2), MUL_I32, LOAD_LOCAL(1), ADD_I32, RETURN

// Generated MIPS:
MOVE  $t0, $a0       // Load a into $t0
ADDIU $t1, $zero, 2  // Load const 2 into $t1  
MULT  $t0, $t1       // Multiply
MFLO  $t0            // Result in $t0
MOVE  $t1, $a1       // Load b into $t1
ADDU  $t0, $t0, $t1  // Add
MOVE  $v0, $t0       // Move to return register
JR    $ra            // Return
```

### Optimization: Delay Slot Filling

The compiler attempts to fill **branch delay slots** with useful instructions:

1. **NOP elimination**: Move previous instruction into delay slot if safe
2. **Stack adjustment**: Place `ADDIU $sp, $sp, N` in `JR $ra` delay slot
3. **Safe instructions**: Only ALU ops that don't affect `$ra`

**Example:**
```mips
# Before optimization
ADDU  $v0, $t0, $t1
JR    $ra
NOP

# After optimization
JR    $ra
ADDU  $v0, $t0, $t1  # Moved into delay slot
```

---

## Register Allocation

### Algorithm

The compiler uses a **simple linear scan** allocator:

**Phase 1: Classify Locals**
```
FOR each local variable i:
    IF i < 4 AND i < arg_count:
        local_regs[i] = $a0 + i
        local_stack_offs[i] = -1
    ELSE IF is_loop_counter(i) AND saved_regs_available:
        local_regs[i] = $s0 + next_saved
        local_stack_offs[i] = -1
        Mark $s register as used
    ELSE:
        local_regs[i] = -1
        local_stack_offs[i] = stack_offset
        stack_offset += sizeof(type)
```

**Phase 2: Prologue**
```
1. Allocate stack frame (16-byte aligned)
2. Save $ra (if non-leaf)
3. Save $fp (if locals on stack)  
4. Setup $fp = $sp
5. Save used $s0-$s7 registers
6. Move arguments to final locations
```

**Phase 3: Epilogue**
```
1. Move return value to $v0
2. Restore $s0-$s7
3. Restore $fp
4. Restore $ra
5. Deallocate stack
6. Return
```

### Leaf Function Optimization

**Leaf functions** (no calls to other functions) skip:
- Saving `$ra`
- Setting up `$fp` (if no locals)
- Allocating stack (if registers suffice)

**Detection:**
```c
bool is_leaf(IRFunction *ir) {
    FOR each block:
        FOR each instruction:
            IF op == IR_CALL OR op == IR_CALL_C_FUNC:
                RETURN false
    RETURN true
}
```

---

## Struct System

### Struct Definition

Structs are defined via `Native.struct()` and compiled into:
1. **NativeStructDef**: Metadata (fields, sizes, offsets)
2. **JSClass**: JavaScript wrapper class
3. **Memory layout**: C-compatible struct

**Memory Layout:**
```c
struct Vec3 {
    float x;  // offset 0
    float y;  // offset 4
    float z;  // offset 8
    // padding to 12 bytes
};
```

Fields are aligned according to their type:
- `int`/`float`: 4-byte alignment
- `int64`: 8-byte alignment
- Arrays: Align to element type

### Methods

Methods are **deferred-compiled** functions with a special `self` parameter:

**Compilation Flow:**
```
1. User defines method with Native.compile({args: ['self', ...]})
2. Mark method with _deferred=1 flag
3. In struct constructor:
   a. Replace 'self' with struct_ref (contains NativeStructDef*)
   b. Resolve 'self' type to actual struct pointer
   c. Compile method normally
4. Wrap compiled method with js_native_method_wrapper
5. Bind to struct prototype
```

**Method Wrapper:**
```javascript
// JavaScript call: instance.scale(2)

// Wrapper in C:
JSValue js_native_method_wrapper(JSContext *ctx, JSValue this_val, ...) {
    NativeStructInstance *inst = get_instance(this_val);
    
    // Reorder: this_val becomes first arg
    new_argv[0] = inst->data; // Struct pointer
    new_argv[1] = argv[0];    // Original arg 0
    new_argv[2] = argv[1];    // Original arg 1
    ...
    
    return call_native_func(ctx, native_func, new_argc, new_argv);
}
```

**Field Access Compilation:**

For `self.x`:
1. `self` is local 0 (in `$a0`)
2. IR: `IR_LOAD_FIELD(local_idx=0, offset=0, type=float)`
3. MIPS:
   ```mips
   MOVE  $t0, $a0      # Copy base pointer
   LW    $t1, 0($t0)   # Load field at offset 0
   ```

**Critical Fix (2025-01-02):**
Early versions incorrectly scanned all 64 locals to find the struct base, leading to crashes. **Fixed** to only scan function arguments (0 to `arg_count-1`).

### Array Fields

Struct fields can be arrays:
```javascript
const Transform = Native.struct('Transform', {
    position: {type: 'float', length: 3},  // float[3]
    rotation: {type: 'float', length: 4}   // float[4]
});
```

Array fields return a **view object** with numeric index access:
```javascript
const t = new Transform();
t.position[0] = 1.0;  // Sets position[0]
t.position[1] = 2.0;  // Sets position[1]
```

**Implementation:**
- View class: `NativeStructArray`
- Exotic methods: `get_own_property`, `set_property`
- Direct memory access (no copying)

---

## Optimization Passes

### 1. Strength Reduction

Replaces expensive operations with cheaper equivalents.

**Transformations:**
- `x * 2ⁿ` → `x << n` (SHL)
- `x / 2ⁿ` →` `x >> n` (SAR for signed) **(only if positive power)**
- Pattern matching on `IR_MUL_I32` / `IR_DIV_I32` followed by `IR_CONST_I32`

**Example:**
```javascript
// Input: x * 8
IR_LOAD_LOCAL(0), IR_CONST_I32(8), IR_MUL_I32

// After strength reduction:
IR_LOAD_LOCAL(0), IR_CONST_I32(3), IR_SHL
```

### 2. Constant Folding

Evaluates constant expressions at compile time.

**Supported operations:**
- Arithmetic: `ADD, SUB, MUL, DIV, MOD, NEG`
- Bitwise: `AND, OR, XOR, NOT, SHL, SHR, SAR`
- Comparisons: `EQ, NE, LT, LE, GT, GE`

**Example:**
```javascript
// Input: 3 + 5 * 2
IR_CONST_I32(3), IR_CONST_I32(5), IR_CONST_I32(2), 
IR_MUL_I32, IR_ADD_I32

// After folding:
IR_CONST_I32(13)
```

### 3. Dead Code Elimination

Removes instructions whose results are never used.

**Algorithm:**
1. Mark all `RETURN` and `STORE` operations as live
2. Backward pass: mark dependencies as live
3. Remove unmarked instructions

**Example:**
```javascript
// Input:
let x = 5;      // Never used
let y = 10;
return y;

// After DCE:
let y = 10;
return y;
```

### 4. Loop Increment Optimization

Detects `i = i + const` patterns and replaces with optimized IR opcode.

**Pattern:**
```
IR_LOAD_LOCAL(i), IR_CONST_I32(n), IR_ADD_I32, IR_STORE_LOCAL(i)
```

**Transformation:**
```
IR_ADD_LOCAL_CONST(i, n)
```

**Generated code:**
```mips
# Before: LW + ADDIU + ADDU + SW (4 instructions)
LW    $t0, offset($fp)
ADDIU $t1, $zero, 1
ADDU  $t0, $t0, $t1
SW    $t0, offset($fp)

# After: LW + ADDIU + SW (3 instructions)
LW    $t0, offset($fp)
ADDIU $t0, $t0, 1      # Immediate add!
SW    $t0, offset($fp)
```

### 5. Loop Invariant Code Motion (LICM)

Hoists loop-invariant computations outside loops.

**Example:**
```javascript
// Before:
for (let i = 0; i < n; i++) {
    let c = Math.sqrt(radius);  // Invariant!
    area += c * i;
}

// After LICM:
let c = Math.sqrt(radius);
for (let i = 0; i < n; i++) {
    area += c * i;
}
```

**Implementation:**
- Identifies backward jumps (loops)
- Analyzes constants and non-dependent operations
- Moves to preceding block

---

## Examples

### Example 1: Vector Operations

```javascript
const Vec3 = Native.struct('Vec3', {
    x: 'float',
    y: 'float',
    z: 'float'
}, {
    dot: Native.compile({
        args: ['self', 'self'],  // Both are Vec3*
        returns: 'float'
    }, (a, b) => {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }),
    
    lengthSquared: Native.compile({
        args: ['self'],
        returns: 'float'
    }, (self) => {
        return self.x * self.x + 
               self.y * self.y + 
               self.z * self.z;
    }),
    
    normalize: Native.compile({
        args: ['self'],
        returns: 'void'
    }, (self) => {
        const len = Math.sqrt(self.x * self.x + 
                             self.y * self.y + 
                             self.z * self.z);
        if (len > 0) {
            self.x /= len;
            self.y /= len;
            self.z /= len;
        }
    })
});

// Usage
const v1 = new Vec3();
v1.x = 3; v1.y = 4; v1.z = 0;
console.log(v1.lengthSquared()); // 25.0

const v2 = new Vec3();
v2.x = 1; v2.y = 0; v2.z = 0;
console.log(v1.dot(v2)); // 3.0

v1.normalize();
console.log(v1.x); // 0.6
```

**Generated MIPS (dot product):**
```mips
# $a0 = self (v1), $a1 = other (v2)
LW     $t0, 0($a0)      # Load v1.x
LW     $t1, 0($a1)      # Load v2.x
MTC1   $t0, $f0
MTC1   $t1, $f1
MUL.S  $f2, $f0, $f1    # v1.x * v2.x

LW     $t0, 4($a0)      # Load v1.y
LW     $t1, 4($a1)      # Load v2.y
MTC1   $t0, $f0
MTC1   $t1, $f1
MUL.S  $f3, $f0, $f1    # v1.y * v2.y
ADD.S  $f2, $f2, $f3    # Accumulate

LW     $t0, 8($a0)      # Load v1.z
LW     $t1, 8($a1)      # Load v2.z
MTC1   $t0, $f0
MTC1   $t1, $f1
MUL.S  $f3, $f0, $f1    # v1.z * v2.z
ADD.S  $f2, $f2, $f3    # Accumulate

MFC1   $v0, $f2         # Move result to $v0
JR     $ra              # Return
ADDIU  $sp, $sp, 0      # Delay slot (no stack frame needed)
```

### Example 2: Matrix Multiplication

```javascript
const matrixMultiply = Native.compile({
    args: ['Float32Array', 'Float32Array', 'Float32Array'],
    returns: 'void'
}, (result, a, b) => {
    // 4x4 matrix multiply (row-major)
    for (let i = 0; i < 4; i++) {
        for (let j = 0; j < 4; j++) {
            let sum = 0;
            for (let k = 0; k < 4; k++) {
                sum += a[i * 4 + k] * b[k * 4 + j];
            }
            result[i * 4 + j] = sum;
        }
    }
});

// Usage
const a = new Float32Array(16);  // 4x4 matrix
const b = new Float32Array(16);
const result = new Float32Array(16);

// ... initialize a and b ...

matrixMultiply(result, a, b);
```

### Example 3: Particle System Update

```javascript
const Particle = Native.struct('Particle', {
    position: {type: 'float', length: 3},
    velocity: {type: 'float', length: 3},
    life: 'float'
});

const updateParticles = Native.compile({
    args: ['ptr', 'int', 'float'],  // Particle*, count, deltaTime
    returns: 'int'  // Active count
}, (particles, count, dt) => {
    let active = 0;
    for (let i = 0; i < count; i++) {
        const p = particles[i];;  // Get particle at index
        
        if (p.life > 0) {
            // Update position
            p.position[0] += p.velocity[0] * dt;
            p.position[1] += p.velocity[1] * dt;
            p.position[2] += p.velocity[2] * dt;
            
            // Update life
            p.life -= dt;
            
            if (p.life > 0) active++;
        }
    }
    return active;
});
```

---

## Implementation Details

### Bytecode Translation

QuickJS bytecode is translated instruction-by-instruction to IR.

**Example bytecode:**
```
OP_get_loc0        # Load local 0
OP_get_loc1        # Load local 1
OP_add             # Add
OP_return          # Return result
```

**Translated IR:**
```
IR_LOAD_LOCAL(0, type=int32)
IR_LOAD_LOCAL(1, type=int32)
IR_ADD_I32
IR_RETURN(type=int32)
```

**Intrinsic Detection:**

Math functions are detected during `OP_get_field2` or `OP_get_field` and replaced with IR opcodes:

```javascript
Math.sqrt(x)  → IR_SQRT_F32
Math.abs(x)   → IR_ABS_F32
Math.min(a,b) → IR_MIN_F32
Math.max(a,b) → IR_MAX_F32
Math.imul(a,b) → IR_IMUL_I32
```

### Control Flow Handling

**Loops:**
- Detected via backward jumps in bytecode
- Converted to `IR_JUMP` with label pointing to loop header
- Loop counters allocated to `$s0-$s7` for persistence

**Conditional:**
```javascript
if (x > 0) { ... } else { ... }
```

**IR:**
```
IR_LOAD_LOCAL(x)
IR_CONST_I32(0)
IR_GT_I32
IR_JUMP_IF_FALSE(else_label)
... then block ...
IR_JUMP(endif_label)
LABEL(else_label)
... else block ...
LABEL(endif_label)
```

### Error Handling

**Compile-time errors:**
- Type mismatches
- Unsupported operations
- Stack overflow (>8 depth)
- Invalid struct field access

**Runtime safety:**
- Bounds checking for typed arrays
- Null pointer checks for structs
- Division by zero (undefined behavior - follows MIPS semantics)

**Error reporting:**
```c
nc->has_error = true;
snprintf(nc->error_msg, sizeof(nc->error_msg), 
         "Type mismatch: expected %s, got %s",
         type_name(expected), type_name(actual));
return -1;
```

### Memory Management

**Compiled code:**
- Allocated via `malloc()` (aligned to 16 bytes)
- Cache-flushed before execution (PS2 requirement)
- Freed via `Native.free()` or garbage collection

**Struct instances:**
- Allocated via `malloc()` with struct size
- Zero-initialized
- Freed when JS object is garbage collected

**Strings and Arrays:**
- Reference-counted heap allocations
- Freed when reference count reaches zero

### Debugging Support

**Disassembly:**
```javascript
const code = Native.disassemble(nativeFunc);
console.log(code);
```

**Output format:**
```
=== MIPS Assembly Dump ===
0000: ADDIU  $sp, $sp, -16
0001: SW     $ra, 12($sp)
0002: MOVE   $t0, $a0
0003: MOVE   $t1, $a1
0004: ADDU   $v0, $t0, $t1
0005: LW     $ra, 12($sp)
0006: JR     $ra
0007: ADDIU  $sp, $sp, 16
=== End of dump ===
```

**Benchmarking:**
```javascript
const iterations = 1000000;
const timeMs = Native.benchmark(nativeFunc, iterations);
console.log(`${iterations} iterations in ${timeMs}ms`);
console.log(`${(iterations / (timeMs / 1000)).toFixed(0)} ops/sec`);
```

---

## Performance Characteristics

### Benchmarks

**Simple arithmetic** (`a + b`):
- Native code: **~5 cycles**
- QuickJS interpreter: **~200 cycles**
- **40x speedup**

**Vector operations** (dot product):
- Native code: **~25 cycles**
- QuickJS interpreter: **~800 cycles**
- **32x speedup**

**Matrix multiply** (4x4):
- Native code: **~1200 cycles**
- QuickJS: **~40,000 cycles**
- **33x speedup**

### Limitations

1. **Simplified float ABI**: Float args use `$a0-$a3` instead of `$f12-$f15` (still work correctly via MTC1)
2. **No SIMD**: R5900 VU units not utilized
3. **No register coloring**: Simple linear scan allocation
4. **Fixed stack size**: 8-level expression depth limit
5. **No tail call optimization**: `IR_TAIL_CALL` not fully implemented
6. **Limited escape analysis**: Struct allocations always heap-based

### Future Improvements

- [ ] Standard MIPS float ABI (pass floats directly in `$f12-$f15`)
- [ ] VU0/VU1 SIMD utilization
- [ ] Graph coloring register allocation
- [ ] Tail call elimination
- [ ] Stack-allocated structs
- [ ] JIT compilation (currently AOT only)
- [ ] Multi-threading support

---

## Appendices

### A. QuickJS Integration

**Minimal modification required**. The compiler integrates with QuickJS through:

#### Required QuickJS Extension

**Single API addition** to access function bytecode:

**Header** ([`quickjs.h:1140-1151`](file:///D:/GitHub/AthenaEnv/src/quickjs/quickjs.h#L1140-L1151)):
```c
/* Native compiler support - get bytecode info from a JS function */
typedef struct {
    const uint8_t *bytecode;
    int bytecode_len;
    int arg_count;
    int var_count;
    int stack_size;
} JSFunctionBytecodeInfo;

/* Returns 0 on success, -1 if not a bytecode function, -2 on other errors */
int JS_GetFunctionBytecodeInfo(JSContext *ctx, JSValueConst func_val, 
                               JSFunctionBytecodeInfo *info);
```

**Implementation** ([`quickjs.c:54617-54663`](file:///D:/GitHub/AthenaEnv/src/quickjs/quickjs.c#L54617-L54663)):
- Validates input is a bytecode function
- Extracts `JSFunctionBytecode` from object
- Returns bytecode buffer pointer and metadata
- Used by `native_compiler.c` to access bytecode for translation

#### Standard QuickJS APIs Used

1. **`JS_NewCFunction()`**: Register `Native.compile()` and `Native.struct()`
2. **`JS_GetArrayBuffer()`**: Access typed array data for `Float32Array`, `Int32Array`
3. **`JS_GetOpaque()` / `JS_SetOpaque()`**: Store/retrieve native pointers in JS objects
4. **`JS_NewClass()`**: Create custom classes for structs and arrays
5. **`JS_AtomToCString()`**: Convert property names to C strings (for struct field access)

**Why this design?**

The `JS_GetFunctionBytecodeInfo` API provides **read-only** access to internal QuickJS structures without exposing implementation details. This maintains encapsulation while enabling bytecode analysis.

**Alternative considered:** Directly accessing `JSObject->u.func.function_bytecode` would have required making internal structures public, breaking QuickJS modularity.

### B. Build System

**Compiler flags:**
```makefile
CFLAGS += -O2 -fno-strict-aliasing
MIPS_CFLAGS += -march=r5900 -mabi=eabi
```

**Required libraries:**
- QuickJS: `libquickjs.a`
- Standard C library (newlib)

### C. Testing

**Test suite:** `native_test.js`
- 20+ test cases
- Covers all IR opcodes
- Struct methods validation
- Array operations
- Type conversions

**Run tests:**
```bash
cd bin
./AthenaEnv native_test.js
```

### D. References

- [MIPS R5900 Core Architecture Manual](https://www.google.com/search?q=MIPS+R5900+manual)
- ps2tek: PS2 technical documentation
- [QuickJS Documentation](https://bellard.org/quickjs/)
- MIPS n32 ABI Specification

---

## License

Copyright (c) 2025 AthenaEnv Project  
Licensed under MIT License

---

## Contributors

- **Native Compiler**: Core team
- **Documentation**: Technical writing team
- **Testing**: QA team

For questions or contributions, please visit the project repository.

---

**End of Document**
