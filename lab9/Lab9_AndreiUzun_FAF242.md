# Operating Systems — Laboratory Work No. 9
## Watching System Calls with strace

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS, from the outside, Linux)

---

## 1. Goal

In the lecture we crossed the door between a program and the kernel: the system call. In this lab I watched that door with `strace`, which prints every system call a program makes. First I read the calls of a real command (`ls -l`). Then I wrote a small C program that uses `open`, `write` and `read` directly, and watched its calls. Finally I counted how many times one single kind of call is made.

Environment: Ubuntu (VirtualBox), `strace` 6.19 (already installed, checked with `sudo apt install strace`), `gcc` for the C program.

---

## 2. Part 1: the calls of a real command

The command:

```
$ strace -f -e trace=openat,read,write,close ls -l
```

The beginning of the trace is the dynamic linker loading the shared libraries; each library is an `openat`, a `read` of 832 bytes (the ELF header) and a `close`:

```
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
close(3)                                = 0
openat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/libselinux.so.1", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832
close(3)                                = 0
...
openat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0 \250\2\0\0\0\0\0"..., 832) = 832
close(3)                                = 0
```

Then `ls` does its own work. It opens the current directory, and reads `/etc/passwd` and `/etc/group` to turn the numeric user and group IDs into names:

```
openat(AT_FDCWD, "/usr/share/coreutils/locales/ls/en-US.ftl", O_RDONLY|O_CLOEXEC) = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, ".", O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY) = 3
...
openat(AT_FDCWD, "/etc/passwd", O_RDONLY|O_CLOEXEC) = 4
read(4, "root:x:0:0:root:/root:/bin/bash\n"..., 4096) = 2823
close(4)                                = 0
...
openat(AT_FDCWD, "/etc/group", O_RDONLY|O_CLOEXEC) = 4
read(4, "root:x:0:\ndaemon:x:1:\nbin:x:2:\ns"..., 4096) = 1192
close(4)                                = 0
```

The last call is the one that prints the result:

```
write(1, "total 2040\ndrwxrwxr-x 2 vboxuser"..., 541total 2040
drwxrwxr-x 2 vboxuser vboxuser    4096 Nov  1 14:31 7
-rw-rw-r-- 1 vboxuser vboxuser    1174 Oct  4 12:54 LICENSE
...
) = 541
+++ exited with 0 +++
```

### How many system calls does one `ls -l` make?

