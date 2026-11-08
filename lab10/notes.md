# Operating Systems — Laboratory Work No. 10
## Add Your Own System Call: `trace` (xv6)

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** B (build your own small OS, xv6)

---

## 1. Goal

In Lab 9 I watched system calls from the outside with `strace` on Linux. In this lab I changed the kernel itself for the first time: I added a new system call, `trace`, to xv6. After `trace` sets a mask on a process, the kernel prints every system call of that process whose bit is on in the mask, together with the pid, the name of the call and its return value.

I followed one number, 23, from a user program down to a function in the kernel.

Environment: Ubuntu (VirtualBox), the `~/xv6-riscv` tree, `riscv64-linux-gnu-gcc`, and `qemu-system-riscv64` (started with `make qemu`, 3 CPUs).

One difference from the MIT page on the internet: in our tree number 22 is already taken by `sync`, and the kernel print function is called `printk`, not `printf`. So the new call is number 23 and the kernel uses `printk`.

---

## 2. How one call travels

```
user/trace.c           trace(mask)
user/usys.S            li a7, SYS_trace (23);  ecall;  ret
        |  ecall: the CPU enters the kernel
kernel/syscall.c       syscall() reads a7 = 23, runs syscalls[23]
kernel/sysproc.c       sys_trace(): reads a0 with argint(0, ...), stores it in the process
kernel/proc.h          p->trace_mask
```

After that, every later system call of the process goes through `syscall()`, which compares the call number with the mask and prints a line if the bit is on.

---

## 3. Part 1: the number and the user side

Five small edits, so that a user program can ask for the call.

| File | Change |
|---|---|
| `kernel/syscall.h` | `#define SYS_trace 23` after `SYS_sync 22` |
| `user/user.h` | `int trace(int);` after `int sync(void);` |
| `user/usys.pl` | `entry("trace");` at the end. At build time this script writes `user/usys.S`, where `trace` becomes `li a7, SYS_trace; ecall; ret` |
| `user/trace.c` | new program (below) |
| `Makefile` | `$U/_trace\` in the `UPROGS` list, after `$U/_zombie\`, starting with a TAB |

`user/trace.c` sets the mask on itself and then becomes the command with `exec`. The mask stays, because `exec` keeps the process:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "usage: trace mask command [args...]\n");
    exit(1);
  }
  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "trace: failed\n");
    exit(1);
  }
  exec(argv[2], argv + 2);
  fprintf(2, "trace: exec %s failed\n", argv[2]);
  exit(1);
}
```

I made the edits with `sed`, `echo` and `cat` commands, and checked each one with `grep`. For example, the Makefile check with `cat -A` showed the TAB (`^I`) and the backslash as the last character:

```
151-^I$U/_zombie\$
152:^I$U/_trace\$
153-^I$U/_run\$
```

### Check 1: the number reaches the kernel

The kernel has no function for 23 yet, so it has to complain:

```
$ trace 32 grep hello README
3 trace: unknown sys call 23
trace: failed
```

This is the expected result and it is good news: the program ran `ecall` with `a7 = 23` and the kernel received it. The build also picked up `_trace` in the `mkfs` line.

---

## 4. Part 2: the kernel side

Five more edits.

**2.1 `kernel/proc.h`**: a place for the mask, in `struct proc`, after the `name` field:

```c
  char name[16];               // Process name (debugging)
  int trace_mask;
```

**2.2 `kernel/sysproc.c`**: the handler, at the end of the file. It reads argument 0 (register `a0`) and stores it on the process:

```c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}
```

**2.3 `kernel/syscall.c`**: the table. One line to tell the file the function exists, one to give it place 23:

```c
extern uint64 sys_trace(void);
...
  [SYS_sync]    = sys_sync,
  [SYS_trace]   = sys_trace,
```

**2.4 `kernel/syscall.c`**: the names and the print. The kernel only knows numbers, so I added a table of names above the function `syscall()`:

```c
static char *syscallnames[] = {
  [SYS_fork] = "fork", [SYS_exit] = "exit", [SYS_wait] = "wait",
  [SYS_pipe] = "pipe", [SYS_read] = "read", [SYS_kill] = "kill",
  [SYS_exec] = "exec", [SYS_fstat] = "fstat", [SYS_chdir] = "chdir",
  [SYS_dup] = "dup", [SYS_getpid] = "getpid", [SYS_sbrk] = "sbrk",
  [SYS_pause] = "pause", [SYS_uptime] = "uptime", [SYS_open] = "open",
  [SYS_write] = "write", [SYS_mknod] = "mknod", [SYS_unlink] = "unlink",
  [SYS_link] = "link", [SYS_mkdir] = "mkdir", [SYS_close] = "close",
  [SYS_sync] = "sync", [SYS_trace] = "trace",
};
```

I wrote `[SYS_fork] = "fork"` with the `=` sign. The sheet has no `=`, which is old GNU syntax that some compilers warn about. Then, inside `syscall()`, right after the line that runs the call, I added the print. It is placed after the call, so the return value in `a0` is already there:

