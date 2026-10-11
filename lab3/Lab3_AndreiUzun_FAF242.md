# Lab 3: The Command Line: Shell, Pipes and Text Filters

**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS, from the outside, Linux)
**Environment:** Ubuntu 26.04.1 LTS (VirtualBox), bash

## Introduction

This lab is about working fast in the Linux command line. The goals are to move around bash efficiently, predict what the shell does to a command line before it runs (globbing, quoting, expansion), redirect input and output, and answer questions about data by joining small filters (`grep`, `cut`, `sort`, `uniq`, `wc`) with pipes.

The key idea is that the shell is only a program that rewrites your line and starts other programs. Most of the "magic" (wildcards, variables, `$(...)`) happens in the shell *before* the program ever runs, and small tools connected by pipes can answer real questions about data.

For the later parts I generated an artificial web access log of 500 lines:
 write 
```bash
mkdir -p ~/os-lab3 && cd ~/os-lab3
for i in $(seq 1 500); do
echo "2026-10-05 10:$(printf %02d $((i % 60))) user$((i % 7)) $([ $((i % 5)) -eq 0 ] && echo FAIL || echo OK) /page$((i % 4))"
done > access.log
head -3 access.log ; wc -l access.log
```

```
2026-10-05 10:01 user1 OK /page1
2026-10-05 10:02 user2 OK /page2
2026-10-05 10:03 user3 OK /page3
500 access.log
```

The last line prints `500 access.log`, so the data set is correct.

## Part 1. Move fast, find help

### 1. Tab completion

I typed `ls /usr/sh` and pressed Tab twice. The first Tab completed the name to `/usr/share/`, and the second Tab, with several possible continuations, made bash offer to list them (`Display all 253 possibilities? (y or n)`). Then I typed `cd /et` and pressed Tab once: it completed immediately to `cd /etc/`, because `/etc` is the only match.

So Tab completes a name when it is unique, and when it is ambiguous it lists the candidates on the second press.

<img width="480" height="60" alt="image" src="https://github.com/user-attachments/assets/e52735ca-b63d-41de-b7a1-cfc4380cde0f" />



### 2. Command history search (Ctrl+R)

After running `pwd`, `whoami` and `date`, I pressed Ctrl+R and typed `pw`. The prompt changed to a reverse incremental search:

```
(reverse-i-search)`pw': pwd
```

Ctrl+R searches backwards through the command history while I type, and it matched the most recent command containing those letters. Enter then ran `pwd` again.

<img width="480" height="133" alt="image" src="https://github.com/user-attachments/assets/9d237959-4cf7-4504-b173-7b3599a1de56" />


### 3. Built-in or external, and where the shell looks

```bash
type cd ; type ls ; type echo ; which ls
```

```
cd is a shell builtin
ls is aliased to `ls --color=auto'
echo is a shell builtin
/usr/bin/ls
```

```bash
echo $PATH | tr ':' '\n'
```

```
/usr/local/sbin
/usr/local/bin
/usr/sbin
/usr/bin
/sbin
/bin
/usr/games
/usr/local/games
/snap/bin
/snap/bin
```

```bash
nosuchprog ; echo "exit code: $?"
```

```
nosuchprog: command not found
exit code: 127
```
<img width="552" height="368" alt="Screenshot From 2026-10-11 13-38-38" src="https://github.com/user-attachments/assets/0e4aad53-0b5c-467e-94e5-9ec92950c36d" />



### 4. Finding help without a browser

```bash
man ls                  # /sort, n for next, q to quit
ls --help | head
man -k "list directory"
help cd
```

`man -k "list directory"` found `ls`, `dir`, `vdir`, `gnuls` and others, and `help cd` showed the built-in's usage and options (`-L`, `-P`, `-e`, `-@`).

One difference from the handout: on this Ubuntu release `ls` is the Rust rewrite of coreutils (`rust-ls`), so `ls --help` is shorter than the classic GNU version, and the man page appears both as `ls` and `gnuls`. The options used in this lab behave the same.

<img width="552" height="223" alt="image" src="https://github.com/user-attachments/assets/a022b1ef-0d4c-4d6e-955d-740cc0d3eacb" />


### Observe

**1. Which of `cd`, `ls`, `echo` are built-ins, and why MUST `cd` be one?**

`cd` and `echo` are built-ins. `ls` is an external program (`/usr/bin/ls`) that is also aliased to `ls --color=auto`. `cd` must be a built-in because the working directory belongs to the process: if `cd` were an external program, the shell would start it as a child process, the child would change *its own* directory and exit, and the shell's directory would stay unchanged.

**2. How many directories are in PATH, and in which one is `ls`?**

PATH has 10 entries, but `/snap/bin` appears twice, so there are 9 distinct directories. `ls` is in `/usr/bin`.

**3. What exit code does a command that is not found return?**

