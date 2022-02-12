#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  uint vaddr;
  pde_t *pde;

  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    //vaddr = va;
    //pde = page directory entry pointer;
    //PDX = PDX(*pde);
    //PTX = PTX(*pde);
    //PTE_P check = ((uint*)PTE_ADDR( P2V(*vaddr)))[PTX(addr)] & PTE_P;

    vaddr = rcr2(); // va
    pde = &(myproc()->pgdir[PDX(vaddr)]); //pde
    pte_t *pte;
    //cprintf("==============================================\n");
    //cprintf("page fault occured\n");

    int offset = PDX(vaddr) + PTX(vaddr) + (vaddr & 0xfff);
    int flag = ((uint*)PTE_ADDR( P2V(*pde)))[PTX(vaddr)] & 1;

    //cprintf("addr: %x, vaddr: %x, P2V: %x pgdir: %x\n", vaddr, pde, P2V(pde), myproc()->pgdir);
    //cprintf("offset: %x PTE_P: %x\n", offset, flag);

    if(flag == 0 ){
      //if(((uint *)PTE_ADDR(P2V(*vaddr)))[PTX(addr)] & PTE_PG){
      //in pa, P2V(pa)
      
      char *mem;
      mem = kalloc();
      
      memset(mem, 0, PGSIZE);
      //cprintf("1\n");
      swapread(mem, offset);
      
      //cprintf("2\n");
      clearBitmap(offset);

      pte = walkpgdir(pde, (char *)vaddr, 0);
      //cprintf("3\n");
      //Set pagetableentry to physical address & PTE_P
      *pte = V2P(mem) | PTE_P | PTE_U | PTE_W;
      //set vaddr to physical address
      //cprintf("Final pa: %x, V2P: %x entry: %x\n", mem, V2P(mem), *pte);
      //cprintf("==============================================\n");
      //}
      return;
    }

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
