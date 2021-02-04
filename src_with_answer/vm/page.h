#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

/* Virtual page. */
struct page 
  {
    
    void *addr;                 /* �û�����ҳ��*/
    bool read_only;             /* ��дȨ��*/
    struct thread *thread;      /* �����Ľ��� */

    
    struct hash_elem hash_elem; /*����hash�����ӳ���ϵ */

    
    struct frame *frame;        /* Page frame. */

    /* Swap information, protected by frame->frame_lock. */
    block_sector_t sector;       /* ����������ʼλ��/-1 */
    

    /* Memory-mapped file information, protected by frame->frame_lock. */
    bool private;               /* False to write back to file,
                                   true to write back to swap. */
    struct file *file;          /* File. */
    off_t file_offset;          /* �ļ��е�ƫ���� */
    off_t file_bytes;           /* ��д�ļ���byte���� */
  };

void page_exit (void);

struct page *page_allocate (void *, bool read_only);
void page_deallocate (void *vaddr);

bool page_in (void *fault_addr);
bool page_out (struct page *);
bool page_accessed_recently (struct page *);

bool page_lock (const void *, bool will_write);
void page_unlock (const void *);

hash_hash_func page_hash;
hash_less_func page_less;

#endif /* vm/page.h */
