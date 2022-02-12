// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "proc.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

struct page pages[PHYSTOP/PGSIZE];
struct page *page_lru_head;
int num_free_pages;
int num_lru_pages;

char bitmap[SWAPMAX / 8];
void clearBitmap(int i){
  bitmap[i] = '0';
}

void page_insert(pde_t *pgdir, char* va){
  if(num_lru_pages == 0){
    page_lru_head = &pages[0];

    pages[0].vaddr = va;
    pages[0].pgdir = pgdir;
    pages[0].prev = page_lru_head;
    pages[0].next = page_lru_head;

    num_free_pages--;
    num_lru_pages++;
    //cprintf("inserted!! lru: %d va: %x pgdir: %x\n", num_lru_pages, va, pgdir);
  }
  else{
    int index = 0;
    struct page *cur = page_lru_head;
    while(cur->next != page_lru_head){
      if(cur->vaddr == va && cur->pgdir==pgdir) return;
       cur = cur->next;
    }

    while(pages[index].next!=0){
      index++;
    }
    
    pages[index].pgdir = pgdir;
    pages[index].vaddr = va;
    pages[index].next = page_lru_head;
    
    pages[index].prev = cur;
    cur->next = &pages[index];

    page_lru_head->prev = &pages[index];
    
    num_free_pages--;
    num_lru_pages++;
    //cprintf("inserted!! lru: %d va: %x pgdir: %x\n", num_lru_pages, va, pgdir);
  }
}

void page_delete(pde_t *pgdir, char* va){
  if(num_lru_pages==0) return;
  
  struct page *cur;
  for(cur=page_lru_head; (cur->pgdir!=pgdir || cur->vaddr !=va);){
    if(cur->next==page_lru_head) return;
    cur = cur->next;
  }
  if(cur==page_lru_head) return;

  // int i=0;
  // struct page *cur;
  // while(pages[i].vaddr!=va && pages[i].pgdir!=pgdir){
  //   if(i==num_lru_pages) return;
  //   i++;
  // }
  // cur = &pages[i];
  cur->next->prev = cur->prev;
  cur->prev->next = cur->next;

  cur->prev = 0;
  cur->next = 0;
  cur->vaddr = 0;
  cur->pgdir = 0;

// int index=0;
// while(pages[index].next != cur) index++;
// index++;
// pages[index].vaddr = 0;
// pages[index].pgdir = 0;
// pages[index].next = 0;
// pages[index].prev = 0;

  num_free_pages++;
  num_lru_pages--;
}

int reclaim(){
  //evict page;
  //cprintf("----------------------------------------------\n");
  pde_t *pgdir = page_lru_head->pgdir;
  char *va = page_lru_head->vaddr;

  page_lru_head = page_lru_head->next; //send to tail of LRU

  pte_t *pte;
  uint pa;
  pte = walkpgdir(pgdir, va, 0);

  int j;
  for(j=0; j<PHYSTOP/PGSIZE; j++){
    if(pages[j].vaddr == va && pages[j].pgdir == pgdir) break;
  }

  //cprintf("pass va: %x, j: %d PTEA: %x PTEU: %x\n", va, j, *pte & PTE_A, *pte & PTE_U);

  if(*pte & PTE_A) {
    *pte -= PTE_A;

    //cprintf("UP lru va: %x\n", page_lru_head->vaddr);
    if(kmem.use_lock)
      release(&kmem.lock);
    //cprintf("----------------------------------------------\n");
    return 1;
  }
  else {
    struct page* cur = page_lru_head->prev;
    page_lru_head->prev = cur->prev;
    cur->prev->next = page_lru_head;
    
    cur->vaddr = 0;
    cur->pgdir = 0;
    cur->next = 0;
    cur->prev = 0;

    
    //cprintf("DOWN lru va: %x\n", page_lru_head->vaddr);
    pa = PTE_ADDR(*pte);
    //cprintf("DOWN lru va: %x\n", page_lru_head->vaddr);
    if(kmem.use_lock)
      release(&kmem.lock);
    
    int offset = PDX(va) + PTX(va) + ((uint)va & 0xfff);
    bitmap[offset]='1';
    swapwrite(P2V(pa), offset);
    //cprintf("DOWN lru va: %d\n", offset);

    //*pte = i<<1;
    *pte &= ~PTE_P;
    *pte |= PTE_U;
    //cprintf("%dth table PA %x, VA %x is cleared!\n", j, pa, va);
    kfree(P2V(pa));
    
    //cprintf("%dth table PA %x, VA %x is cleared!\n", j, pa, va);
    //cprintf("----------------------------------------------\n");
    return 1;
  }
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);

  num_lru_pages = 0;
  num_free_pages = (PGROUNDDOWN((uint)vend) - PGROUNDUP((uint)vstart)) / PGSIZE;
  for(int i=0; i<SWAPMAX / 8; i++){
    bitmap[i] = '0';
  }
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);

  num_free_pages += (PGROUNDDOWN((uint)vend) - PGROUNDUP((uint)vstart)) / PGSIZE;
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");
  
  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  
  r = (struct run*)v;
  
  r->next = kmem.freelist;
  kmem.freelist = r;
  
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

try_again:
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  //cprintf("CREATE %x\n", r);
  if(!r){
    if(num_lru_pages == 0){
      cprintf("Out Of Memory!\n");
      return 0;
    }
    if(reclaim()){
      goto try_again;
    }
  }
  if(r){
    kmem.freelist = r->next;
  }
  if(kmem.use_lock)
    release(&kmem.lock);

  return (char*)r;
}