# Operating Systems — Laboratory Work No. 12
## Producer, Consumer and the Other Tools (Linux, pthreads)

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS, from the outside: Linux)

---

## 1. Goal

In Lab 11 I used one semaphore at a time: as a lock, as a signal, as a counter. In this lab the tools work together. I solved the producer and consumer problem with two semaphores and a mutex, I solved the order problem of Lab 11 again with a condition variable, I solved readers and writers with two semaphores, and I built a deadlock on purpose and then removed it.

Environment: Ubuntu (VirtualBox), folder `~/os-lab12`, `gcc` with `-Wall -pthread`.

Files of this lab: `prodcons.c`, `condvar.c`, `readers.c`, `deadlock.c` (the fixed version).

---

## 2. Part 1: producer and consumer

`prodcons.c`: a producer puts 8 items into a buffer of 4 slots, a slow consumer (it sleeps 0.3 s after every item) takes them out. Three tools work together:

- `empty` counts the free slots (starts at 4),
- `full` counts the filled slots (starts at 0),
- a mutex protects the buffer itself (`buf`, `in`, `out`, `count`).

```
$ gcc -Wall -pthread -o prodcons prodcons.c
$ ./prodcons
put 1 (in buffer: 1)
put 2 (in buffer: 2)
put 3 (in buffer: 3)
put 4 (in buffer: 4)
 take 1 (in buffer: 3)
put 5 (in buffer: 4)
 take 2 (in buffer: 3)
put 6 (in buffer: 4)
 take 3 (in buffer: 3)
put 7 (in buffer: 4)
 take 4 (in buffer: 3)
put 8 (in buffer: 4)
 take 5 (in buffer: 3)
 take 6 (in buffer: 2)
 take 7 (in buffer: 1)
 take 8 (in buffer: 0)
```

The producer is fast and fills the buffer with 4 items at once. After that, it can put a new item only when the slow consumer has taken one out, so `put` and `take` alternate. At the end the producer has finished and the consumer empties the last 4 items.

### Experiment: lock the mutex before `sem_wait(&empty)`

In `producer()` I swapped the first two lines, so that the mutex is locked before `sem_wait(&empty)`:

```
$ gcc -Wall -pthread -o prodcons prodcons.c
$ ./prodcons
put 1 (in buffer: 1)
put 2 (in buffer: 2)
put 3 (in buffer: 3)
put 4 (in buffer: 4)
^C
```

The program froze after `put 4` and I had to stop it with Ctrl-C. No `take` line was ever printed. After this I put the two lines back in the original order, and the output was the same as the first one above.

### Observe

**1. The producer is fast, yet "in buffer" never goes above 4. Which call stops it?**
`sem_wait(&empty)`. The semaphore `empty` counts the free slots and starts at 4. Every `put` takes one unit. After 4 items `empty` is 0, so the producer blocks in `sem_wait(&empty)` until the consumer takes an item and calls `sem_post(&empty)`. This is why "in buffer" never goes above 4.

**2. Who waits at the start: the consumer or the producer? Which call makes it wait?**
The consumer. `full` starts at 0 (no filled slot), so the consumer blocks in `sem_wait(&full)` until the producer has put the first item and called `sem_post(&full)`. (The consumer thread is even created first, so it reaches `sem_wait(&full)` before there is anything to take.)

**3. Lock the mutex BEFORE `sem_wait(&empty)`. What happens when the buffer is full, and why?**
The program hangs: it is a **deadlock**. When the buffer is full, the producer holds the mutex and goes to sleep in `sem_wait(&empty)`, waiting for a free slot. A free slot can appear only if the consumer takes an item, but to take an item the consumer needs the same mutex, and the producer is holding it. So:

- the producer waits for the consumer (to free a slot),
- the consumer waits for the producer (to release the mutex).

Nobody can continue. In my run the consumer did not even get the mutex once (no `take` line was printed), because the producer kept taking it again before the consumer could. The rule: **never sleep on a semaphore while holding a lock that the other side needs.** In the correct version the producer waits on `empty` *first*, with no lock in its hand, and takes the mutex only for the short moment when it touches the buffer.

---

## 3. Part 2: the condition variable

The problem is the same as `order.c` in Lab 11 (B must read only after A has written), but solved in another way. The state is a normal variable `ready`, protected by a mutex. The condition variable `changed` is only the place where B sleeps until A says "something changed". `pthread_cond_wait` unlocks the mutex, puts the thread to sleep, and takes the mutex again before it returns.

```
$ gcc -Wall -pthread -o condvar condvar.c
$ ./condvar
B: not ready, I sleep
A: message written
B: I read "hello"
```

B prints its first line at once, then nothing happens for one second (A sleeps), then A writes the message and B reads it.

### Experiment: `if` instead of `while`

