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
��ʵ��֮ǰ�������Ķ�process.c��frame.c��swap.c���߼�
*/

/* Maximum size of process stack, in bytes. */
/* Right now it is 1 megabyte. */
#define STACK_MAX (1024 * 1024)

/*write codes here and you need to implement following functions*/

/*destroyһ��page������Ƿ��ǵ�ǰprocess��page
*/
static void
destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  
}

/*write codes here and you need to implement following functions*/
/* Destroys the current process's page table������hash_destory���� */
void
page_exit (void)
{
}

/*write codes here and you need to implement following functions*/
/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
//����һ�����������ַaddr��ҳ�棬�����������ַ����ջ�������ռ�֮�������·���
/*
1.�жϵ�addressС��phys_baseʱ��
2.����pg_ruond_down�ҵ���ǰ��ҳ���ʼ��ַ
3.����hash_find�������ҵ�ǰҳ���Ƿ���thread��ҳ��֮�У������ٴ���ʾ�����̵�ҳ������hash����֯�ģ�������һ�����飩
4.������صĽ������null˵����һ����ʵ�������ҳ�棬����ֱ�ӷ�����Ӧ�Ĳ�ѯ���
5.�������Ƿ���ջ��������һ��ҳ�棬��Ҫ���·��䣺�����Ƿ��Ƿ���ջ �������Ƿ����·���һ��ҳ�棨����û�г���ջ�Ľ��� �������ջָ�������32byte��Χ֮�ڣ���Ϊpush����󳤶���32��
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
��������������ӽ�������ȡ���ݣ����ļ��еõ����ݣ�����ȫ0ҳ��
*/
static bool
do_page_in (struct page *p)
{
  /* Get a frame for the page. */
  p->frame = frame_alloc_and_lock (p);
  if (p->frame == NULL)
    return false;

  /* ���ѷ��������ҳ������������*/
  /*write codes here and you need to implement following functions*/

  //�ӽ������еõ����� /*write codes here and you need to implement following functions*/
  if (p->sector != (block_sector_t) -1)
    {

    }
  //���ļ��еõ�����
  /*write codes here and you need to implement following functions*/
  else if (p->file != NULL)
    {
      
    }
  else//ȫ0ҳ�� /*write codes here and you need to implement following functions*/
    {
    }

  return true;
}

/*write codes here and you need to implement following functions*/
/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
//�����false�Ļ����պ�lab2�е���ͬ�ķ�ʽ�������أ�����Ƿ���һ�������λ�õ�pagefault�Ļ� ����do page in�������·���һ��ҳ��
//����Ƿ�����һ������ҳ��ķ�ʽ����page_for_addr������飬�������null�������һ�����ܷ��ʵ�ҳ�棬����fault
//��������do page in������������ҳ��
//ע�������Ҫ����frame lock����
bool
page_in (void *fault_addr)
{
  struct page *p;
  bool success;

  /* santy check */
  if (thread_current ()->pages == NULL)
    return false;

  //����page for addr���
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
tips��Ҫ����dirty�����д��
����Ҫ�����������д�ؽ����������ļ�
ֻ����p->privateΪfalse�Ҷ�Ӧһ��file��ʱ���д���ļ�����������swapд�ؽ�����
*/
bool
page_out (struct page *p)
{
  bool dirty;
  bool ok = false;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  //��һ����Ϊ�˷�ֹ��һ����������д��һҳ�������race
  pagedir_clear_page(p->thread->pagedir, (void *) p->addr);

  //���dirtyҪд��
  dirty = pagedir_is_dirty (p->thread->pagedir, (const void *) p->addr);

  /* If the frame is not dirty (and file != NULL), we have sucsessfully evicted the page. */
  if(!dirty)
    ok = true;

  //���û�ж�Ӧ��file����Ҫд��������
  /*write codes here and you need to implement following functions*/


  //�����Ӧһ��file����Ҫ�����Ƿ�privateѡ��д�ش��̻���д�뽻����
  /*write codes here and you need to implement following functions*/
 
  return true;//����һ������true��Ӧ�÷���ĳ��״̬
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

/* ����һ������ҳ�棬Ҳ������supplemental page table�����ú���Ӧ��ҳ���� */
struct page *
page_allocate (void *vaddr, bool read_only)
{
  struct thread *t = thread_current ();
  struct page *p = malloc (sizeof *p);
  //mallocһ��ҳ���������ʼ��
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
  return p;//����allocate��ҳ��ָ��p
}

/* evict������vaddr��ҳ�� */
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
