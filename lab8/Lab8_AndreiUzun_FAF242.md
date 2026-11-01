# Operating Systems — Laboratory Work No. 8
## The Same Calls, Inside Your Own OS (xv6)

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** B (build your own small OS, xv6)

---

## 1. Goal

Take the calls from Lab 7 (`fork`, `exec`, `wait`) and run them inside xv6, the small OS built during the semester, to see that they are the same calls. Then make two processes talk through pipes. Two programs were written: `run.c` (fork + exec + wait, inside xv6) and `pingpong.c` (one byte sent back and forth through two pipes). The optional `primes.c` was not done.

Environment: Ubuntu (VirtualBox), xv6-riscv in `~/xv6-riscv`, started with `make qemu` (QEMU, RISC-V). Each program is a file `user/NAME.c` plus one line `$U/_NAME\` in the `UPROGS` list of the `Makefile`:

```
	$U/_run\
	$U/_pingpong\
```

---

## 2. Part 1: run, inside xv6

`run.c` has the same three steps as `run.c` in Lab 7, written for xv6: there is no glibc here, only the system calls of the kernel that was compiled. The child becomes `echo`.

```c
int pid = fork();                     // step 1: copy myself
...
if(pid == 0){
  char *args[] = { "echo", "hello", "from", "the", "child", 0 };
  exec("echo", args);                 // step 2: become echo
  printf("exec failed\n");            // runs only if exec failed
  exit(1);
}
int status;
wait(&status);                        // step 3: wait for the child
```

### Run 1: `echo` (step 1.2)

```
$ run
parent: my pid is 3
child: my pid is 4
hello from the child
parent: child 4 ended with status 0
```

### Run 2: `ls` (step 1.3)

The two lines were changed to `char *args[] = { "ls", 0 };` and `exec("ls", args);`.

```
$ run
parent: my pid is 3
child: my pid is 4
.              1 1 1024
..             1 1 1024
README         2 2 2441
cat            2 3 36728
echo           2 4 35592
sleep          2 5 35280
forktest       2 6 18136
grep           2 7 44184
init           2 8 35976
kill           2 9 35512
ln             2 10 35304
ls             2 11 42880
mkdir          2 12 35568
rm             2 13 35552
sh             2 14 58312
stressfs       2 15 36416
usertests      2 16 206800
grind          2 17 51824
wc             2 18 37728
mystat         2 19 36368
find           2 20 42568
zombie         2 21 34848
run            2 22 35888
logstress      2 23 37560
forphan        2 24 36352
dorphan        2 25 35800
sync           2 26 34944
console        3 27 0
parent: child 4 ended with status 0
```

The listing is the one of the root directory of the xv6 file system; it includes the programs written in earlier labs (`mystat`, `find`, `zombie`).

### Run 3: a program that does not exist (step 1.4)

The line was replaced with `exec("nosuchprog", args);`.

```
$ run
parent: my pid is 3
child: my pid is 4
exec failed
parent: child 4 ended with status 1
```

After the experiments `run.c` was put back to the `echo` version, which is the one submitted.

### Observe

**(1) The three calls that `run.c` uses in xv6 and the three it used on Linux in Lab 7. What is the same?**
In xv6 `run.c` uses `fork()`, `exec()` and `wait()`. On Linux (Lab 7) it used `fork()`, `execlp()` and `waitpid()`. The three jobs are the same: `fork` copies the process, `exec` replaces the program of the copy with another one and does not return on success, and `wait` gives the parent the exit status of the child. Even the shape of the code is the same (`pid == 0` is the child, the line after `exec` runs only if `exec` failed). The difference is the layer below. On Linux the calls go through glibc wrappers (`execlp` also searches the directories of `PATH`, and `waitpid` can choose a child). In xv6 they are the system calls of the kernel itself: `exec` takes the program name and the argument array directly, and looks the name up as a path in the file system (here the current directory is `/`, where `echo` and `ls` are).

**(2) Who prints "hello from the child": the code of `run`, or `echo`?**
`echo` prints it. After `exec("echo", args)` succeeds, the memory image of the child is replaced by the code of `echo`, so nothing after the `exec` line of `run.c` is executed in the child. The only things the child printed from `run.c` are the lines before the `exec` (`child: my pid is 4`). The PID stays the same across `exec`: it is still the same process, running a new program.

**(3) Why is the status 1 here and 127 on Linux?**
`wait` only reports the number the child passed to `exit`; the kernel does not choose it. In the xv6 `run.c` the line after the failed `exec` is `exit(1)`, so the parent sees 1. In the Lab 7 program it was `exit(127)`, the shell convention for "command not found", so the parent saw 127. The same failed `exec` gives different statuses only because each program picked a different number.

---

## 3. Part 2: pingpong

`pingpong.c` makes two processes talk through two pipes: `p2c` (parent to child) and `c2p` (child to parent). The parent sends one byte to the child. The child prints `<pid>: received ping`, writes the byte back and exits. The parent reads it, prints `<pid>: received pong` and waits for the child. Each process closes the pipe ends it does not use.

```c
int p2c[2], c2p[2];     // parent-to-child, child-to-parent
char b = 'x';

pipe(p2c);
pipe(c2p);
if(fork() == 0){
  close(p2c[1]); close(c2p[0]);       // the ends the child does not use
  read(p2c[0], &b, 1);
  printf("%d: received ping\n", getpid());
  write(c2p[1], &b, 1);
  exit(0);
}
close(p2c[0]); close(c2p[1]);         // the ends the parent does not use
write(p2c[1], &b, 1);
read(c2p[0], &b, 1);
printf("%d: received pong\n", getpid());
wait(0);
exit(0);
```

The byte `b` is initialized to `'x'`, so the parent does not send an undefined value.

```
$ pingpong
6: received ping
5: received pong
$
```

"ping" comes first, printed by the child (PID 6); "pong" comes second, printed by the parent (PID 5).

### Observe

**(1) Why are TWO pipes needed, and not one for both directions?**
A pipe is one-directional: what is written at the write end comes out at the read end. If both processes used one pipe, each of them could read the byte that was meant for the other. After the parent writes its ping, the parent itself might read the byte back before the child does, and then the child would block forever. The kernel does not mark whose data is whose, so nothing in a single shared pipe tells the two directions apart. With two pipes every direction has its own pipe (`p2c` and `c2p`), and every process closes the ends it does not use, so there is no ambiguity.

**(2) Nothing in the code says "wait until the child has replied". Which call makes the parent wait?**
The parent's `read(c2p[0], &b, 1)`. When the pipe is empty and a write end is still open, `read` blocks until the child writes a byte. This is what orders the output: the parent cannot print "pong" until the child has printed "ping" and written the byte back, so "ping" always comes first. The final `wait(0)` does not do this job; it only collects the exit status of the child after the conversation is over.

---

## 4. Conclusion

In this lab I ran the same three calls as in Lab 7, but inside xv6. `fork`, `exec` and `wait` behave exactly as on Linux: the copy is made, the child's program is replaced and does not return on success, and the parent receives the exit status. What changed is only the layer below: no glibc, the calls are the system calls of the kernel that I compiled myself. The two runs of `run` with a program that works (`echo`, `ls`) and one that does not exist showed that the exit status (1 here, 127 on Linux) is just the number the program chooses to pass to `exit`.

`pingpong` showed how two processes communicate through pipes. A pipe goes one way, so a conversation needs two of them, one per direction, and each process closes the ends it does not use. The blocking `read` on an empty pipe is what makes the parent wait for the child, with no explicit synchronization in the code. Both programs were built into the xv6 image through the `UPROGS` list and ran correctly in the emulator.