I replaced `while (!ready)` with `if (!ready)`, compiled and ran it:

```
$ gcc -Wall -pthread -o condvar condvar.c
$ ./condvar
B: not ready, I sleep
A: message written
B: I read "hello"
```

The output is exactly the same. After that I put `while` back, and the output was again the same:

```
B: not ready, I sleep
A: message written
B: I read "hello"
```

### Observe

**1. In `order.c` (Lab 11) the semaphore itself remembered that the message was ready. Here, which variable remembers it?**
The plain variable `ready`. The condition variable `changed` remembers nothing: it is only a place to sleep. If A signals when nobody is waiting, the signal is lost. That is why the information "the message is ready" must live in a normal variable that is protected by the mutex.

**2. Replace `while (!ready)` with `if (!ready)`. Does it still work here?**
Yes, it works here (see the output above). There is exactly one waiter (B) and one signal (from A), so B wakes up only after A has set `ready = 1`, and the result is the same.

**3. Then why do we write `while`? Give one reason why a thread can wake up and still find `ready == 0`.**
Because waking up does not guarantee that the condition is true. Two reasons:

- **Spurious wakeup:** POSIX allows `pthread_cond_wait` to return without any signal.
- **Another thread was faster:** if several threads wait for the same condition, one of them can take the mutex first and use up the state (for example, take the only item). When the second thread gets the mutex back, the condition is false again.

In both cases the thread must check the condition again, so the check has to be in a loop. With `if` it would continue with a wrong state. It works in this lab only because there is one waiter and one signal.

---

## 4. Part 3: readers and writers

Many readers may read together; a writer must be alone. Two semaphores are used: `room` (1 = the room is free for a writer) and `rlock` (a lock around the counter `readers`). The first reader that comes in locks the room, the last one that leaves unlocks it. The program has 3 readers and 2 writers.

```
$ gcc -Wall -pthread -o readers readers.c
$ ./readers
reader 2 reads 0 (readers inside: 1)
reader 3 reads 0 (readers inside: 2)
reader 1 reads 0 (readers inside: 3)
writer 1 writes 10 (alone)
writer 2 writes 20 (alone)
```

My order is different from the one in the assignment (there writer 1 came first and the readers read 10). The scheduler decides which thread runs first, so the order can change from run to run. In my run the three readers got in first, so they read the old value 0, and after the last reader left, the two writers came one after the other. The important facts are the same as in the expected output: all three readers were inside at the same time ("readers inside: 3"), and each writer was alone.

### Observe

**1. Three readers are inside at the same time, but a writer is always alone. Which two lines of `reader()` make that possible?**

```c
if (++readers == 1)   /* the first reader in locks out the writers */
    sem_wait(&room);
```
```c
if (--readers == 0)   /* the last reader out lets the writers in */
    sem_post(&room);
```

The first reader takes `room` (so `room` becomes 0 and every writer must wait at `sem_wait(&room)`), and it keeps it for the whole group. The next readers see that `readers` is not 1, so they do not touch `room` and they walk in. The last reader out gives `room` back, and only now can a writer enter. A writer takes `room` for itself, so no reader and no other writer can be inside at the same time.

**2. Why does the counter `readers` need its own lock (`rlock`)? What could go wrong without it?**
`++readers` and `--readers` are not one step: they are load, add, store, the same race as `counter++` in Lab 11. Without `rlock`, two readers can change the counter at the same time and one change is lost. Then:

- two readers can both think "I am the first" (or none of them thinks it), so `room` is locked twice or not at all, and a writer can come in together with a reader;
- the counter can stay above 0 after all readers have left, so the last one never calls `sem_post(&room)` and the writers wait forever.

`rlock` also makes the test `++readers == 1` together with `sem_wait(&room)` one atomic action.

**3. Imagine readers that keep arriving without a pause. What happens to writer 2?**
Writer 2 waits forever. As long as a new reader comes in before the old ones have left, `readers` never goes down to 0, so `sem_post(&room)` is never called and the writer stays blocked at `sem_wait(&room)`. Nothing is locked in the sense of a deadlock, but the writer never gets its turn. This is **starvation**. (This solution gives priority to readers; a solution that gives priority to writers has the opposite problem for readers.)

---

## 5. Part 4: build a deadlock, then remove it

Two threads, two mutexes, taken in opposite order: `one()` takes A and then B, `two()` takes B and then A. Each thread has a `sleep(1)` between its two locks, so that each one surely holds its first mutex before the other asks for it.

### 5.1 The stuck run

First I ran it without `timeout`, and it froze (I stopped it with Ctrl-C):

```
$ gcc -Wall -pthread -o deadlock deadlock.c
$ ./deadlock
two: I have B, I want A
one: I have A, I want B
^C
```

Then with `timeout`, as the assignment asks:

```
$ timeout 5 ./deadlock; echo "exit code: $?"
one: I have A, I want B
two: I have B, I want A
exit code: 124
```

Both threads printed their "I want" line, then nothing happened for 5 seconds, and `timeout` had to kill the program (exit code 124). The order of the two lines is not important; it depends on which thread the scheduler starts first.

### 5.2 Without the `sleep(1)` lines

I saved the original as `deadlock_orig.c`, removed both `sleep(1)` lines and ran it ten times (`0` = finished, `124` = hung, killed by `timeout`):

```
$ for i in $(seq 10); do timeout 5 ./deadlock_orig > /dev/null; echo "run $i: $?"; done
run 1: 0
run 2: 0
run 3: 0
run 4: 124
run 5: 0
run 6: 0
run 7: 0
run 8: 0
run 9: 124
run 10: 0
```

| Result | Runs |
|---|---|
| finished (exit code 0) | 8 of 10 |
| hung (exit code 124) | 2 of 10 (runs 4 and 9) |

I ran the loop a second time and got the same pattern (hangs in runs 4 and 9). One more single run of this version, later, also hung:

```
$ timeout 5 ./deadlock_orig; echo "exit code: $?"
one: I have A, I want B
two: I have B, I want A
exit code: 124
```

### 5.3 The fix

In `two()` I locked A first and then B (the same order as `one()`), and I swapped the two unlocks to match. The `printf` texts are unchanged, so "two: I have B, I want A" is now only a leftover text (it is the same as in the expected output of the assignment).

```
$ gcc -Wall -pthread -o deadlock deadlock.c
$ timeout 5 ./deadlock; echo "exit code: $?"
one: I have A, I want B
one: I have both
two: I have B, I want A
two: I have both
done
exit code: 0
```

Both threads finish, `done` is printed and the exit code is 0.

### Observe

**1. In the stuck run, which thread holds which mutex, and which one is each thread waiting for? Draw the circle.**

- Thread `one` holds **A** and waits for **B**.
- Thread `two` holds **B** and waits for **A**.


The same as a cycle of waiting: `one` waits for B, B is held by `two`; `two` waits for A, A is held by `one`. Each thread waits for something that only the other thread can release, and the other thread cannot release it because it is also waiting. So both wait forever.

**2. Remove both `sleep(1)` lines from the ORIGINAL version and run it ten times. Does it still always hang? Why is that worse, not better?**
No. In my 10 runs it hung **2 times** (runs 4 and 9) and finished normally 8 times. Without the sleeps, `one` usually takes both mutexes and finishes before `two` has even started, so there is no problem. The deadlock happens only when the two threads interleave in the bad way: each takes its first mutex before the other takes its second one.

This is worse, not better. The bug is still in the program, but it is hidden: it shows up rarely and unpredictably, so tests can pass many times and the program still fails later (on another machine, under another load, in front of a user), and it is very hard to reproduce and to debug. The `sleep(1)` in the original makes the bad interleaving certain, and a bug that always happens is a bug that is easy to find.

**3. In one sentence: why can taking the locks in the same order never deadlock?**
If all threads take the locks in the same order, a thread that holds a later lock never waits for an earlier one, so a cycle of threads waiting for each other cannot form.

---

## 6. Conclusion

The three tools from Lab 11 now work together, and each problem needs a different combination:

| Problem | Tools | Where |
|---|---|---|
| Producer and consumer | `empty` (counter, starts at 4), `full` (counter, starts at 0), mutex for the buffer | `prodcons.c`: at most 4 items in the buffer, the consumer waits at the start |
| Order of events | mutex + condition variable + a normal variable `ready` | `condvar.c`: B sleeps until A has written |
| Readers and writers | `room` (1 = free for a writer), `rlock` (protects the counter `readers`) | `readers.c`: many readers together, a writer alone |
| Deadlock | two mutexes taken in opposite order | `deadlock.c`: fixed with the same lock order |

What I learned:

- In Part 1, the order of the calls matters: wait on the semaphore first, take the mutex after it. The opposite order (mutex first, then `sem_wait(&empty)`) made a deadlock, because the producer slept while it held the mutex that the consumer needed.
- In Part 2, the condition variable has no memory: the state is in the variable `ready`. The waiting must be in a `while` loop, because a thread can wake up and find the condition false (spurious wakeup, or another thread was faster). `if` worked in my run only because there was one waiter and one signal.
- In Part 3, the first reader in and the last reader out lock and unlock the room for the whole group, and the counter needs its own lock. The price is starvation: writers can wait forever if readers never stop coming.
- In Part 4, a deadlock needs a cycle of waiting. Taking the locks in the same order in every thread removes the cycle. A deadlock that appears only 2 times out of 10 is more dangerous than one that always appears.
