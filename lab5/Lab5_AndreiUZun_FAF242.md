# Lab 5: The File System with Your Own Hands (Linux)

**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Student:** Andrey
**Environment:** Ubuntu running in VirtualBox, user `vboxuser` (not root). All hardware numbers below (disk, CPU, RAM) are those of the virtual machine, not of the host laptop.

## What I did

I worked through all four parts of the lab in `~/os-lab5`, running every command as a normal user (`whoami` printed `vboxuser`). For each part I ran the commands, wrote down what I expected where the lab asked for a prediction, compared it with the real output, and then explained the result. The optional stretch tasks were skipped.

One practical note: some commands were first pasted with HTML-escaped characters (`&amp;`, `&gt;`), which made bash fail. Those failed attempts are not included below; only the clean reruns are.

---

## Part 1. The tree and the file types

```
$ ls /
bin   cdrom  etc   lib    lost+found  mnt  proc  run   snap  sys  usr
boot  dev    home  lib64  media       opt  root  sbin  srv   tmp  var

$ ls -ld /etc /etc/passwd /bin /dev/null /tmp
lrwxrwxrwx   1 root root     7 Apr 20 08:46 /bin -> usr/bin
crw-rw-rw-   1 root root  1, 3 Oct 18 17:48 /dev/null
drwxr-xr-x 142 root root 12288 Oct 18 18:40 /etc
-rw-r--r--   1 root root  2823 Sep 28 15:29 /etc/passwd
drwxrwxrwt  17 root root   420 Oct 18 19:22 /tmp

$ ls -l /dev | head
total 0
crw-r--r-- 1 root     root     10, 235 Oct 18 17:48 autofs
drwxr-xr-x 2 root     root        500 Oct 18 17:54 block
drwxr-xr-x 2 root     root         80 Oct 18 17:47 bsg
crw------- 1 root     root     10, 234 Oct 18 17:47 btrfs-control
drwxr-xr-x 3 root     root         60 Oct 18 17:47 bus
lrwxrwxrwx+1 root     root          3 Oct 18 17:47 cdrom -> sr0
drwxr-xr-x 2 root     root       3860 Oct 18 18:32 char
crw------- 1 root     tty       5,   1 Oct 18 17:48 console
lrwxrwxrwx 1 root     root         11 Oct 18 17:47 core -> /proc/kcore

$ file /usr/bin/ls /etc/passwd /bin
/usr/bin/ls: symbolic link to ../lib/cargo/bin/coreutils/ls
/etc/passwd: ASCII text
/bin:        symbolic link to usr/bin

$ du -sh /var/log 2>/dev/null ; du -sh /usr 2>/dev/null
90M     /var/log
5.5G    /usr
```

### Observe

**(1) Types from the first letter of `ls -l`.**

| Path | First letter | Type |
| --- | --- | --- |
| `/etc` | `d` | directory |
| `/etc/passwd` | `-` | regular file |
| `/bin` | `l` | symbolic link |
| `/dev/null` | `c` | character device |
| `/tmp` | `d` | directory (the final `t` is the sticky bit, see Part 3) |

**(2) What is `/bin`?** It is a symbolic link, not a directory, and it points to `usr/bin` (a relative target; the link size of 7 bytes is exactly the length of the string `usr/bin`). `file /bin` confirms it. So on this system the old top-level `/bin` is just an alias for `/usr/bin`.

**(3) Character and block devices in `/dev`.**
- Character devices (type `c`): `/dev/null` (major 1, minor 3), `/dev/console` and `/dev/autofs`. They show `major, minor` numbers where a regular file would show its size, because a device has no size.
- Block devices (type `b`): the disk `sda` and its partitions `sda1`, `sda2` (major 8, minors 0, 1, 2), plus the virtual CD drive `sr0` (major 11). I identified them with `lsblk` in Part 4, where they appear as `disk`, `part` and `rom` entries. The `loop*` devices (major 7), which back the snap packages, are block devices too.

<!-- TODO (optional): paste the output of `ls -l /dev/sda* /dev/sr0` here to show the leading `b` directly. -->

**(4) `/var/log` vs `/usr`.** `/usr` is much bigger: 5.5G against 90M, so roughly 60 times larger. A small caveat: as a normal user, `du` could not read some subdirectories of `/var/log` (`gdm3`, `sssd`, `speech-dispatcher`, `private`, `chrony`: "Permission denied"), so the real size of `/var/log` is slightly higher than 90M. The conclusion does not change.

