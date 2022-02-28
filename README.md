# xv6-public
[![Hits](https://hits.seeyoufarm.com/api/count/incr/badge.svg?url=https%3A%2F%2Fgithub.com%2Fcyw320712%2FSKKU-xv6&count_bg=%23767C72&title_bg=%23555555&icon=github.svg&icon_color=%23FFFFFF&title=hits&edge_flat=false)](https://hits.seeyoufarm.com)

## Project with xv6 machine
> :star: If there are any errors in the content or if there is anything to be added, please let us know through a Pull Request. <br>
> :star: Thank you for your interest through Star or Watching. :)<br>

## Project (SKKU Operating System Class)

### [[PA0]: xv6 install and boot](https://github.com/cyw320712/xv6-public/tree/master/pa0)
- How to install and boot xv6
- Print any message in init
- score: (10/10)

### [[PA1]: xv6 system call](https://github.com/cyw320712/xv6-public/tree/master/pa1)
- Make new three system calls related to nice values
- `getnice()`: function for get nice value
- `setnice()`: function for set nice value
- `ps()`: function for print pid, nice, status, and name
- score: (15/15)

### [[PA2]: xv6 CFS scheduler](https://github.com/cyw320712/xv6-public/tree/master/pa2)
- Change round-robin scheduler to Linux CFS(Completely Fair Scheduler)
- Should consider very large value of runtime, vruntime...
- score: (21/25)

### [[PA3]: xv6 Virtual memory management](https://github.com/cyw320712/xv6-public/tree/master/pa3)
- Implement three system calls and page fault handler on xv6
- mmap() syscall
- Page fault handler
- munmap() syscall
- freemem() syscall
- score: (22/25)

### [[pa4]: xv6 Page Replacement](https://github.com/cyw320712/xv6-public/tree/master/pa4)
- Implement page-level swapping, LRU list management
- Page replacement policy is Clock Algorithm
- Should Implement Swap-in, Swap-out operation, LRU list management and etcs..
- It's not uploaded yet

## What is xv6

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern x86-based multiprocessor using ANSI C.

ACKNOWLEDGMENTS

xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
to Peer Communications; ISBN: 1-57398-013-7; 1st edition (June 14,
2000)). See also https://pdos.csail.mit.edu/6.828/, which
provides pointers to on-line resources for v6.

xv6 borrows code from the following sources:
    JOS (asm.h, elf.h, mmu.h, bootasm.S, ide.c, console.c, and others)
    Plan 9 (entryother.S, mp.h, mp.c, lapic.c)
    FreeBSD (ioapic.c)
    NetBSD (console.c)

The following people have made contributions: Russ Cox (context switching,
locking), Cliff Frey (MP), Xiao Yu (MP), Nickolai Zeldovich, and Austin
Clements.

We are also grateful for the bug reports and patches contributed by Silas
Boyd-Wickizer, Anton Burtsev, Cody Cutler, Mike CAT, Tej Chajed, eyalz800,
Nelson Elhage, Saar Ettinger, Alice Ferrazzi, Nathaniel Filardo, Peter
Froehlich, Yakir Goaron,Shivam Handa, Bryan Henry, Jim Huang, Alexander
Kapshuk, Anders Kaseorg, kehao95, Wolfgang Keller, Eddie Kohler, Austin
Liew, Imbar Marinescu, Yandong Mao, Matan Shabtay, Hitoshi Mitake, Carmi
Merimovich, Mark Morrissey, mtasm, Joel Nider, Greg Price, Ayan Shafqat,
Eldar Sehayek, Yongming Shen, Cam Tenny, tyfkda, Rafael Ubal, Warren
Toomey, Stephen Tu, Pablo Ventura, Xi Wang, Keiichi Watanabe, Nicolas
Wolovick, wxdao, Grant Wu, Jindong Zhang, Icenowy Zheng, and Zou Chang Wei.

The code in the files that constitute xv6 is
Copyright 2006-2018 Frans Kaashoek, Robert Morris, and Russ Cox.

ERROR REPORTS

We don't process error reports (see note on top of this file).

BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run
"make". On non-x86 or non-ELF machines (like OS X, even on x86), you
will need to install a cross-compiler gcc suite capable of producing
x86 ELF binaries (see https://pdos.csail.mit.edu/6.828/).
Then run "make TOOLPREFIX=i386-jos-elf-". Now install the QEMU PC
simulator and run "make qemu".
