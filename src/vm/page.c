#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

/*
在实现之前请认真阅读process.c，frame.c和swap.c的逻辑
*/

/* Maximum size of process stack, in bytes. */
/* Right now it is 1 megabyte. */
#define STACK_MAX (1024 * 1024)

/*write codes here and you need to implement following functions*/

/*destroy一个page，检查是否是当前process的page
*/
static void
destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  
}

/*write codes here and you need to implement following functions*/
/* Destroys the current process's page table，利用hash_destory函数 */
void
page_exit (void)
{
}

/*write codes here and you need to implement following functions*/
/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
//返回一个包含虚拟地址addr的页面，如果这个虚拟地址不在栈的增长空间之中则重新分配
/*
1.判断当address小与phys_base时：
2.利用pg_ruond_down找到当前的页面初始地址
3.根据hash_find函数查找当前页面是否在thread的页表之中（这里再次提示，进程的页表是用hash表组织的，而不是一个数组）
4.如果返回的结果不是null说明是一个真实分配过的页面，所以直接返回相应的查询结果
5.否则检查是否是栈增长超过一个页面，需要重新分配：决定是否是访问栈 来决定是否重新分配一个页面（首先没有超过栈的界限 其次是在栈指针的向下32byte范围之内，因为push的最大长度是32）
*/
static struct page *
page_for_addr (const void *address)
{
  
  return NULL;
}
/*write codes here and you need to implement following functions*/
/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
/*
考虑四种情况：从交换区获取数据；从文件中得到数据；分配全0页面
*/
static bool
do_page_in (struct page *p)
{
  /* Get a frame for the page. */
  p->frame = frame_alloc_and_lock (p);
  if (p->frame == NULL)
    return false;

  /* 将已分配的物理页面填充相关内容*/
  /*write codes here and you need to implement following functions*/

  //从交换区中得到数据 /*write codes here and you need to implement following functions*/
  if (p->sector != (block_sector_t) -1)
    {

    }
  //从文件中得到数据
  /*write codes here and you need to implement following functions*/
  else if (p->file != NULL)
    {
      
    }
  else//全0页面 /*write codes here and you need to implement following functions*/
    {
    }

  return true;
}

/*write codes here and you need to implement following functions*/
/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
//如果是false的话按照和lab2中的相同的方式处理并返回，如果是访问一个合理的位置的pagefault的话 利用do page in函数重新分配一个页面
//检查是否分配过一个虚拟页面的方式，用page_for_addr函数检查，如果返回null则表明是一个不能访问的页面，返回fault
//否则利用do page in函数分配物理页面
//注意过程中要利用frame lock上锁
bool
page_in (void *fault_addr)
{
  struct page *p;
  bool success;

  /* santy check */
  if (thread_current ()->pages == NULL)
    return false;

  //先用page for addr检查
  p = page_for_addr (fault_addr);
  if (p == NULL)
    return false;

  /*write codes here and you need to implement following functions*/
  

  return success;
}


/*write codes here and you need to implement following functions*/

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */

/*
tips：要考虑dirty的情况写回
并且要分情况讨论是写回交换区还是文件
只有在p->private为false且对应一个file的时候才写回文件，否则利用swap写回交换区
*/
bool
page_out (struct page *p)
{
  bool dirty;
  bool ok = false;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  //这一步是为了防止另一个进程正在写这一页面而产生race
  pagedir_clear_page(p->thread->pagedir, (void *) p->addr);

  //如果dirty要写回
  dirty = pagedir_is_dirty (p->thread->pagedir, (const void *) p->addr);

  /* If the frame is not dirty (and file != NULL), we have sucsessfully evicted the page. */
  if(!dirty)
    ok = true;

  //如果没有对应的file则需要写到交换区
  /*write codes here and you need to implement following functions*/


  //如果对应一个file，则要根据是否private选择写回磁盘或者写入交换区
  /*write codes here and you need to implement following functions*/
 
  return true;//并非一定返回true，应该返回某个状态
}

/* Returns true if page P's data has been accessed recently,
   false otherwise.
   P must have a frame locked into memory. */
bool
page_accessed_recently (struct page *p)
{
  bool was_accessed;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  was_accessed = pagedir_is_accessed (p->thread->pagedir, p->addr);
  if (was_accessed)
    pagedir_set_accessed (p->thread->pagedir, p->addr, false);
  return was_accessed;
}

/* 分配一个虚拟页面，也就是在supplemental page table中设置好相应的页表项 */
struct page *
page_allocate (void *vaddr, bool read_only)
{
  struct thread *t = thread_current ();
  struct page *p = malloc (sizeof *p);
  //malloc一个页表项，并作初始化
  if (p != NULL)
    {
      p->addr = pg_round_down (vaddr);

      p->read_only = read_only;
      p->private = !read_only;

      p->frame = NULL;

      p->sector = (block_sector_t) -1;

      p->file = NULL;
      p->file_offset = 0;
      p->file_bytes = 0;

      p->thread = thread_current ();

      if (hash_insert (t->pages, &p->hash_elem) != NULL)
        {
          /* Already mapped. */
          free (p);
          p = NULL;
        }
    }
  return p;//返回allocate的页表指针p
}

/* evict掉包含vaddr的页面 */
void
page_deallocate (void *vaddr)
{
  struct page *p = page_for_addr (vaddr);
  ASSERT (p != NULL);
  frame_lock (p);
  if (p->frame)
    {
      struct frame *f = p->frame;
      if (p->file && !p->private)
        page_out (p);
      frame_free (f);
    }
  hash_delete (thread_current ()->pages, &p->hash_elem);
  free (p);
}

/* Returns a hash value for the page that E refers to. */
unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return ((uintptr_t) p->addr) >> PGBITS;
}

/* Returns true if page A precedes page B. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}

/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
bool
page_lock (const void *addr, bool will_write)
{
  struct page *p = page_for_addr (addr);
  if (p == NULL || (p->read_only && will_write))
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    return (do_page_in (p)
            && pagedir_set_page (thread_current ()->pagedir, p->addr,
                                 p->frame->base, !p->read_only));
  else
    return true;
}

/* Unlocks a page locked with page_lock(). */
void
page_unlock (const void *addr)
{
  struct page *p = page_for_addr (addr);
  ASSERT (p != NULL);
  frame_unlock (p->frame);
}