A side observation: `/usr/bin/ls` is a symlink into `../lib/cargo/bin/coreutils/ls`, so this Ubuntu uses the Rust rewrite of coreutils instead of the GNU one.

---

## Part 2. Inodes and links

```
$ echo hello > notes.txt
$ stat notes.txt
  File: notes.txt
  Size: 6               Blocks: 8          IO Block: 4096   regular file
Device: 8,2     Inode: 419259      Links: 1
Access: (0664/-rw-rw-r--)  Uid: ( 1000/vboxuser)   Gid: ( 1000/vboxuser)
Access: 2026-10-18 19:28:20.659490701 +0000
Modify: 2026-10-18 19:28:20.659490701 +0000
Change: 2026-10-18 19:28:20.659490701 +0000
 Birth: 2026-10-18 19:28:20.659490701 +0000
$ ls -li notes.txt
419259 -rw-rw-r-- 1 vboxuser vboxuser 6 Oct 18 19:28 notes.txt
```

Hard link, then remove the "original":

```
$ ln notes.txt hard.txt
$ ls -li notes.txt hard.txt
419259 -rw-rw-r-- 2 vboxuser vboxuser 6 Oct 18 19:28 hard.txt
419259 -rw-rw-r-- 2 vboxuser vboxuser 6 Oct 18 19:28 notes.txt
$ rm notes.txt
$ cat hard.txt ; ls -li hard.txt
hello
419259 -rw-rw-r-- 1 vboxuser vboxuser 6 Oct 18 19:28 hard.txt
```

Symbolic link, then remove its target:

```
$ ln -s hard.txt soft.txt
$ ls -li hard.txt soft.txt
419259 -rw-rw-r-- 1 vboxuser vboxuser 6 Oct 18 19:28 hard.txt
419262 lrwxrwxrwx 1 vboxuser vboxuser 8 Oct 18 19:28 soft.txt -> hard.txt
$ cat soft.txt
hello
$ rm hard.txt
$ cat soft.txt ; ls -l soft.txt
cat: soft.txt: No such file or directory
lrwxrwxrwx 1 vboxuser vboxuser 8 Oct 18 19:28 soft.txt -> hard.txt
```

Directory link counts:

```
$ mkdir d ; ls -ld d
drwxrwxr-x 2 vboxuser vboxuser 4096 Oct 18 19:29 d
$ mkdir d/x d/y ; ls -ld d
drwxrwxr-x 4 vboxuser vboxuser 4096 Oct 18 19:29 d
```

Size is not space:

```
$ echo x > one.txt ; ls -l one.txt ; du -h one.txt
-rw-rw-r-- 1 vboxuser vboxuser 2 Oct 18 19:29 one.txt
4.0K    one.txt
$ truncate -s 1G big.img ; ls -lh big.img ; du -h big.img
-rw-rw-r-- 1 vboxuser vboxuser 1.0G Oct 18 19:29 big.img
0       big.img
$ rm big.img
```

### Observe

**(1) Inode and link count in `stat`.** The inode number is the `Inode:` field (419259). The number of names the file has is the `Links:` field (1 at first). `ls -li` shows the same two numbers: the inode in the first column, the link count right after the permission string.

**(2) Why `hard.txt` still worked after `rm notes.txt`.** A file name is not part of the inode; it is just an entry in a directory that points to an inode. After `ln`, both `notes.txt` and `hard.txt` pointed to inode 419259 and the link count was 2. `rm notes.txt` removed only that directory entry and decreased the link count from 2 to 1 (visible in the last `ls -li`). The inode and its data blocks are freed only when the link count reaches 0 and no process has the file open. So there was never an "original" and a "copy": both names were equal.

**(3) Why `cat soft.txt` failed.** A symbolic link is a separate file with its own inode (419262 here) whose content is just a path. `soft.txt` contains the text `hard.txt`, so its size is **8 bytes**, exactly the number of characters in the target path `hard.txt`. When I removed `hard.txt`, its link count dropped to 0 and the real file was deleted, but `soft.txt` kept pointing at that name. `cat` follows the path, finds nothing and reports "No such file or directory". It is a dangling symlink: the link exists, its target does not.

**(4) Directory link counts.** A new directory `d` has 2 links:
1. its name `d` in the parent directory (`os-lab5`);
2. its own `.` entry, which points to itself.

After `mkdir d/x d/y` the count is 4, because each subdirectory contains a `..` entry pointing back to `d`, which adds one link per subdirectory. So a directory's link count is 2 plus the number of its subdirectories. Regular files inside it do not change the count.

