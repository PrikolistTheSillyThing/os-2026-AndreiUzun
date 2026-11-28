# Operating Systems — Laboratory Work No. 17
## Threads Share Memory, and Race (Linux, pthreads)

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS, from the outside: Linux)

---

## 1. Goal

The lecture said that threads share memory, which is their power and their danger. In this lab I saw both: two threads see the same variable at once, and when they both change it without protection, updates are lost. I first measured the race with two threads, then made it worse with four threads, and then thought about how to stop it.

Environment: Ubuntu (VirtualBox), folder `~/os-lab17`, `gcc` with `-Wall -Wextra -pthread`.

Files of this lab: `fire.c` (two threads), `fire2.c` (the same program with four threads).

---

## 2. Part 1: shared and racing

`fire.c`: two threads of one process add 1 to the same global variable `counter`, a million times each, **without a lock**. Before its loop, each thread prints the value of `counter` that it sees.

```
$ gcc -Wall -Wextra -pthread -o fire fire.c
$ ./fire
thread 2 sees counter = 0
thread 1 sees counter = 0
expected: 2000000
counter: 1442737 (lost updates: the race)
$ ./fire
thread 1 sees counter = 0
thread 2 sees counter = 91150
expected: 2000000
counter: 1135690 (lost updates: the race)
$ ./fire
thread 2 sees counter = 0
thread 1 sees counter = 112980
expected: 2000000
counter: 1328455 (lost updates: the race)
```

The program compiled without warnings. In all three runs the final value was below 2000000 and it was different every time:

| Run | Expected | Got | Lost updates |
|---|---|---|---|
| 1 | 2000000 | 1442737 | 557263 |
| 2 | 2000000 | 1135690 | 864310 |
| 3 | 2000000 | 1328455 | 671545 |

The order of the two "sees" lines is also different from run to run (in runs 1 and 3 thread 2 printed first). The scheduler decides which thread starts first, so this is the same kind of nondeterminism as the race itself. In run 1 both threads printed 0, so that run is not a good evidence for the first question; runs 2 and 3 are.

### Observe

**1. Thread 2 prints a non-zero counter at the start. What does that prove about the memory of the two threads?**
It proves that the two threads share the same memory. In run 2, thread 2 started later and saw `counter = 91150`: thread 1 had already changed the variable, and thread 2 saw the change at once. In run 3 it was the opposite: thread 1 was the later one and saw `counter = 112980`. `counter` is one variable in the address space of the process, not a private copy for each thread, so a write by one thread is immediately visible to the other. (After `fork()` it would be different: each process gets its own copy of the memory.)

**2. Why is the final counter less than 2000000, and different each run? Explain with the three steps of `counter++`.**
`counter++` is not one atomic operation. It is three steps:

1. **read** `counter` from memory into a register,
2. **add** 1 in the register,
3. **write** the register back to memory.

The system can switch between threads, or two cores can run them at the same time, between any two of these steps. For example, thread A reads 100, then thread B also reads 100; both add 1 and both write 101. Two increments were made, but the counter went up only by one, so one update is lost. How many such collisions happen depends on the exact timing of the threads, which is different in every run. That is why the result is random, and always below 2000000 (collisions can only lose updates, never add them).

---

## 3. Part 2: make it worse, then think

`fire2.c` is a copy of `fire.c` where `main` creates four threads in a loop and joins them, so the expected value is 4000000.

```
$ cp fire.c fire2.c
$ gcc -Wall -Wextra -pthread -o fire2 fire2.c
$ ./fire2
thread 1 sees counter = 0
thread 3 sees counter = 984200
thread 2 sees counter = 1078116
thread 4 sees counter = 1237571
expected: 4000000
counter: 2614959 (lost updates: the race)
$ ./fire2
thread 1 sees counter = 0
thread 2 sees counter = 140102
thread 3 sees counter = 183594
thread 4 sees counter = 232681
expected: 4000000
counter: 2430718 (lost updates: the race)
$ ./fire2
thread 1 sees counter = 0
thread 2 sees counter = 0
thread 3 sees counter = 0
thread 4 sees counter = 0
expected: 4000000
counter: 1350069 (lost updates: the race)
```

To compare 2 and 4 threads fairly I used the lost updates and the percentage of lost updates (lost ÷ expected):

| Threads | Run | Expected | Got | Lost | Lost % |
|---|---|---|---|---|---|
| 2 | 1 | 2000000 | 1442737 | 557263 | 27.9 % |
| 2 | 2 | 2000000 | 1135690 | 864310 | 43.2 % |
| 2 | 3 | 2000000 | 1328455 | 671545 | 33.6 % |
| 4 | 1 | 4000000 | 2614959 | 1385041 | 34.6 % |
| 4 | 2 | 4000000 | 2430718 | 1569282 | 39.2 % |
| 4 | 3 | 4000000 | 1350069 | 2649931 | 66.2 % |

Averages over three runs:

| Threads | Average lost updates | Average lost % |
|---|---|---|
| 2 | about 697 700 | about 35 % |
| 4 | about 1 868 000 | about 47 % |

**Does the loss get bigger?** Yes. In absolute numbers it is clearly bigger: my smallest loss with 4 threads (1385041) is bigger than my largest loss with 2 threads (864310). As a percentage, the average also goes up (from about 35 % to about 47 %), but the results are noisy and the ranges overlap (4-thread run 1 with 34.6 % is lower than 2-thread run 2 with 43.2 %). So the honest conclusion is "usually bigger", not "always bigger". With more threads there are more chances that two of them read the same old value and overwrite each other's write.

Run 3 with four threads is a good example: all four threads printed `counter = 0`, so all of them started before anybody had made an increment. They ran really in parallel from the beginning, collided all the time, and 66 % of the updates were lost.

### How to make `counter++` safe

The problem is that read, add and write can be interleaved with the steps of another thread. The fix is to make these three steps one indivisible block: only one thread at a time may be inside it, and the others wait at the entrance. This block is called a critical section, and the thing that guards its entrance is a lock.

### Observe

**In one sentence: what single idea would stop the lost updates?**
Mutual exclusion: make the read-add-write of `counter++` a critical section that only one thread can execute at a time (for example, guarded by a lock), so that no other thread can come in between its three steps.

---

## 4. Conclusion

| Part | What I did | What I saw |
|---|---|---|
| 1 | 2 threads, `counter++` a million times each, no lock | the result was always below 2000000 and different in every run (28 % to 43 % lost) |
| 2 | 4 threads, the same code | more updates lost (35 % on average with 2 threads, 47 % with 4 threads) |

What I learned:

- Threads of one process share the global memory: a value written by one thread is seen by the others at once (the non-zero "sees counter" lines).
- `counter++` is read, add, write, not one step. When two threads interleave between these steps, an update is lost, and the amount lost changes from run to run because it depends on the scheduler.
- More threads on the same variable means more collisions, so the loss grows (clearly in absolute numbers, and on average in percent).
- The idea that stops the loss is mutual exclusion: a critical section where only one thread at a time can do the read-add-write. I will build it in the next lab.
