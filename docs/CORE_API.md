# Core API

`std`, `os`, `System`, `Archive`, `IOP`, `Mutex`, `Thread`, `Timer`.

P.S.: *Italic* parameters refer to optional parameters.

## std module

The std module provides wrappers to the libc `stdlib.h` and `stdio.h`, and a few other utilities.

**Available exports:**

* `std.evalScript(str, options = undefined)` - Evaluate the string `str` as a script (global eval). `options` is an optional object containing the following optional properties:
  * `std.backtrace_barrier` - Boolean (default = false). If true, error backtraces do not list the stack frames below the evalScript.
* `std.loadScript(filename)` - Evaluate the file `filename` as a script (global eval).
* `let hasfile = std.exists(filename)` - Returns a bool that determines whether the file exists or not.
* `let fstr = std.loadFile(filename)` - Load the file `filename` and return it as a string assuming UTF-8 encoding. Return null in case of I/O error.
* `let file = std.open(filename, flags, errorObj = undefined)` - Open a file (wrapper to the libc `fopen()`). Return the FILE object or null in case of I/O error. If `errorObj` is not undefined, set its `errno` property to the error code or to 0 if no error occurred.
* `std.fdopen(fd, flags, errorObj = undefined)` - Open a file from a file handle (wrapper to the libc `fdopen()`). Return the FILE object or null in case of I/O error. If `errorObj` is not undefined, set its `errno` property to the error code or to 0 if no error occurred.
* `std.tmpfile(errorObj = undefined)` - Open a temporary file. Return the FILE object or null in case of I/O error. If `errorObj` is not undefined, set its `errno` property to the error code or to 0 if no error occurred.
* `std.puts(str)` - Equivalent to `std.out.puts(str)`.
* `std.printf(fmt, ...args)` - Equivalent to `std.out.printf(fmt, ...args)`.
* `std.sprintf(fmt, ...args)` - Equivalent to the libc `sprintf()`.
* `std.in`, `std.out`, `std.err` - Wrappers to the libc file `stdin`, `stdout`, `stderr`.
* `std.SEEK_SET`, `std.SEEK_CUR`, `std.SEEK_END` - Constants for `seek()`.

**Enumeration object containing the integer value of common errors** (additional error codes may be defined):

`std.EINVAL`, `std.EIO`, `std.EACCES`, `std.EEXIST`, `std.ENOSPC`, `std.ENOSYS`, `std.EBUSY`, `std.ENOENT`, `std.EPERM`, `std.EPIPE`

* `std.strerror(errno)` - Return a string that describes the error `errno`.
* `std.gc()` - Manually invoke the cycle removal algorithm. The cycle removal algorithm is automatically started when needed, so this function is useful in case of specific memory constraints or for testing.
* `std.parseExtJSON(str)` - Parse `str` using a superset of `JSON.parse`. The following extensions are accepted:
  * Single line and multiline comments
  * Unquoted properties (ASCII-only JavaScript identifiers)
  * Trailing comma in array and object definitions
  * Single quoted strings
  * `\f` and `\v` are accepted as space characters
  * Leading plus in numbers
  * Octal (`0o` prefix) and hexadecimal (`0x` prefix) numbers
* `std.reload(str)` - Reloads the whole JavaScript environment (stack, modules, etc.) with the specified script.

### FILE prototype

**Construction:**
```js
let file = std.open("test.txt", "w");
```
* `filename` - Path to the file, e.g.: `"samples/test.txt"`.
* `flags` - File mode, e.g.: `"w"`, `"r"`, `"wb"`, `"rb"`, etc.

**Methods:**

* `close()` - Close the file. Return 0 if OK or -errno in case of I/O error.
* `puts(str)` - Outputs the string with UTF-8 encoding.
* `printf(fmt, ...args)` - Formatted printf. The same formats as the standard C library printf are supported. Integer format types (e.g. `%d`) truncate the Numbers or BigInts to 32 bits. Use the `l` modifier (e.g. `%ld`) to truncate to 64 bits.
* `flush()` - Flush the buffered file.
* `seek(offset, whence)` - Seek to a given file position (`whence` is `std.SEEK_*`). `offset` can be a number or a bigint. Return 0 if OK or -errno in case of I/O error.
* `tell()` - Return the current file position.
* `tello()` - Return the current file position as a bigint.
* `eof()` - Return true if end of file.
* `fileno()` - Return the associated OS handle.
* `error()` - Return true if there was an error.
* `clearerr()` - Clear the error indication.
* `read(buffer, position, length)` - Read `length` bytes from the file to the ArrayBuffer `buffer` at byte position `position` (wrapper to the libc `fread`).
* `write(buffer, position, length)` - Write `length` bytes to the file from the ArrayBuffer `buffer` at byte position `position` (wrapper to the libc `fwrite`).
* `getline()` - Return the next line from the file, assuming UTF-8 encoding, excluding the trailing line feed.
* `readAsString(max_size = undefined)` - Read `max_size` bytes from the file and return them as a string assuming UTF-8 encoding. If `max_size` is not present, the file is read up to its end.
* `getByte()` - Return the next byte from the file. Return -1 if the end of file is reached.
* `putByte(c)` - Write one byte to the file.