**(5) Why 2 bytes use 4.0K and 1 GB uses 0.** `ls -l` shows the logical size stored in the inode, while `du` shows the space actually allocated. The file system allocates whole blocks, and the block size here is 4 KB (`IO Block: 4096`), so even a 2-byte file occupies one full 4K block. `truncate -s 1G` is the opposite case: it only sets the size field in the inode and allocates no data blocks at all, so `du` reports 0. This is a sparse file: reading it returns zeros without those zeros being stored on disk.

---

## Part 3. Permissions

Files:

```
$ echo secret > s.txt
$ chmod 600 s.txt ; ls -l s.txt
-rw------- 1 vboxuser vboxuser 7 Oct 18 19:34 s.txt
$ chmod 000 s.txt ; cat s.txt
cat: s.txt: Permission denied
$ chmod u+r s.txt ; cat s.txt
secret
$ umask ; touch new.txt ; mkdir newdir ; ls -ld new.txt newdir
0002
-rw-rw-r-- 1 vboxuser vboxuser    0 Oct 18 19:34 new.txt
drwxrwxr-x 2 vboxuser vboxuser 4096 Oct 18 19:34 newdir
```

Directories:

```
$ mkdir box ; echo hi > box/f
$ chmod u=r,go= box ; ls box ; cat box/f     # read only
f
cat: box/f: Permission denied
$ chmod u=x,go= box ; ls box ; cat box/f     # execute only
ls: cannot open directory 'box': Permission denied
hi
$ chmod 755 box
```

Special bits and other people's files:

```
$ ls -l /usr/bin/passwd ; ls -ld /tmp
-rwsr-xr-x 1 root root 93640 Feb  2  2026 /usr/bin/passwd
drwxrwxrwt 16 root root 400 Oct 18 19:26 /tmp
$ cat /etc/shadow
cat: /etc/shadow: Permission denied
$ ls -l /etc/shadow
-rw-r----- 1 root shadow 1336 Sep 28 15:31 /etc/shadow
```

### Observe

**(1) Permission strings and uses.**

| Mode | String | Typical real use |
| --- | --- | --- |
| 600 | `rw-------` | private files, e.g. an SSH private key (ssh refuses keys that are more open) |
| 644 | `rw-r--r--` | ordinary files everyone may read, only the owner edits, e.g. `/etc/passwd` |
| 755 | `rwxr-xr-x` | programs, scripts and public directories: everyone can run or enter, only the owner can modify |
| 700 | `rwx------` | private directories, e.g. `~/.ssh` or a closed home directory |

**(2) Why ownership does not help with `chmod 000`.** The kernel checks only one class of permission bits: if the process is the owner, the *owner* bits apply, and here they are `---`. It does not fall back to group or other bits. Ownership does not mean access; it means the right to change the mode, which is why `chmod u+r s.txt` worked and `cat` then succeeded.

**(3) Umask.** My umask is `0002`. Programs request mode 666 for new files and 777 for new directories, and the umask removes the bits that are set in it:
- file: 666 minus 002 gives 664, `-rw-rw-r--` (matches `new.txt`);
- directory: 777 minus 002 gives 775, `drwxrwxr-x` (matches `newdir`).

New files never get `x` by default because the request starts from 666, not 777.

**(4) The `box` experiment.** On a directory, `r` allows reading the list of names inside it, and `x` allows passing through it, meaning looking up an entry by name to reach its inode.
- Read only (`u=r`): `ls box` worked and printed `f`, but `cat box/f` failed. I could see the name but could not go through the directory to reach the file.
- Execute only (`u=x`): `ls box` failed (cannot open directory), but `cat box/f` worked and printed `hi`. I could not list the contents, but since I knew the name `f` I could still open it.

To access `box/f` you need `x` on `box` (and on every directory above it); to list a directory you need `r`.

