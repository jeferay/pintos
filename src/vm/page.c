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

/* Maximum size of process stack, in bytes. */
/* Right now it is 1 megabyte. */
#define STACK_MAX (1024 * 1024)

/* Destroys a page, which must be in the current process's
   page table.  Used as a callback for hash_destroy(). */
static void
destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  struct page *p = hash_entry (p_, struct page, hash_elem);
  frame_lock (p);
  if (p->frame)
    frame_free (p->frame);
  free (p);
}

/* Destroys the current process's page table. */
void
page_exit (void)
{
  struct hash *h = thread_current ()->pages;
  if (h != NULL)
    hash_destroy (h, destroy_page);
}

/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
//返回一个包含虚拟地址addr的页面，如果这个虚拟地址不在栈的增长空间之中则重新分配
static struct page *
page_for_addr (const void *address)
{
  if (address < PHYS_BASE)
    {
      struct page p;
      struct hash_elem *e;

      /* Find existing page. */
      p.addr = (void *) pg_round_down (address);
	  //round到附近的page
      e = hash_find (thread_current ()->pages, &p.hash_elem);
	  //如果不是null，说明已经分配过相应的虚拟页面，而只是没有分配实际的物理页面
      if (e != NULL)
        return hash_entry (e, struct page, hash_elem);
	  //并非真的维护一个页表，是用hash来实现地址到页表项的映射

	  //当发现没有分配到一个虚拟页面时，要判断是否是要为栈分配页面
      /* 决定是否是访问栈 来决定是否重新分配一个页面
         首先没有超过栈的界限
         其次是在栈指针的向下32byte范围之内，因为push的最大长度是32
      */
      if ((p.addr > PHYS_BASE - STACK_MAX) && ((void *)thread_current()->user_esp - 32 < address))
      {
        return page_allocate (p.addr, false);
      }
    }

  return NULL;
}

/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
static bool
do_page_in (struct page *p)
{
  /* Get a frame for the page. */
  p->frame = frame_alloc_and_lock (p);
  if (p->frame == NULL)
    return false;

  /* 将已分配的物理页面填充相关内容*/
  if (p->sector != (block_sector_t) -1)
    {
      //从交换区中得到数据
      swap_in (p);
    }
  else if (p->file != NULL)
    {
      //从文件中得到数据
      off_t read_bytes = file_read_at (p->file, p->frame->base,
                                        p->file_bytes, p->file_offset);
      off_t zero_bytes = PGSIZE - read_bytes;
      memset (p->frame->base + read_bytes, 0, zero_bytes);
      if (read_bytes != p->file_bytes)
        printf ("bytes read (%"PROTd") != bytes requested (%"PROTd")\n",
                read_bytes, p->file_bytes);
    }
  else
    {
      //全0页面
      memset (p->frame->base, 0, PGSIZE);
    }

  return true;
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
//如果是false的话按照和lab2中的相同的方式处理并返回，如果是访问一个合理的位置的pagefault的话
//重新分配一个页面
bool
page_in (void *fault_addr)
{
  struct page *p;
  bool success;

  /* santy check */
  if (thread_current ()->pages == NULL)
    return false;

  //p是指向分配页的指针，此时只是有了页表项但是还没有具体分配物理内存
  p = page_for_addr (fault_addr);
  if (p == NULL)
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    {
      if (!do_page_in (p))
        return false;
    }
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  /* Install frame into page table. */
  success = pagedir_set_page (thread_current ()->pagedir, p->addr,
                              p->frame->base, !p->read_only);

  /* Release frame. */
  frame_unlock (p->frame);

  return success;
}

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
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
  if (p->file == NULL)
	  ok = swap_out(p);
  //如果对应一个file，则要根据是否private选择写回磁盘或者写入交换区
  else
  {
	  if (dirty)
	  {
		  if (p->private)
			  ok = swap_out(p);
		  else
			  ok = file_write_at(p->file, (const void*)p->frame->base, p->file_bytes, p->file_offset);
	  }
  }
  if(ok)
    p->frame = NULL;//表明这一个虚拟页不对应frame

  return ok;
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
