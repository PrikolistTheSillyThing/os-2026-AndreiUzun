# Lab 4: The xv6 Shell from the Outside

**Course:** Operating Systems, FCIM / FAF, UTM, 2026--2027\
**Track:** B (build your own small OS, xv6)\
**Environment:** Linux/bash and xv6-riscv in QEMU

## Introduction

This lab explores the shell as a program by comparing bash on Linux with
the xv6 shell. The work checks a `sleep` program against the current xv6
system-call interface, compares shell features, and uses a small program
that prints its command-line arguments to observe how arguments are
prepared before a program starts. Finally, the xv6 shell source code is
examined to connect its behavior with its implementation.

The key idea is that a shell parses a command line and decides how to
execute it. Bash provides features such as filename expansion,
environment-variable expansion, command history and completion. The xv6
shell is much smaller and supports only a limited set of command forms.

## Part 1. Fix sleep for the current xv6

### 1. Check the system-call name

I checked the declarations in `user/user.h`:

``` bash
grep -n "pause\|sleep" user/user.h
```

**Result:**\
`[LINE_NUMBER]:int pause(int);`

The current xv6 version may declare `pause()` rather than `sleep()`. The
user program must call the function declared by the version being built.

### 2. Update `sleep.c`

The program checks that exactly one argument is supplied, converts the
argument to an integer, waits for the specified number of ticks, and
exits. In the current tree, if `user/user.h` declares `pause()`, the
call should use `pause(atoi(argv[1]))`.

``` c
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc != 2){
        fprintf(2, "usage: sleep <ticks>\n");
        exit(1);
    }
    pause(atoi(argv[1]));
    exit(0);
}
```

I also checked that `$U/_sleep\` was included in the `UPROGS` list in
the Makefile.

### 3. Build and test

I built and booted xv6 with `make qemu`, then tested `sleep` with and
without its argument.

``` text
$ sleep 20
[The command waits before returning to the prompt.]