127.

**4. Which option of `ls` sorts by modification time?**

`-t` (newest first; `-r` reverses the order, so `ls -ltr` shows the oldest first).

## Part 2. What the shell does to your line

Setup: a test directory with a few files, one of them with a space in its name.

```bash
mkdir -p ~/os-lab3/play && cd ~/os-lab3/play
touch a.c b.c file1.txt file2.txt notes.txt "alfa beta.txt"
ls
```

```
 a.c  'alfa beta.txt'   b.c   file1.txt   file2.txt   notes.txt
```
<img width="644" height="95" alt="image" src="https://github.com/user-attachments/assets/d552334a-1e21-401a-af92-9643891414de" />


### Predictions and results

| Command | My prediction | Real output | Explanation (if different) |
|---|---|---|---|
| `echo *.c` | `a.c b.c` | `a.c b.c` | |
| `echo file?.txt` | error | `file1.txt file2.txt` | |
| `echo [a-b]*` | `a.c alfa beta.txt b.c` | `a.c alfa beta.txt b.c` | |
| `echo *.xyz` | Not found | `*.xyz` | No file matches, so bash leaves the pattern as it is and `echo` prints it literally. |
| `ls alfa beta.txt` | error | `ls: cannot access 'alfa': No such file or directory` and the same for `'beta.txt'` | Word splitting turned it into two arguments. |
| `ls "alfa beta.txt"` | `'alfa beta.txt'` | `'alfa beta.txt'` | Quotes keep it as one argument. |
| `echo '$HOME' "$HOME" \$HOME` | `$HOME /home/vboxuser $HOME` | `$HOME /home/vboxuser $HOME` | Only the double-quoted one is expanded. |
| `echo {1..5} ; echo $((7 * 6))` | 1 2 3 4 5` and `42` | `1 2 3 4 5` and `42` | Brace expansion and arithmetic expansion, both done by the shell. |
| `echo "today is $(date +%A), I am $(whoami)"` | `today is Sunday, I am vboxuser` | `today is Sunday, I am vboxuser` | `$(...)` runs the command and substitutes its output, even inside double quotes. |
| `touch log{1..3}.txt && ls log*` | `log1.txt  log2.txt  log3.txt` | `log1.txt  log2.txt  log3.txt` | Braces create three names for `touch`, then `log*` matches them. |

<img width="644" height="401" alt="image" src="https://github.com/user-attachments/assets/8d700f63-65a3-4c68-acc0-98c7b3d12ef1" />

<img width="644" height="59" alt="image" src="https://github.com/user-attachments/assets/45624266-e69d-4caa-9320-7216d5faef4f" />

<img width="644" height="132" alt="image" src="https://github.com/user-attachments/assets/caaa2503-d068-4ca3-b376-c6c7b330d057" />


### Observe

**1. Predictions vs. real output**

See the table above.

**2. `echo` knows nothing about files, yet `echo *.c` printed file names. Who produced them?**

The shell. Before running `echo`, bash expands the pattern `*.c` against the files in the current directory and replaces it with the matching names. `echo` just receives `a.c` and `b.c` as two ordinary arguments and prints them. The same happens for `file?.txt`, `[a-b]*` and `log*`. When nothing matches (`*.xyz`), bash passes the pattern through unchanged.

**3. Why did `ls alfa beta.txt` fail? Two ways to fix it.**

The shell splits the line on spaces, so `ls` received two arguments, `alfa` and `beta.txt`, and neither file exists. Fixes:

1. Quote the name: `ls "alfa beta.txt"` (or with single quotes).
2. Escape the space with a backslash: `ls alfa\ beta.txt`.

(Tab completion also works, since bash inserts the backslash automatically.)

**4. Difference between single and double quotes**

Single quotes keep everything literal: `'$HOME'` prints `$HOME`. Double quotes protect spaces and wildcards but still allow `$variable`, `$(command)` and `$((arithmetic))` to be expanded: `"$HOME"` prints `/home/vboxuser`. A backslash escapes just the next character, so `\$HOME` also prints `$HOME`.

**Why `ls` shows `'alfa beta.txt'` in quotes**

`ls` adds quotes around names that contain special characters such as spaces when it prints to a terminal. That way the name is shown as one unit and can be copied and pasted straight into a command. The quotes are not part of the file name.

## Part 3. Redirection, chaining and jobs

### 1. stdout and stderr into separate files

```bash
cd ~/os-lab3
ls /etc /nope > out.txt 2> err.txt
wc -l out.txt ; cat err.txt
```

```
238 out.txt
ls: cannot access '/nope': No such file or directory
```

The listing of `/etc` (238 lines, including the `/etc:` header) went to `out.txt`, and the error about `/nope` went to `err.txt`. Nothing was printed on the terminal except the output of `wc` and `cat`.
<img width="644" height="61" alt="image" src="https://github.com/user-attachments/assets/dc3232d2-5dca-4ce1-8a9b-c10dbb5f53ff" />

### 2. Both streams into one file, and the order of redirections

```bash
ls /etc /nope > all.txt 2>&1 ; tail -2 all.txt
```

```
xml
zsh_command_not_found
```

Both streams went to `all.txt` and nothing appeared on the terminal. The error line is at the top of the file (stderr is written immediately, while the listing is written afterwards), so `tail` shows only the end of the listing.

Now the two orders compared:

```bash
ls /etc /nope > all.txt 2>&1
ls /etc /nope 2>&1 > all.txt
```

The first prints nothing: the listing and the error are both in `all.txt`. The second prints `ls: cannot access '/nope': No such file or directory` on the terminal, and `all.txt` contains only the listing.


### 3. Appending, input redirection, chaining

```bash
echo first > notes.md ; echo second >> notes.md ; cat notes.md
```
```
first
second
```

```bash
wc -l < access.log
```
```
500
```

```bash
ls /nope 2> /dev/null || echo "failed with code $?"
```
```
failed with code 2
```

```bash
mkdir -p tmp && cd tmp && echo "I am in $(pwd)" ; cd ~/os-lab3
```
```
I am in /home/vboxuser/os-lab3/tmp
```

<img width="644" height="61" alt="image" src="https://github.com/user-attachments/assets/5d85ec59-102d-48dd-968f-9bb365b273c7" />


### 4. Background job

```bash
sleep 100 &
jobs ; kill %1 ; jobs
```
```
[1] 15219
[1]+  Running                    sleep 100 &
[1]+  Running                    sleep 100 &
[1]+  Terminated                 sleep 100
```

`&` started `sleep` in the background and gave job number 1 (PID 15219). `kill %1` sent it a termination signal. The second `jobs` still said `Running` because the signal was only just delivered, and the shell reported `Terminated` right before the next prompt.
<img width="644" height="188" alt="image" src="https://github.com/user-attachments/assets/c8ff6d42-5d8f-435a-8b01-7fe0a5df6ea3" />


### Observe

**1. Which file got the error message, and which got the listing?**

`err.txt` got the error (`ls: cannot access '/nope'...`), and `out.txt` got the listing of `/etc`.

**2. `> all.txt 2>&1` vs `2>&1 > all.txt`**

The shell processes redirections from left to right. In `> all.txt 2>&1`, stdout is first pointed at `all.txt`, then stderr is made a copy of stdout, so both end up in the file. In `2>&1 > all.txt`, stderr is first made a copy of stdout while stdout is still the terminal, and only afterwards stdout is moved to the file. So stderr stays on the terminal and only the listing goes to `all.txt`. `2>&1` means "send stderr to wherever stdout points right now", not "to stdout forever".

**3. Why does `wc -l < access.log` print only a number?**

With `<`, the shell opens the file and `wc` reads from standard input, so it never learns the file name and has nothing to print next to the count. With `wc -l access.log`, the file name is an argument, so `wc` prints it after the count.

**4. The operators**

- `;` runs the next command regardless of whether the previous one succeeded.
- `&&` runs the next command only if the previous one succeeded (exit code 0).
- `||` runs the next command only if the previous one failed (non-zero exit code).


## Part 4. Answer questions with one-liners

Each line of `access.log` is: date, time, user, result (OK or FAIL), page. With `cut -d' '` the fields are 1 = date, 2 = time, 3 = user, 4 = result, 5 = page. All commands are saved in `oneliners.sh`, one per question with the question as a comment above it, and the file runs with `bash oneliners.sh` inside `~/os-lab3`.

### Q1. How many requests failed?

```bash
grep -c FAIL access.log
```
```
100
```

`grep -c` counts the matching lines. Every 5th request fails, so 500 / 5 = 100.


### Q2. How many different pages were requested, and which?

```bash
cut -d' ' -f5 access.log | sort -u | wc -l ; cut -d' ' -f5 access.log | sort -u
```
```
4
/page0
/page1
/page2
/page3
```

`cut` extracts the page field, `sort -u` sorts and removes duplicates, and `wc -l` counts what is left. The `;` runs the same pipeline twice: once for the count and once for the list.


### Q3. How many requests did each page get?

```bash
cut -d' ' -f5 access.log | sort | uniq -c
```
```
    125 /page0
    125 /page1
    125 /page2
    125 /page3
