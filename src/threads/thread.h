#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "fixed_point.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
//在这里定义thread的结构，可以有限制地增加相应的成员函数
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. *///这个是thread_get_priority()得到的优先级（表征的优先级）的状态
    int64_t sleep_ticks;                //记录该线程被阻塞得时间，在creat的时候初始化,这里的逻辑是，只有当调用sleep的时候，才有可能大于0，而其他时候以及用非sleep调用block时候，始终保持0，所以>0可以判别是否是block的状态；并且unblock应该重置为0
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    //每个线程包含一个list_element成员，以辅助它们实现在队列中的操作，因为pintos本身栈的维护方式，所以可以直接根据list_elem的地址通过取整得到该thread的初始位置
    struct list_elem elem;             /* List element. */

	//指向被锁住的线程 注意到只可能被一个锁锁住，而反过来一个线程可以拥有多个锁，且可以处于被锁且拥有锁的状态：嵌套
	struct lock* waiting_lock;
	//一个线程可以拥有多个锁
	struct list locks;
	//当一个lock也不附加的时候处于的优先级状态
	int meta_priority;
	//一个关键的原理在于，每次获得因为lock而上升优先级的时候，被锁住的线程不会再有任何操作（不会在被lock的时候set）
	//释放的时候同理，所以只需要建立每个lock对thread的影响，而不需要查询整个chain
	//所以每个时刻的priority是max(meta-priority,lock's max priority)
	//每次release的时候重新set即可，set的策略如上
	//每次acquire未成的时候对lock链接到的holder进行修改优先级
	//需要用到递归逐层更改，直到没有lock或者即使是lock但是优先级不比某个thread priority更大得程度
	//实际上分为两个阶段1.在真正得到锁之前，要做相关的donate2.得到锁之后，更新锁的所属状态并重新更新优先级
	//每个阶段都做三件事1.更新相关的锁的状态,包括每个线程对锁的占有状态2.更新相关的优先级状态
	//3.对于就绪态的改变，进行readylist的modify;对于运行态的改变，执行yield重新调度。这三个过程应该顺序发生
    //所有涉及到优先级列表操作的地方禁止中断

	int nice;                           /* Niceness. 继承自它的parent thread*/
	fixed_t recent_cpu;                 /* Recent CPU. 每个线程都有一份，继承自它的parent thread*/

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);
void check_sleep_ticks(struct thread *,void *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);
void thread_gold_set_priority(int);
bool thread_compare_priority(const struct list_elem *elem_new,const struct list_elem *elem_in_list, void * aux);//注意要和list_less_fun的写法完全相同

void thread_update_priority(struct thread* t);
void thread_gold_update_priority(struct thread* t);
void thread_modify_ready_list(struct thread* t);
void thread_hold_the_lock(struct lock* lock);
void thread_donate_priority(struct thread* t);
void thread_remove_lock(struct lock* lock);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

int thread_count_ready(void);
void thread_mlfqs_update_priority(struct thread* t, void* aux UNUSED);
void thread_mlfqs_update_priority_every_four_ticks(void);
void thread_mlfqs_update_recent_cpu_per_second(void);
void thread_mlfqs_update_recent_cpu(struct thread* t,void * aux UNUSED);
void thread_mlfqs_update_recent_cpu_for_current_thread_per_tick(void);
void thread_mlfqs_update_load_avg_per_second(void);

/*
注意 
1.idle不算作ready_threads，也不参与priority和recent cpu的更新
2.每四个ticks的时候虽然更新优先级但是并不重新调度
3.注意timer_interrupt中函数的执行顺序，先修改recent_cpu然后再执行相关的priority计算

*/


#endif /* threads/thread.h */