To count, I ran `strace` without the filter and counted the lines (strace writes to stderr, so `2>&1 >/dev/null` sends strace's lines into the pipe and discards the output of `ls` itself):

```
$ strace -f ls -l 2>&1 >/dev/null | wc -l
288
```

So the trace of one simple `ls -l` is 288 lines long. This is only a rough measure of the number of calls, because the lines also include things that are not calls (like the final `+++ exited with 0 +++`). The exact count from `strace -c` in Part 3 is 270 calls. Most of them are not the "real work" of listing a directory: they are the setup of the process (loading the libraries, mapping memory) before `ls` does anything useful. Note that this number counts calls of all kinds together; counting one single kind is done in Part 3.

### Observe

**Which system call does `ls` use to print to the screen? To which descriptor does it write?**
`ls` uses `write`, and it writes to descriptor 1, which is the standard output (stdout): `write(1, "total 2040\n...", 541) = 541`. It prints the whole listing in a single call of 541 bytes, after all the reading is finished.

Other things visible in the trace:

- A failed call does not stop the program. `openat(... "ls/en-US.ftl") = -1 ENOENT` returned -1 with the error `ENOENT` (no such file), and `ls` just continued.
- The descriptor numbers 3 and 4 appear again and again. A `close` frees a number, and the next `openat` takes the lowest free one.
- The `uucore/...ftl` files and the scanning of `/usr/share/zoneinfo` show that this `ls` is the newer Rust implementation (uutils coreutils) shipped by this Ubuntu version, so the number of calls is larger than for the classic GNU `ls`.

---

## 3. Part 2: a program that calls the kernel directly

`syscalls.c` opens a file, writes a line, closes it, opens it again and reads the line back. The file itself is handled only with `open`, `write`, `read` and `close`; `printf` is used just to report what happened.

```c
/* syscalls.c - open, write, read and close, using only system calls.
 * Build: gcc -Wall -Wextra -o syscalls syscalls.c
 * Run:   ./syscalls
 * See the calls: strace ./syscalls
 */
#include <fcntl.h>   /* open */
#include <unistd.h>  /* write, read, close */
#include <string.h>  /* strlen */
#include <stdio.h>   /* printf */

int main(void) {
    const char *msg = "hello, kernel\n";

    /* open() is a system call: ask the kernel for a file, get back a descriptor */
    int fd = open("note.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }
    printf("open returned descriptor %d\n", fd);

    /* write() is a system call: hand the bytes to the kernel */
    ssize_t n = write(fd, msg, strlen(msg));
    printf("write sent %zd bytes\n", n);
    close(fd);

    /* read it back, through the kernel again */
    fd = open("note.txt", O_RDONLY);
    char buf[64];
    n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    printf("read got %zd bytes: %s", n, buf);
    close(fd);
    return 0;
}
```

It compiled without warnings with `-Wall -Wextra`:

```
$ gcc -Wall -Wextra -o syscalls syscalls.c
$ ./syscalls
open returned descriptor 3
write sent 14 bytes
read got 14 bytes: hello, kernel
```

The output is the same as the expected one: the first descriptor is 3, 14 bytes are written and 14 bytes are read back.

### The same program seen from outside

```
$ strace -e trace=openat,write,read,close ./syscalls
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
close(3)                                = 0
openat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0 \250\2\0\0\0\0\0"..., 832) = 832
close(3)                                = 0
openat(AT_FDCWD, "note.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
write(1, "open returned descriptor 3\n", 27open returned descriptor 3
) = 27
write(3, "hello, kernel\n", 14)         = 14
write(1, "write sent 14 bytes\n", 20write sent 14 bytes
)   = 20
close(3)                                = 0
openat(AT_FDCWD, "note.txt", O_RDONLY)  = 3
read(3, "hello, kernel\n", 63)          = 14
write(1, "read got 14 bytes: hello, kernel"..., 33read got 14 bytes: hello, kernel
) = 33
close(3)                                = 0
+++ exited with 0 +++
```

The first lines are only the setup (the dynamic linker loading `libc`), much shorter than for `ls` because this program needs only `libc`. My six calls are in the middle, mixed with three `write(1, ...)` lines. The program's own text (`open returned descriptor 3`, ...) appears inside the strace lines because both go to the same terminal (the program to stdout, `strace` to stderr).

How the C calls became system calls:

| C call in my program | System call seen by strace |
|---|---|
| `open("note.txt", O_CREAT \| O_WRONLY \| O_TRUNC, 0644)` | `openat(AT_FDCWD, "note.txt", O_WRONLY\|O_CREAT\|O_TRUNC, 0644) = 3` |
| `write(fd, msg, strlen(msg))` | `write(3, "hello, kernel\n", 14) = 14` |
| `close(fd)` | `close(3) = 0` |
| `open("note.txt", O_RDONLY)` | `openat(AT_FDCWD, "note.txt", O_RDONLY) = 3` |
| `read(fd, buf, sizeof(buf) - 1)` | `read(3, "hello, kernel\n", 63) = 14` |
| `close(fd)` | `close(3) = 0` |
| `printf(...)` (three times) | `write(1, "...", 27 / 20 / 33)` |

Observations:

- The C function `open` appears as the system call `openat`. The modern C library implements `open` with `openat`, and `AT_FDCWD` means "relative to the current directory". The flags are the same ones I passed, only printed in a different order.
- `printf` is not a system call itself: underneath it ends in `write(1, ...)`, descriptor 1 again, as in Part 1. Each `printf` line became one `write`, because stdout is a terminal and is line-buffered.
- `read` asked for 63 bytes (`sizeof(buf) - 1`) but returned 14, because the file contains only 14 bytes. The return value of `read` is how many bytes it really got.
- The first `openat` returns 3, `close(3)` frees it, and the second `openat` returns 3 again: the kernel always gives the lowest free descriptor number.

### Observe

**The descriptor returned by the first `openat` is 3, not 0, 1 or 2. Why are those three already taken?**
Descriptors 0, 1 and 2 are already open when the program starts: 0 is the standard input (stdin), 1 is the standard output (stdout) and 2 is the standard error (stderr). They are set up by the shell that starts the program and are inherited by the new process (this is also why `ls` wrote to descriptor 1 in Part 1, and why `printf` ends in `write(1, ...)` here). `open` returns the lowest descriptor number that is not in use, so the first free one is 3.

---

## 4. Part 3: counting one kind of call

In Part 1 I counted the lines of the whole trace, so all kinds of calls together. Here I count only one kind: `openat`. I filter the trace with `-e trace=openat`, so strace records only that call, and write it to a file with `-o` so it is not mixed with the output of `ls`:

```
$ strace -f -e trace=openat -o trace.txt ls -l > /dev/null
$ grep -c 'openat(' trace.txt
31
```

`ls -l` makes 31 calls of `openat`. To check the number I used the summary table of strace, which counts every kind of call separately:

```
$ strace -c -f ls -l > /dev/null
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ------------------
 23.21    0.004193         155        27           mmap
 17.98    0.003248         104        31         1 openat
 10.48    0.001893          94        20           read
  7.80    0.001408        1408         1           execve
  6.81    0.001230          36        34           close
  5.89    0.001064          31        34           getdents64
  ...
  0.78    0.000141         141         1           write
  ...
------ ----------- ----------- --------- --------- ------------------
100.00    0.018062          66       270        13 total
```

(Only the first rows and the `write` row are shown; the full table has 35 kinds of calls.) The `openat` row says 31 calls, the same as my `grep`. The 1 in the `errors` column is the `ENOENT` seen in Part 1 (the missing `ls/en-US.ftl` file); a failed call is still a call, so it is counted. In total `ls -l` makes 270 calls, so `openat` is about 11.5% of them (31 of 270). The most frequent kinds are `close` and `getdents64` (34 each), then `openat` (31) and `mmap` (27). The `write` row confirms Part 1: the whole listing is printed with a single `write`.

### Comparing two commands

I counted `openat` for the long and the short form of the command:

```
$ strace -f -e trace=openat -o t1.txt ls -l > /dev/null
$ strace -f -e trace=openat -o t2.txt ls > /dev/null
$ grep -c 'openat(' t1.txt t2.txt
t1.txt:31
t2.txt:11
```

Plain `ls` makes 11 `openat` calls and `ls -l` makes 31, so the option `-l` costs 20 more. Looking at the trace of Part 1, the 11 calls of plain `ls` are the setup: 6 for the shared libraries, `/proc/filesystems`, `/proc/self/maps`, the two `.ftl` message files, and the directory `.` itself. The extra 20 come only from `-l`: `/etc/nsswitch.conf`, `/etc/passwd` and `/etc/group` (to turn the user and group numbers into names), and 17 calls on `/usr/share/zoneinfo` (its subdirectories and `Etc/UTC`), used to print the dates in the local time zone.

### Observe

**Why is counting one kind of call better than counting all of them?**
The total (288 lines in Part 1, 270 calls in `strace -c`) only says that the command is busy; it does not say what it is doing. One kind of call answers a precise question: how many files does the command open? And comparing the same count for two commands (31 against 11) shows exactly what an option changes.

---

## 5. Conclusion

In this lab I watched the boundary between a program and the kernel with `strace`. For a real command (`ls -l`) I saw that about 270 system calls are made (288 lines of trace), most of them just loading libraries, and that the screen output is a single `write` to descriptor 1. With my own program I saw how C calls become system calls: `open` is implemented as `openat`, `write`, `read` and `close` keep their names, and `printf` is a `write(1, ...)` underneath.

The numbers returned by the kernel explain the behaviour of the program. Descriptors 0, 1 and 2 (stdin, stdout, stderr) are inherited from the shell, so the first file gets 3, and a closed number is reused by the next `open`. A failed call (like the `ENOENT` of `ls`) returns -1, and it is the program that decides what to do next.

Finally, I counted one kind of call instead of all of them: `ls -l` makes 31 `openat` calls (confirmed by `strace -c`), while plain `ls` makes 11. The difference of 20 is exactly the files that `-l` needs to open to print user names, group names and local dates.