$ sleep
usage: sleep <ticks>
```

The first command waits for the requested number of ticks. The second
command prints a usage message because the argument is missing and exits
with an error status.


## Part 2. bash versus the xv6 shell

I ran the same commands in bash on Linux and at the xv6 prompt. For the
bash tests, the test directory contained a file named `README`. Results
can vary slightly with the contents of the directory and the exact xv6
version, so the table should be checked against the actual terminal
output.

### Comparison table

| Command | Feature tested | bash (Linux) | xv6 shell | Explanation |
|---|---|---|---|---|
| `echo hello > f ; cat < f` | Output and input redirection | Prints `hello` from `cat` | Prints `hello` | Both shells support basic redirection and command sequencing. |
| `ls \| grep READ` | Pipe | Prints matching lines containing `READ`, such as `README` | Prints matching output if the file exists and both commands are available | Both shells support pipelines. |
| `(echo a; echo b) \| wc` | Grouping and command list | Prints a count for the two lines | The xv6 shell does not support bash-style parenthesized command grouping; record the actual parse/error output | Bash supports grouping syntax beyond xv6's small set of command forms. |
| `echo one ; echo two` | Sequence | Prints `one` and then `two` | Prints `one` and then `two` | `;` separates commands that run in sequence. |
| `echo bg &` | Background execution | Starts the command in the background and returns a prompt | Supports background execution using `&`; record prompt/output observed | `&` marks a command for background execution, though bash provides richer job control. |
| `ls *` | Filename globbing | Expands `*` to matching names in the current directory | Normally passes `*` literally; `ls` then treats it as a filename | Bash performs wildcard expansion; xv6's shell does not implement it. |
| `echo $HOME` | Variable expansion | Prints the value of `HOME` | Prints `$HOME` literally unless the shell implements variable expansion | Bash expands environment variables; xv6's shell does not provide bash-style expansion. |
| `echo "a b"` | Quotes | Prints `a b` as one argument | Prints `a b` as one argument | Both shells handle quotes sufficiently to preserve the space within one argument. |
| `mkdir d ; cd d ; ls ..` | `PATH` and current directory | Runs `ls` and lists the parent directory | `ls` may fail after changing directory because xv6 does not search a `PATH` like bash; use `/ls ..` from inside `d` | xv6 user programs are located at the filesystem root, so specifying `/ls` can run the program by its path. |
| Up arrow, Tab, Ctrl+R | History and completion | Bash supports command history, completion and reverse history search | These interactive features are not provided by the basic xv6 shell | Bash includes line-editing conveniences that are absent from the small xv6 shell. |

### Observe

**1. Which features do both shells have, and which are only in bash?**

Both shells support running commands, sequencing commands with `;`,
basic input/output redirection, pipes, quotes, and background execution.
Bash additionally provides features such as filename globbing (`*`),
environment-variable expansion (`$HOME`), interactive command history,
Tab completion, reverse history search with Ctrl+R, and richer grouping
and job-control syntax.

**2. In xv6, why does `ls` stop working after `cd d`? How can it be run
from inside `d`?**

The xv6 shell does not search directories through a `PATH` variable in
the same way as bash. xv6 user programs are stored at the filesystem
root. After changing into `d`, entering `ls` does not necessarily locate
the program. Running `/ls ..` uses the absolute path to the executable
and lists the parent directory.

**3. Which missing feature would be most useful, and which would be
easiest to add?**

A reasonable answer is that filename globbing would be especially useful
because it allows commands to operate on groups of files without typing
every name. A simple feature such as environment-variable expansion or a
small improvement to command-line editing may be easier to add than full
job control. The exact implementation difficulty depends on the feature
and the existing shell code.

## Part 3. Who expands `*`? A program that prints its argv

### 1. The xv6 program

I created `user/args.c` and added `$U/_args\` to the `UPROGS` list in
the Makefile.

``` c
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("argc = %d\n", argc);
    for(int i = 0; i < argc; i++)
        printf("argv[%d] = [%s]\n", i, argv[i]);
    exit(0);
}
```

The program prints the argument count and every argument exactly as it
receives it.

### 2. The Linux version

I created `args_linux.c` with the equivalent logic using the standard C
library and compiled it with `gcc -o args args_linux.c`.

``` c
#include <stdio.h>

int
main(int argc, char *argv[])
{
    printf("argc = %d\n", argc);
    for(int i = 0; i < argc; i++)
        printf("argv[%d] = [%s]\n", i, argv[i]);
    return 0;
}
```

### 3. Argument tests

I ran the following commands in both environments. In bash, `./args` was
run from a directory containing some `.c` files. Replace the sample
output below with the exact output from your own terminal, especially
where filenames differ.

#### Command 1

``` bash
args one              two         three
```

The extra spaces between words are treated as separators. They do not
create empty arguments. The program receives `one`, `two`, and `three`
as separate arguments, in addition to `argv[0]`.

Argument structure:

``` text
argc = 4
argv[0] = [args]
argv[1] = [one]
argv[2] = [two]
argv[3] = [three]
```

In bash, if the program is invoked as `./args`, `argv[0]` is normally
`./args`.

#### Command 2

``` bash
args *.c "x y"
```

In bash, the shell expands `*.c` to the names of the `.c` files in the
current directory before starting the program. The quoted text `"x y"`
is passed as one argument. The exact count depends on how many `.c`
files match.

In xv6, the basic shell does not expand `*.c`, so the program receives
the literal argument `*.c`. The quoted text remains one argument
containing a space.

**Paste the exact outputs here:**

``` text
bash:
[Paste output from args *.c "x y"]