## os module

The os module provides Operating System specific functions:

* low level file access
* signals
* timers
* asynchronous I/O

The OS functions usually return 0 if OK or an OS-specific negative error code.

**Available exports:**

* `let fd = os.open(filename, flags, mode = 0o666)` - Open a file. Return a handle or < 0 if error.
  * Flags: `os.O_RDONLY`, `os.O_WRONLY`, `os.O_RDWR`, `os.O_APPEND`, `os.O_CREAT`, `os.O_EXCL`, `os.O_TRUNC` (POSIX open flags).
* `os.close(fd)` - Close the file handle `fd`.
* `os.seek(fd, offset, whence)` - Seek in the file. Use `std.SEEK_*` for `whence`. `offset` is either a number or a bigint. If `offset` is a bigint, a bigint is returned too.
* `os.read(fd, buffer, offset, length)` - Read `length` bytes from the file handle `fd` to the ArrayBuffer `buffer` at byte position `offset`. Return the number of read bytes or < 0 if error.
* `os.write(fd, buffer, offset, length)` - Write `length` bytes to the file handle `fd` from the ArrayBuffer `buffer` at byte position `offset`. Return the number of written bytes or < 0 if error.
* `os.remove(filename)` - Remove a file. Return 0 if OK or -errno.
* `os.rename(oldname, newname)` - Rename a file. Return 0 if OK or -errno.
* `os.realpath(path)` - Return `[str, err]` where `str` is the canonicalized absolute pathname of `path` and `err` the error code.
* `os.getcwd()` - Return `[str, err]` where `str` is the current working directory and `err` the error code.
* `os.chdir(path)` - Change the current directory. Return 0 if OK or -errno.
* `os.mkdir(path, mode = 0o777)` - Create a directory at `path`. Return 0 if OK or -errno.
* `os.stat(path)`, `os.lstat(path)` - Return `[obj, err]` where `obj` is an object containing the file status of `path`. `err` is the error code. The following fields are defined in `obj`: `dev`, `ino`, `mode`, `nlink`, `uid`, `gid`, `rdev`, `size`, `blocks`, `atime`, `mtime`, `ctime`. The times are specified in milliseconds since 1970. `lstat()` is the same as `stat()` except that it returns information about the link itself.
  * Constants to interpret the `mode` property returned by `stat()`, matching `sys/stat.h`: `os.S_IFMT`, `os.S_IFIFO`, `os.S_IFCHR`, `os.S_IFDIR`, `os.S_IFBLK`, `os.S_IFREG`, `os.S_IFSOCK`, `os.S_IFLNK`, `os.S_ISGID`, `os.S_ISUID`
* `os.utimes(path, atime, mtime)` - Change the access and modification times of the file `path`. The times are specified in milliseconds since 1970. Return 0 if OK or -errno.
* `os.readdir(path)` - Return `[array, err]` where `array` is an array of strings containing the filenames of the directory `path`. `err` is the error code.
* `os.setReadHandler(fd, func)` - Add a read handler to the file handle `fd`. `func` is called each time there is data pending for `fd`. A single read handler per file handle is supported. Use `func = null` to remove the handler.
* `os.setWriteHandler(fd, func)` - Add a write handler to the file handle `fd`. `func` is called each time data can be written to `fd`. A single write handler per file handle is supported. Use `func = null` to remove the handler.
* `os.sleep(delay_ms)` - Sleep during `delay_ms` milliseconds.
* `os.setTimeout(func, delay)` - Call the function `func` after `delay` ms. Return a handle to the timer.
* `os.setInterval(func, interval)` - Call the function `func` at specified intervals (in milliseconds). Return a handle to the timer.
* `os.setImmediate(func)` - Executes a given function immediately.
* `os.clearTimeout(handle)` - Cancel a timer.
* `os.clearInterval(handle)` - Cancel an interval.
* `os.clearImmediate(handle)` - Cancel an immediate execution.
* `os.platform` - Return a string representing the platform: `"ps2"`.

## System module