```

`uniq -c` counts only *adjacent* duplicates, so `sort` has to come first. Check: 4 × 125 = 500, which matches the number of lines in the log.


### Q4. Which user or users have the most failed requests, and how many?

```bash
grep FAIL access.log | cut -d' ' -f3 | sort | uniq -c | sort -rn | awk 'NR==1{m=$1} $1==m'
```
```
     15 user5
     15 user3
```

Two users share the top place with 15 failures each, so a plain `head -1` would have hidden one of them. The `awk` stores the count from the first line (the maximum, since the list is sorted) in `m` and prints every line with that same count.

Self-check: without the final `awk` the pipeline lists all users. Users 5 and 3 have 15 failures each and the other five users have 14 each, so 15 + 15 + 5 × 14 = 100, which equals the answer to Q1.

```bash
grep FAIL access.log | cut -d' ' -f3 | sort | uniq -c | sort -rn
```


### Q5. How many requests did user3 make, and how many of them failed?

```bash
grep -c ' user3 ' access.log ; grep ' user3 ' access.log | grep -c FAIL
```
```
72
15
```

The spaces around `user3` make sure only that user matches. The second command filters twice: first by user, then by result. The 15 failures also agree with the Q4 list.


### Q6. Print the last 3 failed requests, showing only time and user.

```bash
grep FAIL access.log | tail -3 | cut -d' ' -f2,3
```
```
10:10 user0
10:15 user5
10:20 user3
```

The order matters: first filter the failures, then keep the last three lines with `tail -3`, and only then cut out the fields. If `cut` came first, `tail` would have no result column to filter on.


### Q7. Which login shells appear in /etc/passwd, and how many accounts use each?

```bash
cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn
```
```
     40 /usr/sbin/nologin
      6 /bin/false
      2 /bin/bash
      1 /bin/sync
