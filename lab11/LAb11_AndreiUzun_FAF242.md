# Operating Systems — Laboratory Work No. 11
## The Race and the Semaphore (Linux, pthreads)

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS, from the outside: Linux)

---

## 1. Goal

Two threads that touch the same variable at the same time lose work. In this lab I saw it happen, then I fixed it with a mutex and with a semaphore, and I used the same semaphore in three ways: as a **lock** (start value 1), as a **signal** (start value 0) and as a **counter** (start value 3).

Environment: Ubuntu (VirtualBox), folder `~/os-lab11`, `gcc` with `-Wall -pthread`. I compiled **without** `-O2`, because the optimizer could keep `counter` in a register and hide the race of Part 1.

Files of this lab: `race.c`, `mutex.c`, `sem.c`, `order.c`, `parking.c`.

---

## 2. Part 1: see the race

`race.c`: two threads each run `counter++` one million times on the same global `long counter`, with no protection. The right answer is 2 000 000.

```
$ gcc -Wall -pthread -o race race.c
$ ./race
counter = 1458335 (expected 2000000)
$ for i in 1 2 3; do ./race; done
counter = 1535579 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 1644977 (expected 2000000)
$ time ./race
counter = 1351099 (expected 2000000)

real	0m0.030s
user	0m0.011s
sys	0m0.003s
```

In five runs, four gave a wrong number (1458335, 1535579, 1644977, 1351099) and one gave 2 000 000. The correct run is a lucky one: one million additions are so fast that thread `a` can almost finish before thread `b` starts, and then nothing collides. The program is still wrong, because nothing protects `counter`.

### Observe

**1. `counter++` looks like one step. What are the three steps the CPU really does?**
1. **Load:** read the value of `counter` from memory into a CPU register.
2. **Add:** add 1 to the value in the register.
3. **Store:** write the register back to `counter` in memory.

**2. How can two threads lose one addition?**
Suppose `counter = 5`.
- Thread A loads 5 into its register.
- The OS pauses A and runs B before A has written anything.
- Thread B loads 5, adds 1 and stores 6. Now `counter = 6`.
- The OS runs A again. A still has the old 5 in its register, adds 1 and stores 6. `counter = 6`.

Two additions were done, but the counter grew only by one. A's store overwrote the result of B, because A worked with an old value. One addition is lost.

**3. Why is the result different at every run?**
The OS scheduler decides when to pause a thread and run the other one. This depends on timing, on the load of the machine and on which cores are free, so it is not the same in two runs. Every run has a different number of moments where a thread works with an old value, so every run loses a different number of additions. In run 2 there were no such moments, so the result was correct by chance.

---

## 3. Part 2: fix it with a mutex, then with a semaphore

### 3.1 A mutex around `counter++` (`mutex.c`)

I added `pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;` and put `pthread_mutex_lock(&lock)` before `counter++` and `pthread_mutex_unlock(&lock)` after it. The line between them is the critical section: only one thread may be inside.

The first compile gave a warning, because I had forgotten `return NULL;` at the end of `add`:

```
mutex.c: In function ‘add’:
mutex.c:15:1: warning: control reaches end of non-void function [-Wreturn-type]
```

After adding the line:

```
$ gcc -Wall -pthread -o mutex mutex.c
$ ./mutex
counter = 2000000 (expected 2000000)
```

### 3.2 The same lock, made from a semaphore (`sem.c`)

A semaphore is a number that never goes below 0. `sem_wait` takes one unit (and waits if there is none), `sem_post` gives one back. Started at 1 (`sem_init(&s, 0, 1)`), it works like a mutex: `sem_wait` before `counter++`, `sem_post` after it.

My first compile failed because of a typo (`em_init` instead of `sem_init`):

```
sem.c:21:5: error: implicit declaration of function ‘em_init’; did you mean ‘sem_init’?
```

After the fix, eight runs in a row were correct:

```
$ for i in 1 2 3; do ./sem; done
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
$ for i in 1 2 3 4 5; do ./sem; done
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
```

### 3.3 The price of the lock

```
$ time ./race
counter = 1351099 (expected 2000000)
real	0m0.030s
user	0m0.011s
sys	0m0.003s

$ time ./mutex
counter = 2000000 (expected 2000000)
real	0m0.052s
user	0m0.038s
sys	0m0.023s

$ time ./sem
counter = 2000000 (expected 2000000)
real	0m0.076s
user	0m0.043s
sys	0m0.057s
```

| Program | real | user | sys | Result |
|---|---|---|---|---|
| `race` | 0.030 s | 0.011 s | 0.003 s | wrong |
| `mutex` | 0.052 s | 0.038 s | 0.023 s | 2 000 000 |
| `sem` | 0.076 s | 0.043 s | 0.057 s | 2 000 000 |

The order is the one expected: `race` is the fastest (and wrong), then `mutex`, then `sem`.

### Observe

**1. Run `mutex` and `sem` three times each. Is the answer always 2 000 000?**
Yes. Every run of `mutex` and `sem` that I did gave exactly 2 000 000 (`sem`: eight runs, all correct). Only one thread at a time can do load, add and store, so no addition is lost.

**2. What would happen in `sem.c` with `sem_init(&s, 0, 2)`? Try it and explain.**
The semaphore starts at 2, so two calls of `sem_wait` can succeed before it reaches 0. Both threads can then be inside the critical section at the same time, and the mutual exclusion is gone. The race of Part 1 comes back and the result can be below 2 000 000 (a single run can still be correct by luck, as in Part 1).

> **[FILL IN before handing in]** my result with start value 2: run `for i in 1 2 3 4 5; do ./sem; done` and paste the output here.

