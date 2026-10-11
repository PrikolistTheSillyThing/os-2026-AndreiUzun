# Lab 3: The Command Line: Shell, Pipes and Text Filters

**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** A (the real OS, from the outside, Linux)
**Environment:** Ubuntu 26.04.1 LTS (VirtualBox), bash

## Introduction

This lab is about working fast in the Linux command line. The goals are to move around bash efficiently, predict what the shell does to a command line before it runs (globbing, quoting, expansion), redirect input and output, and answer questions about data by joining small filters (`grep`, `cut`, `sort`, `uniq`, `wc`) with pipes.

The key idea is that the shell is only a program that rewrites your line and starts other programs. Most of the "magic" (wildcards, variables, `$(...)`) happens in the shell *before* the program ever runs, and small tools connected by pipes can answer real questions about data.

For the later parts I generated an artificial web access log of 500 lines:

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