```

Here the separator is `:` and the shell is field 7. In total there are 49 accounts. Most of them are system or service accounts with `nologin` or `/bin/false`, which cannot log in interactively; only two use `/bin/bash`.


### Q8. Why do `ls /etc | wc -l` and `ls -l /etc | wc -l` differ by exactly one?

```bash
ls /etc | wc -l ; ls -l /etc | wc -l
```
```
237
238
```

`ls -l` prints an extra first line, `total N`, which is a summary of the disk blocks used and is not a file. `wc -l` counts it like any other line, so the long listing has one line more than the number of entries.

```bash
ls -l /etc | head -1
```


### Bonus. Failed requests where the minute ends in 0

```bash
grep -E '10:[0-5]0 .* FAIL' access.log | wc -l
```
```
50
```

The pattern means: `10:`, then a minute digit from 0 to 5, then `0`, then anything, then `FAIL`. There are 50 such requests.

### Full run

```bash
bash oneliners.sh
```

<img width="644" height="432" alt="image" src="https://github.com/user-attachments/assets/3b247b61-9abd-4fd5-8dc0-d049c769e587" />


- The Q3 counts add up to 500, the number of lines in the log.
- The per-user failure counts behind Q4 add up to 100, the answer to Q1.
- Q5 (user3 with 15 failures) agrees with the Q4 list.

## Conclusion

In this lab I worked with Linux from the command line and saw what the shell actually does with the text I type.

**Part 1.** I learned to move faster with Tab completion and Ctrl+R history search, and to find help without a browser (`man`, `--help`, `man -k`, `help`). I also saw the difference between built-in and external commands. `cd` has to be a built-in, because a child process cannot change the working directory of its parent shell. The shell finds external programs by searching the directories in `PATH` (`ls` is in `/usr/bin`), and a command that is not found returns exit code 127.

**Part 2.** The shell rewrites the command line before the program runs. Wildcards (`*`, `?`, `[a-b]`), braces, `$(...)` and `$((...))` are all expanded by the shell, not by `echo` or `ls`. A pattern that matches nothing is passed through unchanged, an unquoted space splits one name into two arguments, and quoting controls what gets expanded: single quotes keep everything literal, double quotes still allow `$` expansions.

**Part 3.** Every program has stdout and stderr, and they can be redirected independently with `>`, `>>`, `2>` and `<`. Redirections are processed from left to right, which is why `> all.txt 2>&1` and `2>&1 > all.txt` give different results. The operators `;`, `&&` and `||` chain commands depending on the exit code, and `&` with `jobs` and `kill` manages background processes.

**Part 4.** Small tools joined by pipes can answer real questions about data. Building each pipeline one stage at a time (`grep`, `cut`, `sort`, `uniq`, `wc`, `tail`) made the answers easy to check: the page counts add up to 500 lines, and the per-user failure counts add up to the 100 failures from Q1. The "most failures" question had a tie between two users, a reminder to check the data before trusting `head -1`. The Q8 puzzle was explained by the `total` line that `ls -l` prints.

**Part 5.** *(Add one or two sentences after finishing `report.sh`: that a script is just these commands saved in a file with a `#!/bin/bash` line and execute permission, and that errors go to stderr with a non-zero exit code so other programs can detect them.)*

Overall, the main lesson is that the command line is a set of small programs connected by the shell, and understanding the order of expansion, redirection and pipes makes it possible to predict and debug any command before running it.
