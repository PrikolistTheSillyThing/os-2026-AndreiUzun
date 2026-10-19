# Operating Systems — Laboratory Work No. 6
## Inside the xv6 File System

**Student:** Andrey Uzun, FAF-242
**Course:** Operating Systems, FCIM / FAF, UTM, 2026-2027
**Track:** B (xv6 on RISC-V, QEMU)

---

## 1. Goal

Read what the xv6 file system exposes (inodes, directories as tables of entries, hard links), write a `stat`-based tool (`mystat`), measure the limits defined in `kernel/fs.h`, and write `find`, which walks the directory tree recursively.

Environment: Ubuntu 26.04 (VirtualBox), `riscv64-linux-gnu-gcc`, QEMU 10.2, `~/xv6-riscv`. Programs were written on the host in `user/`, registered in `UPROGS` in the Makefile, and run inside xv6 after `make qemu`.

---

## 2. Part 1: Reading what xv6 shows

Commands in xv6:

```
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2441
cat            2 3 36728
...
console        3 24 0
f              2 25 6
$ mkdir a
$ ls a
.              1 26 32
..             1 1 1024
```

Each line is: name, type, inode number, size in bytes.

**(1) Types.** From `kernel/stat.h`: type 1 = `T_DIR` (directory), type 2 = `T_FILE` (regular file), type 3 = `T_DEVICE` (device). In the listing, `.` and `..` are directories, `README`, `cat`, etc. are files, and `console` is a device. It has size 0 because a device has no data blocks on disk.

**(2) Root inode.** The root directory is inode 1 (`ROOTINO`). It has no parent, so `..` points back to the root itself, which is why `.` and `..` both show `1 1 1024`. In the new directory `a`, `.` is its own inode (26) while `..` is inode 1, the parent.

**(3) Directory entry size.** `struct dirent` in `kernel/fs.h` is a `ushort inum` (2 bytes) plus `char name[DIRSIZ]` with `DIRSIZ = 14`, so one entry is 2 + 14 = **16 bytes**. The empty directory `a` has size 32 = 2 entries, which are `.` and `..` (as `ls a` shows).

---

## 3. Part 2: `mystat`

### Code (`user/mystat.c`)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char *
typename(int t)
{
  if(t == T_DIR) return "directory";
  if(t == T_FILE) return "file";
  if(t == T_DEVICE) return "device";
  return "?";
}

