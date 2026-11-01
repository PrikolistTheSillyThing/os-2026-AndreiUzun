# Operating Systems — Laboratory Work No. 7
## Three Small Programs: Processes, Redirection, a Pipe

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS from the outside, Linux)

---

## 1. Goal

Run the answers to three questions from the lecture: how a process is born, where `printf` writes, and how `ls | wc -l` works. Four short C programs reproduce what the shell does: `run.c` (fork + exec + wait), `zombie.c` (a child that is not waited for), `redirect.c` (`ls -l > out.txt`) and `pipeline.c` (`ls /etc | wc -l`).

Environment: Ubuntu (VirtualBox), `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, working directory `~/os-lab7`. Every program was compiled with `gcc -Wall -Wextra`.

---

## 2. Part 1: fork + exec + wait

`run.c` copies itself (`fork`), the copy becomes another program (`execlp`), and the original waits for it (`waitpid`). The program has the three steps of the shell for the command `ls -l`.

### Run 1: `ls -l`

```
$ gcc -Wall -Wextra -o run run.c && ./run
parent: my PID is 7113
child: my PID is 7115, my parent is 7113
total 24
-rwxrwxr-x 1 vboxuser vboxuser 16392 Nov  1 14:38 run
-rw-rw-r-- 1 vboxuser vboxuser   723 Nov  1 14:38 run.c
parent: child 7115 ended with status 0
```

The order of the lines matches the expected output: parent PID, child PID, the listing, the parent's status line.

### Run 2: `date` (step 1.3)

The `execlp` line was replaced with `execlp("date", "date", (char *)NULL);`.

```
$ gcc -Wall -Wextra -o run run.c && ./run
parent: my PID is 7161
child: my PID is 7162, my parent is 7161
Sun Nov  1 02:40:06 PM UTC 2026
parent: child 7162 ended with status 0
```

### Run 3: a program that does not exist (step 1.4)

The line was replaced with `execlp("nosuchprog", "nosuchprog", (char *)NULL);`.

```
$ gcc -Wall -Wextra -o run run.c && ./run
parent: my PID is 7209
child: my PID is 7211, my parent is 7209
exec: No such file or directory
parent: child 7211 ended with status 127
```

After the experiments `run.c` was put back to the `ls -l` version, which is the one used as the base for Part 2.

### Observe

**(1) The line `perror("exec")` is in the child, right after `execlp`. Why is it printed only with `nosuchprog`?**
`execlp` replaces the whole program image of the child (code, stack, heap) with the new program. When it succeeds, it never returns, because the code that called it no longer exists in that process. So `perror("exec")` and `exit(127)` never run with `ls` or `date`. They run only when `execlp` fails and returns -1. That happens with `nosuchprog`: no executable with that name exists in any directory of `PATH`, the kernel leaves the child's original program untouched, and execution continues on the next line. `perror` prints the reason (`No such file or directory`) and `exit(127)` ends the child.

**(2) Which number does the child print as "my parent", and whose PID is it?**
In run 1 the child printed `my parent is 7113`. This is the PID of the parent process, the original `./run`, which printed `my PID is 7113` on its first line. `getppid()` in the child returns the PID of the process that called `fork()`.

**(3) What status does the parent see with `ls`, and what status with `nosuchprog`?**
With `ls` the status is **0**: `ls` finished successfully and exited normally. With `nosuchprog` the status is **127**: `exec` failed, so the child ran `exit(127)` (the shell convention for "command not found"). The parent receives the value through `waitpid`, and `WEXITSTATUS` extracts the exit code from `status`.

---

## 3. Part 2: A zombie, on purpose

`zombie.c` is a copy of `run.c` (the `ls -l` version) with one added line, `sleep(20);`, just before `waitpid`:

```c
    int status;
    sleep(20);                                   /* the new line */
    waitpid(pid, &status, 0);                    /* step 3: wait for the child */
```

The parent sleeps for 20 seconds before it waits, so the child has already ended but nobody has collected its exit status. The process table was checked 2 seconds after the start, and again after waiting 20 more seconds:

```
$ gcc -Wall -Wextra -o zombie zombie.c
$ ./zombie > /dev/null &
[3] 8683
$ sleep 2; ps -o pid,ppid,stat,cmd
    PID    PPID STAT CMD
   5888    5845 Ss   /usr/bin/bash
   8683    5888 S    ./zombie
   8685    8683 Z    [ls] <defunct>
   8691    5888 R+   ps -o pid,ppid,stat,cmd

$ sleep 20; ps -o pid,ppid,stat,cmd
[3]+  Done                       ./zombie > /dev/null
    PID    PPID STAT CMD
   5888    5845 Ss   /usr/bin/bash
   8694    5888 R+   ps -o pid,ppid,stat,cmd