```c
    p->trapframe->a0 = syscalls[num]();
    if ((p->trace_mask >> num) & 1)
      printk("%d: syscall %s -> %ld\n", p->pid, syscallnames[num], p->trapframe->a0);
```

**2.5 `kernel/proc.c`**: children inherit the mask. In `fork()`, right after the line that copies the name:

```c
  safestrcpy(np->name, p->name, sizeof(p->name));
  np->trace_mask = p->trace_mask;
```

**Extra (not in the sheet):** in `allocproc()` I also reset the mask, so that a reused process slot cannot keep an old mask from a previous process:

```c
  p->pid = allocpid();
  p->trace_mask = 0;
  p->state = USED;
```

### Check 2: `trace` works

```
trace 32 grep hello README
8: syscall read -> 1023
8: syscall read -> 965
8: syscall read -> 453
8: syscall read -> 0
```

The numbers are the expected ones: 1023 + 965 + 453 = 2441, the size of `README`, read in three pieces, then 0 for the end of the file. The pid is 8 instead of 3; the sheet says it can differ.

---

## 5. Part 3: playing with the mask

The mask is a number read in binary: bit N on means "print call number N". `32 = 2^5`, and 5 is `SYS_read`.

### 3.1 All the calls (all bits on)

```
$ trace 2147483647 grep hello README
12: syscall trace -> 0
12: syscall exec -> 3
12: syscall open -> 3
12: syscall read -> 1023
12: syscall read -> 965
12: syscall read -> 453
12: syscall read -> 0
12: syscall close -> 0
```

It is the same as the sheet. The first line is `trace` itself, then `exec` (the program becomes `grep`; the `3` is the number of arguments it was started with), `open` returns descriptor 3 (0, 1, 2 are taken, like in Lab 9), four `read` calls, and a final `close`.

### 3.2 Only `fork` (bit 1, mask 2)

```
$ trace 2 usertests forkforkfork
usertests starting
test forkforkfork: 14: syscall fork -> 15
15: syscall fork -> 16
16: syscall fork -> 17
16: syscall fork -> 18
16: syscall fork -> 19
...
17: syscall fork -> 75
17: syscall fork -> -1
16: syscall fork -> -1
18: syscall fork -> -1
OK
ALL TESTS PASSED
```

(The middle of the output, about sixty lines of the same kind, is shortened with `...`.)

My output differs from the sheet in the details: the sheet shows pids 6, 7, 8 and one final `-1`, and I have pids 14, 15, 16, 17, 18 and 68 and three `-1` lines. The pid numbers depend on how many processes ran before, and the order of the lines is a scheduling race between the 3 CPUs. What matters is the same: several different pids print `fork` lines, the test ends with `OK` and `ALL TESTS PASSED`, and the `-1` lines are `fork` failing when the process table is full.

### 3.3 Only `write` (bit 16, mask 65536)

```
$ trace 65536 echo hi
hi13: syscall write -> 2

13: syscall write -> 1
```

### 3.4 Only `open` and `close` (my answer to question 4)

```
$ trace 2129920 grep hello README
77: syscall open -> 3
77: syscall close -> 0
```

Only the two wanted calls are printed.

---

## 6. Observe

**1. In `trace 32 grep hello README` why do you see only `read`, and not `open` or `close`?**
Mask 32 = 2^5, so only bit 5 is on, and 5 is `SYS_read`. The kernel prints a call only if `(mask >> num) & 1` is true. For `open` (15) and `close` (21) that bit is 0, so `grep` still calls them, but they are not traced.

**2. In `forkforkfork`, three different pids print lines. Which line of code from Part 2 makes that possible?**
The line `np->trace_mask = p->trace_mask;` in `fork()` in `kernel/proc.c` (step 2.5). `trace` sets the mask on itself and then `exec` keeps it for `usertests`. Every `fork` copies the mask of the parent into the child, so the children (and their children) are traced too. Without this line only the first process would print, because the children would start with mask 0.

**3. In `trace 65536 echo hi` the trace line is printed between "hi" and the new line. Why? (How many times does `echo` call `write`?)**
`echo` calls `write` twice: once for `hi` (2 bytes, returns 2) and once for the newline (1 byte, returns 1). The kernel prints the trace line right after each call returns, so the first trace line appears directly after `hi` and before the `\n` is written. That is also where the blank line in the output comes from.

**4. Which mask traces both `open` (15) and `close` (21), and nothing else?**
`2^15 + 2^21 = 32768 + 2097152 = 2129920`. The check in 3.4 prints only the `open` and `close` lines.

---

## 7. Conclusion

In this lab I added a system call to the kernel. The path is the same one I saw from the outside in Lab 9, now seen from the inside: the stub in `usys.S` puts 23 in `a7` and runs `ecall`; the kernel reads the number in `syscall()`, finds `sys_trace` in the table, and the handler reads its argument from `a0`. The mask lives in `struct proc`, so each process has its own, and `fork` copies it, which is why a traced program and all of its children are traced.

The print is placed after the call, so the return value is already in `a0`. That explains the order in `echo hi`: each trace line comes right after the `write` it describes. The mask is a set of bits (bit N is call N), which is why `32` means `read`, `2` means `fork`, and a sum of powers of two (`2129920`) selects several calls at once.