* `let ret = System.nativeCall(address, arguments, *return_type*)`
  * `address` - Memory address of the native function.
  * `arguments` - Array of arguments for the function. Argument model: `{type: System.T_X, value: X}`, where type is one of: `System.T_LONG`, `T_ULONG`, `T_INT`, `T_UINT`, `T_SHORT`, `T_USHORT`, `T_CHAR`, `T_UCHAR`, `T_PTR`, `T_BOOL`, `T_FLOAT`, `T_STRING`, `System.JS_BUFFER` (ArrayBuffer).
  * `return_type` - Use one of the types above, or simply omit for void.
* `let reloc_id = System.loadReloc(path)` - Loads a relocatable code (library or module).
* `System.unloadReloc(reloc_id)` - Unloads it from memory. DANGER!!! Make sure you aren't using code from it anymore.
* `let symbol_addr = System.findRelocObject("symbol_name")` - Find object inside the Athena binary or a relocatable code/data.
* `let symbol_addr = System.findRelocLocalObject(reloc_id, "symbol_name")` - Find object inside a relocatable code/data.
* `let info = System.getBDMInfo("mass0")` - Gets info about the BDM device, e.g. `{name: "usb", index: 0}`.
* `let result = System.mount(mountpoint, device, *mode*)`
* `let result = System.umount(path)`
* `System.devices()` - Array with available devices: `{name, desc}`.
* `let listdir = System.listDir(*path*)`
  * `listdir[index].name` - file name at the given index (string)
  * `listdir[index].size` - file size at the given index (integer)
  * `listdir[index].dir` - whether the given index is a file or a directory (bool)
* `System.removeDirectory(path)`
* `System.copyFile(source, dest)`
* `System.moveFile(source, dest)`
* `System.rename(source, dest)`
* `System.sleep(sec)`
* `System.exitToBrowser()`
* `System.setDarkMode(value)`
* `let temps = System.getTemperature()` — only works with SCPH-500XX and later models.
* `let info = System.getMCInfo(slot)` — `info.type`, `info.freemem`, `info.format`
* `let ee_info = System.getCPUInfo()` — `implementation`, `revision`, `FPUimplementation`, `FPUrevision`, `ICacheSize`, `DCacheSize`, `RAMSize`, `MachineSize`
* `let gs_info = System.getGPUInfo()` — `id`, `revision`
* `let ram_usage = System.getMemoryStats()`
  * `ram_usage.core` - Kernel + Native code size in RAM
  * `ram_usage.nativeStack` - Kernel + Native stack size
  * `ram_usage.allocs` - Dynamic allocated memory tracking
  * `ram_usage.used` - All above, but combined

**Asynchronous functions:**

* `System.threadCopyFile(source, dest)`
* `let progress = System.getFileProgress()` — `progress.current`, `progress.final`

## Archive module

* `let zip = Archive.open(fname)`
* `let list = Archive.list(zip)`
* `Archive.extractAll(zip)`
* `Archive.close(zip)`
* `Archive.untar(fname)`

## IOP module

* `const module = IOP.newModule(name, data, *options*)` - `data` can be a string when it is a file path or an ArrayBuffer when loading from memory. `options` is an optional object: `{ deps: [moduleId, ...], init: fn, end: fn }`.
* `IOP.loadModule(module, *args*)` - Loads an IOP module created by `newModule`. `args` is an optional string passed as the module's argument buffer.
* `IOP.reset()`
* `let stats = IOP.getMemoryStats()` — `stats.free`, `stats.used`

## Mutex module

**Construction:**
```js
const mutex = new Mutex();
```
**Methods:**
* `lock()` - Acquire the lock.
* `unlock()` - Release the lock.

## Thread module

**Functions:**
* `Thread.list()` - List all system threads (OS level, not JavaScript itself). `List[Object[id, name, status, stack_size]]`
* `Thread.kill(id)` - Force kill a thread from an internal ID.

**Construction:**
```js
const thread = new Thread(() => console.log("Hello from a thread!"), "Thread: Hello World!"); // Thread name is an optional parameter with 64 characters maximum size, useful to be tracked from Thread.list()
```
**Methods:**
* `start()` - Set the thread to an active state.
* `stop()` - Finish thread execution, changing it to the state before calling `start()`.

**Properties:**
* `id` - Thread internal ID
* `name` - Thread name

## Timer module

* `let timer = Timer.new()`
* `Timer.getTime(timer)` – Elapsed time in **microseconds** (divide by 1000 for ms).
* `Timer.setTime(src, value)` – Sets the elapsed time, in microseconds.
* `Timer.destroy(timer)`
* `Timer.pause(timer)`
* `Timer.resume(timer)`
* `Timer.reset(timer)`
* `Timer.isPlaying(timer)`