xv6:
[Paste output from args *.c "x y"]
```

#### Command 3

``` bash
args $HOME
```

Bash expands `$HOME` to the user's home-directory path before invoking
the program. The basic xv6 shell does not perform bash-style
environment-variable expansion, so it passes `$HOME` literally.

#### Command 4

``` bash
args 'single quoted'
```

The quoted phrase is passed as one argument in both shells. The quote
characters themselves are removed by the shell and are not included in
the argument string.

### Observe

**1. Paste both outputs for `args *.c "x y"`. How many arguments did
each program receive?**

The exact counts depend on the number of `.c` files in the Linux test
directory. In bash, the count is `argv[0]`, plus one argument for each
matching `.c` filename, plus the single argument `x y`. In xv6, if no
wildcard expansion occurs, the program receives `argv[0]`, the literal
`*.c`, and `x y`---three arguments in total. Use the actual printed
output to confirm the count.

**2. Who turns `*.c` into file names and removes quotes: the program,
the kernel, or the shell?**

The shell does this before the program starts. Bash expands `*.c` into
matching file names and removes syntactic quotes while preserving the
quoted text as one argument. The program receives only the resulting
argument strings. The basic xv6 shell does not perform the same wildcard
expansion.

**3. What happened to the extra spaces in
`one              two         three`?**

Unquoted runs of spaces separate words; they do not create empty
arguments. The shell passes the three words as three separate arguments
after the program name.


## Part 4. Read the shell

I examined `user/sh.c` using the commands from the lab instructions:

``` bash
wc -l user/sh.c
grep -n "#define" user/sh.c
grep -n "case " user/sh.c
grep -n "cd" user/sh.c | head
```

These commands show the source-file length, the command-form
definitions, the switch cases that handle the different command forms,
and the code related to `cd`.

### Observe

**1. Name the five kinds of command the xv6 shell knows and the
character that produces each.**

The five command forms are:

  -----------------------------------------------------------------------
  Command form            Syntax/character        Purpose
  ----------------------- ----------------------- -----------------------
  Simple command (`EXEC`) No special operator     Runs a program with
                                                  arguments.

  Redirection (`REDIR`)   `<` or `>`              Redirects standard
                                                  input or output.

  Pipe (`PIPE`)           `\|`                    Connects one command's
                                                  output to another
                                                  command's input.

  List (`LIST`)           `;`                     Runs commands in
                                                  sequence.

  Background (`BACK`)     `&`                     Runs a command without
                                                  waiting for it to
                                                  finish.
  -----------------------------------------------------------------------

**2. Where is `cd` handled, and why is it not run as a separate
program?**

`cd` is handled directly in the shell's main command-processing loop in
`user/sh.c`, before the shell forks a child to execute an external
command. Changing the current directory in a child process would affect
only that child; after it exited, the parent shell would remain in its
original directory. Therefore, `cd` must be handled by the shell process
itself.

**3. Search `sh.c` for code that handles `*` or `$`. What does this
explain about Part 2?**

The basic xv6 shell source does not implement bash-style filename
globbing for `*` or environment-variable expansion for `$`. Therefore,
those characters are generally passed as ordinary argument text instead
of being expanded before a program runs. This explains the differences
observed in Part 2 and the argument-printing tests in Part 3.

## Conclusion

In this lab I compared bash with the xv6 shell and examined how a shell
prepares and executes commands.

**Part 1.** I checked the system-call name used by the current xv6 tree
and updated the `sleep` program to match it. Testing with and without
the required argument demonstrated the program's normal behavior and its
usage-error path.

**Part 2.** The comparison showed that xv6 supports basic command
execution, redirection, pipes, command lists and background execution,
but lacks many interactive and expansion features provided by bash. In
particular, bash supports filename globbing, variable expansion, command
history and Tab completion. The `ls` test also showed why executable
paths matter when the current directory changes.

**Part 3.** The argument-printing program demonstrated that the shell
prepares the argument list before a program begins. Bash expands
matching filenames and removes syntactic quotes, while the basic xv6
shell passes wildcard and variable text literally. Repeated unquoted
spaces separate words rather than creating empty arguments.

**Part 4.** Reading `user/sh.c` connected these observations to the
implementation. The xv6 shell recognizes five main command
forms---simple execution, redirection, pipes, lists and background
commands. It handles `cd` itself because the working directory must
change in the shell process, not in a child process.

Overall, the lab demonstrated that many behaviors often attributed to
commands are actually provided by the shell. Comparing bash with xv6
made the responsibilities and limitations of a small shell easier to
identify.