**(5) `s` in `/usr/bin/passwd` and `t` in `/tmp`.**
- The `s` in `-rwsr-xr-x` is the **setuid** bit. It replaces the owner's `x` (lowercase `s` means `x` is set as well). When any user runs the program, the process runs with the identity of the file's owner, here root, instead of the caller's. `passwd` needs this because it must update `/etc/shadow`, which is `-rw-r----- root shadow`; as a normal user I cannot even read it (my `cat` was denied), so without setuid an ordinary process could never change a password hash. Setuid gives that one trusted program the privilege without giving it to the user.
- The `t` in `drwxrwxrwt` is the **sticky bit**. `/tmp` is writable by everyone, which would normally let any user delete other users' files. With the sticky bit, only the file's owner (or the directory's owner, or root) can delete or rename a file in it.

---

## Part 4. Disks and the kernel as files

```
$ lsblk
NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
loop0    7:0    0     4K  1 loop /snap/bare/5
...    (loop1 to loop17: snap packages, all read-only, omitted here)
sda      8:0    0    25G  0 disk 
├─sda1   8:1    0     1M  0 part 
└─sda2   8:2    0    25G  0 part /
sr0     11:0    1  1024M  1 rom  

$ df -h
Filesystem      Size  Used Avail Use% Mounted on
tmpfs           680M  2.7M  677M   1% /run
/dev/sda2        25G  8.9G   15G  39% /
tmpfs           1.7G     0  1.7G   0% /dev/shm
none            1.0M     0  1.0M   0% /run/credentials/systemd-journald.service
tmpfs           1.7G   32K  1.7G   1% /tmp
none            1.0M     0  1.0M   0% /run/credentials/systemd-resolved.service
tmpfs           340M  104K  340M   1% /run/user/1000

$ mount | grep " / "
/dev/sda2 on / type ext4 (rw,relatime)

$ grep -c processor /proc/cpuinfo
4

$ head -3 /proc/meminfo
MemTotal:        3479664 kB
MemFree:          107372 kB
MemAvailable:     913840 kB

$ head -5 /proc/self/status
Name:   head
Umask:  0002
State:  R (running)
Tgid:   29088
Ngid:   0

$ ls -l /proc/self/fd
total 0
lrwx------ 1 vboxuser vboxuser 64 Oct 18 19:39 0 -> /dev/pts/0
lrwx------ 1 vboxuser vboxuser 64 Oct 18 19:39 1 -> /dev/pts/0
lrwx------ 1 vboxuser vboxuser 64 Oct 18 19:39 2 -> /dev/pts/0
lr-x------ 1 vboxuser vboxuser 64 Oct 18 19:39 3 -> /proc/29089/fd
```

### Observe

**(1) My disk.**

```
sda  (25G disk)
├─ sda1   1M    not mounted
└─ sda2   25G   ext4, mounted at /
```

The disk is `sda`, with two partitions. `sda2` is mounted at `/` and its file system type is **ext4** (mounted read-write, `rw,relatime`). `sda1` is a tiny 1M partition with no mount point. `sr0` is the virtual CD drive (1024M, read-only). The `loop` devices are read-only mounts of snap packages.

**(2) How full is the root file system?** `/dev/sda2` is 25G in total, with 8.9G used and 15G available: **39% full**.

**(3) CPU cores and RAM according to `/proc`.** 4 CPU cores (`grep -c processor /proc/cpuinfo` printed 4). `MemTotal` is 3479664 kB, which is about **3.3 GiB** of RAM. These are the values of the virtual machine.

**(4) Which program did `/proc/self/status` show?** It showed `Name: head`. `/proc/self` always refers to the process that is reading it, and the process that opened the file was `head`, so the kernel described `head` itself. With `cat /proc/self/status` it would say `cat`. The same applies to `ls -l /proc/self/fd`: descriptors 0, 1 and 2 (stdin, stdout, stderr) all point to my terminal `/dev/pts/0`, and descriptor 3 points to a `/proc/<pid>/fd` directory, which is the directory that `ls` itself was reading at that moment.

---

## Conclusion

This lab showed that on Linux a lot of what looks like "files" is really something else, or is only a name:

- A file's identity is its **inode**, and a name is just a directory entry pointing to it. That explains hard links (several names, one inode, a link count), why `rm` only removes a name and frees the data when the count reaches zero, and why directories have link counts of 2 plus their subdirectories.
- A **symbolic link** is its own small file containing a path, so it can dangle when the target disappears.
- File **size is not disk usage**: small files take whole 4K blocks, and sparse files take almost nothing.
- **Permissions** are checked by class (owner, then group, then other), `r` and `x` mean different things on directories, the umask shapes the defaults for new files, and the setuid and sticky bits solve specific problems (`passwd` editing `/etc/shadow`, a shared `/tmp`).
- The **kernel exposes itself as files** (`/dev` for devices, `/proc` for processes and hardware info), and `lsblk`, `df` and `mount` show how the disk, partitions and ext4 file system fit together.

All commands were run as a normal user, so the permission experiments behaved as intended.