int
main(int argc, char *argv[])
{
  struct stat st;
  for(int i = 1; i < argc; i++){
    if(stat(argv[i], &st) < 0){
      fprintf(2, "mystat: cannot stat %s\n", argv[i]);
      continue;
    }
    printf("%s: %s, inode %d, links %d, size %d bytes\n",
           argv[i], typename(st.type), st.ino, st.nlink, (int)st.size);
  }
  exit(0);
}
```

Registered in the Makefile with the line `$U/_mystat\` in `UPROGS`.

`stat()` fills a `struct stat`; on failure it returns a negative value, so the program prints an error to fd 2 and continues with the next argument. The cast `(int)st.size` is needed because xv6's `printf` has no `%u`/`%lu`.

### Test

```
$ mystat README . console
README: file, inode 2, links 1, size 2441 bytes
.: directory, inode 1, links 1, size 1024 bytes
console: device, inode 25, links 1, size 0 bytes
```

This matches the lab's expected output. (The `console` inode is 25 here, not 24 as in the Part 1 listing, because adding `mystat` to the disk image consumed one more inode.)

### Hard links

```
$ ln README r2
$ mystat README r2
README: file, inode 2, links 2, size 2441 bytes
r2: file, inode 2, links 2, size 2441 bytes
$ rm README
$ mystat r2
r2: file, inode 2, links 1, size 2441 bytes
$ cat r2 | wc
48 336 2441
```

**(1) What changed and what did not.** After `ln`, both names report the **same inode (2)**, same size, and `links` rises from 1 to 2. After `rm README`, `links` drops back to 1; the inode number, size, and content are unchanged. `cat r2 | wc` still gives 48 lines / 336 words / 2441 bytes, so the data survived. The two names were never copies: they were two directory entries pointing at one inode. `ln` adds an entry and increments `nlink`; `rm` removes an entry and decrements it.

**(2) Where the link count is decremented, and when blocks are freed.** In `kernel/sysfile.c`, `sys_unlink` decrements the count at line 244: `ip->nlink--;` (line 239, `dp->nlink--`, is the parent directory losing the link from a removed subdirectory's `..`). `sys_unlink` does not free any blocks. Freeing is done in `iput` (`kernel/fs.c`), which computes `last = (ip->ref == 1 && ip->valid && ip->nlink == 0)`. The inode's data blocks are freed (via `itrunc`) only when **both** hold: no directory entry names it any more (`nlink == 0`) and no process still has it open (`ref == 1`, the last reference). So a file deleted while still open lives on until it is closed.

**(3) Fields of an xv6 inode.** `struct dinode` in `kernel/fs.h` has: `short type`, `short major`, `short minor` (device numbers, only for `T_DEVICE`), `short nlink`, `uint size`, and `uint addrs[NDIRECT + 1]` (data block addresses). There is no owner, no permission bits, no timestamps, and no symbolic-link type, which is why xv6 has none of those features.

---

## 4. Part 3: Measuring the limits

Constants from `kernel/fs.h`:

```
5:  #define BSIZE   1024 // block size
26: #define NDIRECT   12
27: #define NINDIRECT (BSIZE / sizeof(uint))
54: #define DIRSIZ 14
```

**(1) Long directory name.**

```
$ mkdir averyveryverylongname
$ ls
...
console        3 25 0
averyveryveryl 1 26 32
```

`ls` shows `averyveryveryl`, exactly 14 characters. The constant `DIRSIZ = 14` limits the name field of a directory entry, so the name is truncated when the entry is created; the longer name is never stored on disk.

**(2) Largest file.**

- `NINDIRECT = BSIZE / sizeof(uint) = 1024 / 4 = 256` block numbers fit in the indirect block
- Total blocks per file = `NDIRECT + NINDIRECT = 12 + 256 = 268`
- Maximum size = `268 × 1024 = 274,432 bytes` (about 268 KB)

**(3) Disk layout.** Output of `make fs.img`:

```
nmeta 47 (boot, super, log blocks 31, inode blocks 13, bitmap blocks 1) blocks 1953 total 2000
```

| Region | Blocks |
|---|---|
| Boot + superblock | 2 |
| Log | 31 |
| Inodes | 13 |
| Free-block bitmap | 1 |
| **Metadata total (`nmeta`)** | **47** |
| Data | 1953 |
| **Whole disk** | **2000** |

(2 + 31 + 13 + 1 = 47 metadata blocks; 47 + 1953 = 2000.)

---

## 5. Part 4: `find`

### Code (`user/find.c`)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

// pointer to the last component of path ("a/b/r2" -> "r2")
char *
lastname(char *path)
{
  char *p;
  for(p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  return p + 1;
}

void
find(char *path, char *name)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // check the name for files AND directories (so "find a b" prints a/b)
  if(strcmp(lastname(path), name) == 0)
    printf("%s\n", path);

  if(st.type == T_DIR){
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
      fprintf(2, "find: path too long\n");
      close(fd);
      return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, name);
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "usage: find <dir> <name>\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
```

Registered in the Makefile with `$U/_find\` in `UPROGS`.

### Design notes

- The structure follows `user/ls.c`: open a directory, read `struct dirent` entries one by one, build the full path in a buffer, recurse.
- Entries with `inum == 0` are free slots and are skipped.
- `.` and `..` are skipped; recursing into them would loop forever.
- The name comparison (`strcmp` on the last path component) is done for files **and** directories, so `find a b` finds the directory `a/b`.
- `close(fd)` is called on every exit path, because a process may have only 16 open files and the recursion holds one descriptor per directory level.
- A length check before building the path prevents overflowing `buf`.

### Test (fresh disk after `make clean ; make qemu`)

```
$ mkdir a
$ mkdir a/b
$ echo x > a/b/r2
$ echo y > a/r2
$ ln cat r2
$ find . r2
./a/b/r2
./a/r2
./r2
$ find a b
a/b
```

Both outputs match the lab's expected results.

The optional stretch (`-n` flag printing inode numbers) was not done.

---

## 6. Conclusion

In this lab I looked at the xv6 file system from the user side and then from the source. The listings showed that a directory is just a file made of 16-byte entries (`inum` + a 14-byte name), that the root is inode 1, and that names live only in directories, while everything else (type, link count, size, block addresses) lives in the inode. The `ln`/`rm` experiment showed this directly: two names for one inode, and the data persisting until the last name (and last open reference) is gone. Reading `sys_unlink` and `iput` confirmed that unlinking only decrements `nlink`, and freeing blocks is a separate step in `iput`.

Measuring the limits tied the constants together: `DIRSIZ = 14` truncates file names, `NDIRECT + NINDIRECT` caps a file at 274,432 bytes, and the `nmeta` line splits the 2000-block disk into 47 metadata blocks (log 31, inodes 13, bitmap 1, boot/super 2) and 1953 data blocks. Writing `mystat` and `find` practised the system-call side (`stat`, `fstat`, `open`, `read`, `close`) and the practical constraints of a small OS: no editor inside xv6, a fixed file-descriptor limit, and `-Werror` builds.

Source files are `user/mystat.c`, `user/find.c` and the Makefile with the two `UPROGS` lines, and everything is pushed to the Git repository.