The start value is the number of threads allowed inside at the same time. Only the value 1 gives a lock.

**3. Why is the locked version slower?**
- Every `counter++` now also costs a lock and an unlock. They use atomic CPU instructions, which are much slower than a normal increment.
- The threads must take turns in the critical section, so there is no real parallel work there.
- When a thread finds the lock taken, it can go to sleep and be woken up by the kernel (system calls and context switches). My `sys` times show it: 0.003 s for `race`, 0.023 s for `mutex`, 0.057 s for `sem`.
- The lock variable moves between the caches of the CPU cores.

The semaphore is a little slower than the mutex because it does more work: it keeps a counter, not only "locked / unlocked".

---

## 4. Part 3: a semaphore as a signal

Now the problem is not two threads at once, but the **order**: B must read the message only after A has written it. In `order.c` the semaphore `ready` starts at 0, so it is a closed door: B waits at `sem_wait(&ready)` until A calls `sem_post(&ready)`. B is created first, on purpose, and A sleeps one second before it writes.

With start value 0:

```
$ gcc -Wall -pthread -o order order.c
$ ./order
B: waiting for the message...
A: message written
B: I read "hello"
```

B printed its first line at once, then nothing happened for one second, then A and B finished.

With start value 1 (`sem_init(&ready, 0, 1)`):

```
$ gcc -Wall -pthread -o order order.c
$ ./order
B: waiting for the message...
B: I read ""
A: message written
```

### Observe

**1. With start value 1, what does B read now, and why?**
B reads an **empty string**, and it prints it *before* `A: message written`. The semaphore starts at 1, so `sem_wait` succeeds at once and B does not wait for A. At that moment A is still sleeping, and the global array `message` still holds only zeros (an empty string). The order is no longer guaranteed.

**2. Put it back to 0. In one sentence: what does a semaphore that starts at 0 mean?**
Nothing is available yet, so whoever calls `sem_wait` must block until another thread calls `sem_post` to say that the event happened (here: the message is written).

---

## 5. Part 4: a semaphore as a counter, the parking lot

Six cars (threads), three places. The semaphore `places` starts at 3 and counts the free places. A car does `sem_wait(&places)` to take a place and `sem_post(&places)` to give it back. The mutex is there only so that `inside++` / `inside--` and the `printf` stay together.

With `PLACES 3`:

```
$ gcc -Wall -pthread -o parking parking.c
$ ./parking
car 1 enters (inside: 1)
car 2 enters (inside: 2)
car 5 enters (inside: 3)
car 5 leaves (inside: 2)
car 2 leaves (inside: 1)
car 1 leaves (inside: 0)
car 6 enters (inside: 1)
car 4 enters (inside: 2)
car 3 enters (inside: 3)
car 3 leaves (inside: 2)
car 4 leaves (inside: 1)
car 6 leaves (inside: 0)
```

A second run with `PLACES 3` gave another order of cars, with the same behavior:

```
car 1 enters (inside: 1)
car 3 enters (inside: 2)
car 2 enters (inside: 3)
car 2 leaves (inside: 2)
car 4 enters (inside: 3)
car 3 leaves (inside: 2)
car 1 leaves (inside: 1)
car 6 enters (inside: 2)
car 5 enters (inside: 3)
car 6 leaves (inside: 2)
car 4 leaves (inside: 1)
car 5 leaves (inside: 0)
```

With `PLACES 1`:

car 2 leaves (inside: 0)
car 5 enters (inside: 1)
```
car 1 enters (inside: 1)
car 1 leaves (inside: 0)
car 3 enters (inside: 1)
car 3 leaves (inside: 0)
car 4 enters (inside: 1)
car 4 leaves (inside: 0)
car 2 enters (inside: 1)
car 5 leaves (inside: 0)
car 6 enters (inside: 1)
car 6 leaves (inside: 0)
```

The order of the cars is different in every run (the scheduler decides which waiting thread wakes up first), but `inside` never goes above the number of places.

### Observe

**1. What is the largest value of `inside` you see? Which line of code guarantees it?**
The largest value is **3**. It is guaranteed by `sem_wait(&places);`: the semaphore starts at `PLACES` (3), and every car takes one unit before it enters. A fourth car sees the value 0 and blocks until a car leaves and calls `sem_post(&places)`. This is why cars 4, 5 and 6 enter only after other cars have left. The mutex does not limit the number of cars; it only keeps `inside++` and the `printf` together.

**2. Set `PLACES` to 1. What does the semaphore become?**
A **mutex** (a binary semaphore): only one car can be inside at a time, so the cars enter and leave one after another, as in my third run (`inside` is never above 1). It is the same role the semaphore had in `sem.c`.

---

## 6. Conclusion

One semaphore, three jobs. The only thing that changes is the **start value**:

| Start value | Role | Where I used it |
|---|---|---|
| 1 | lock (mutual exclusion) | `sem.c`: protects `counter++` |
| 0 | signal (order of events) | `order.c`: B waits until A has written |
| 3 (= PLACES) | counter of free resources | `parking.c`: at most 3 cars inside |

In Part 1 I saw that `counter++` is three steps (load, add, store), and that a thread switch between them loses an addition. The result is different at every run because the scheduler is. In Part 2 a mutex and a semaphore at 1 made the critical section safe (always 2 000 000), and the price was time: 0.030 s for `race`, 0.052 s for `mutex`, 0.076 s for `sem`, with more `sys` time for the locked versions. Part 3 showed that the start value is the whole meaning: at 0 B waits for A, at 1 B does not wait and reads an empty message. In Part 4 the start value 3 let three cars in at once, and with the start value 1 the same code became a lock.