```

(Notices of older background jobs printed by bash were left out.)

In the first listing, `./zombie` (8683) is sleeping (`S`) and its child `ls` (8685, PPID 8683) is in state `Z`. After 20 seconds both lines are gone, and `bash` reports the job as `Done`.

### Observe

**(1) Which letter does `ls` have in the STAT column, and what does it mean?**
`Z`, for zombie (it is shown as `[ls] <defunct>`). `ls` has already finished and exited, but its parent never called `wait`/`waitpid`, so the kernel keeps its entry in the process table. The entry holds the exit status until the parent collects it. The process no longer runs and uses no CPU or memory, but it still occupies a PID slot. Here the zombie was PID 8685 and its parent was PID 8683, the sleeping `./zombie`.

**(2) After 20 seconds, is the `[ls] <defunct>` line still there? Which line of `zombie.c` made it disappear?**
No, it is gone. The line that removed it is `waitpid(pid, &status, 0);`, which runs after `sleep(20)`. `waitpid` reads the exit status of the child, which lets the kernel delete the child's process-table entry (this is called "reaping"). Then the parent ends too, so `./zombie` also disappears from `ps`. The `sleep(20)` only delayed the reaping, and that delay is what made the zombie visible.

---

## 4. Part 3: Redirection, `ls -l > out.txt`

`redirect.c` does what the shell does for `ls -l > out.txt`. In the child it closes descriptor 1, opens `out.txt` (which gets the smallest free number), then runs `ls`:

```c
close(1);                                /* descriptor 1 is now free */
int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
...
fprintf(stderr, "open gave descriptor %d\n", fd);
execlp("ls", "ls", "-l", (char *)NULL);  /* ls writes to 1, as always */
```

```
$ gcc -Wall -Wextra -o redirect redirect.c && ./redirect
open gave descriptor 1
done: the listing is in out.txt
$ head -3 out.txt
total 68
-rw-r--r-- 1 vboxuser vboxuser     0 Nov  1 15:24 out.txt
-rwxrwxr-x 1 vboxuser vboxuser 16336 Nov  1 15:24 redirect
```

Only the two status lines appear on the screen; the listing is in the file. `out.txt` shows size 0 in its own listing because `ls` read the directory when the file had just been created and was still empty.

### Observe

**(1) Which number did `open` give, and why exactly that one?**
It gave **1**. `open` always returns the smallest descriptor number that is currently free. In the child, descriptors 0 (stdin), 1 (stdout) and 2 (stderr) start out open. `close(1)` frees descriptor 1, so it is the lowest free number, and `open("out.txt", ...)` takes it. This is how the file silently replaces standard output, with no special redirection call.

**(2) `ls` was not changed at all. Why does its listing not appear on the screen?**
`ls` just writes to descriptor 1 and does not know what that descriptor points to. In the child, descriptor 1 now refers to `out.txt`, not to the terminal. Open descriptors survive `exec`, because exec replaces the program but keeps the descriptor table of the process. So the data written by `ls` goes into the file. The parent's descriptor 1 was never touched, because the change happened only in the child after `fork`. That is why `done: the listing is in out.txt` still appears on the screen. `open gave descriptor 1` also appears on the screen because it was written to descriptor 2 (stderr), which still pointed to the terminal.

**(3) To redirect the errors instead (like `2> err.txt`), which descriptor would you close before `open`?**
Descriptor **2** (`close(2)`). Then `open` would return 2, the smallest free number, and everything the program writes to stderr would go into the file.

---

## 5. Part 4: A pipe, `ls /etc | wc -l`

`pipeline.c` builds the pipeline by hand: one `pipe`, two `fork`s. In the `ls` child, descriptor 1 becomes the writing end `p[1]`; in the `wc` child, descriptor 0 becomes the reading end `p[0]`. `dup` copies a descriptor into the smallest free number, exactly like `open`. Each child closes both original pipe ends after its `dup`, and the parent closes both ends and waits for the two children.

### Normal run, compared with the shell

```
$ gcc -Wall -Wextra -o pipeline pipeline.c && ./pipeline
237
$ ls /etc | wc -l
237
```

The two numbers are equal.

### Broken on purpose (step 4.3)

In the parent, the line `close(p[1]);` was turned into a comment:

```c
    close(p[0]);                                 /* the parent uses neither end */
    /* close(p[1]); */
```

```
$ gcc -Wall -Wextra -o pipeline pipeline.c && ./pipeline
^C
$ ls /etc | wc -l
237
```

The program did not end and had to be stopped with Ctrl-C. (The `237` after `^C` comes from the shell pipeline that was run next, not from `./pipeline`.)

### Observe

**(1) Did `./pipeline` and `ls /etc | wc -l` give the same number?**
Yes, both printed **237**. The hand-built pipeline does the same thing the shell does for `|`: one `pipe`, two `fork`s, `ls` with its descriptor 1 rewired to the write end `p[1]`, and `wc` with its descriptor 0 rewired to the read end `p[0]`.

**(2) After the change, which of the two children never ends, `ls` or `wc`?**
`wc` never ends. `ls` finishes normally, writes its listing into the pipe and exits.

**(3) Why, in one sentence: what is `wc` waiting for?**
`wc` is waiting for end-of-file on its input, and EOF on a pipe arrives only when every copy of the write end `p[1]` has been closed. The parent still holds its copy open, so `wc` keeps waiting for more data that never comes.

As a consequence, the parent is blocked in the second `wait(NULL)` waiting for `wc`, and `wc` is waiting for the parent to release `p[1]`: the two wait for each other. This is why every process must close all the pipe ends it does not use.

---

## 6. Conclusion

In this lab I ran, from the outside, the three things a shell does. The first program showed the birth of a process: `fork` makes a copy, `execlp` replaces the copy's program and never returns on success, and `waitpid` gives the parent the exit status (0 for `ls`, 127 when the program does not exist). The second program showed that a finished child stays in the process table as a zombie (`Z`) until the parent waits for it; `ps` made it visible while the parent slept, and `waitpid` removed it.

The last two programs showed that redirection and pipes are only descriptor rewiring done between `fork` and `exec`. `open` and `dup` both return the smallest free descriptor number, so closing 1 (or 0) first puts the file or the pipe end exactly where the program expects its output (or input), and open descriptors survive `exec`, so `ls` and `wc` need no changes at all. The deliberate mistake in `pipeline.c` showed the price of an unclosed descriptor: as long as one copy of the write end stays open, the reader never gets end-of-file and the pipeline hangs.
