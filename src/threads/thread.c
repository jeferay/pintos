#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
//我们的核心是要将该list维护为一个优先队列，然而优先队列的实现较为复杂，且list.c之中已经提供了相关的排序函数等等
//那么我们可以更进一步直接实现为有序队列，而维护有序队列只需要在每次insert的时候做相关的维护工作即可
//对ready list插入的位置一共有三处，分别是thread_unblock,init_thread和thread_yield,这三处都是转化为就绪态
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
//闲置的thread
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;//表明是否使用多级反馈队列调度
/*
threads no longer directly control their own priorities. 
The priority argument to thread_create() should be ignored, as well as any calls to thread_set_priority(), 
and thread_get_priority() should return the thread's current priority as set by the scheduler.
*/

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

fixed_t load_avg = 0;//维护一个全局变量,初始化为0

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
//被main调用以创造一个初始的系统线程,这个和init_thread()是不同的，后者是在create时候初始化相关的值的
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);//start的thread即为idle
  
  load_avg = FP_CONST(0);//初始化

  /* Start preemptive thread scheduling. *///在creat之后再允许中断
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
//处于外部中断的信号处理程序
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t//这个函数的作用是在创建一个新的非系统线程的时候，初始化相应的变量并设定相应的线程状态
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  //应该按照优先级重新调度，t并不是cur,而是新create出来的
  thread_unblock (t);
  if (thread_current()->priority < t->priority)//这里体现了抢占
    thread_yield();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //list_push_back (&ready_list, &t->elem);
  list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, NULL);
  t->status = THREAD_READY;
  t->sleep_ticks = 0;//将sleep重置为0
  intr_set_level (old_level);
}

void
check_sleep_ticks(struct thread *t,void *aux UNUSED)//这个aux是不需要的，所以调用的时候，参数是null即可
{
  if(t->status==THREAD_BLOCKED && t->sleep_ticks >0)
  {
    t->sleep_ticks -=1;
    if (t->sleep_ticks == 0)
    {
      thread_unblock(t);//此时解除t的block状态,并变为就绪态
    }
  }
return;
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
//其实就是先返回running thread的起始位置指针然后做两步合理性检查
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();//得到当前thread的struct地址
  enum intr_level old_level;
  
  ASSERT (!intr_context ());//不处于处理外部中断的进程之中

  old_level = intr_disable ();//返回当前中断状态并且不允许中断
  if (cur != idle_thread) //如果当前不是闲置的线程，那么加入到就绪态ready_list之中
    //list_push_back (&ready_list, &cur->elem);
    list_insert_ordered(&ready_list,&cur->elem,thread_compare_priority,NULL);
  cur->status = THREAD_READY;//当前变为就绪态
  schedule ();//重新调度
  intr_set_level (old_level);//恢复终端状态
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
//对每个线程执行thread_action_fun的函数，aux是传给这个fun的参数,注意到：fun的第一个参数是线程t
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
//调用该函数的一定已经是current_thread了
//这里需要改变，因为设置的时候可能处于donate的状态，那么就不能直接设置priority,而是应该比较之后再考虑
void
thread_set_priority (int new_priority) 
{
	if (thread_mlfqs)return;//ignore
	enum intr_level old_level = intr_disable();
	struct thread* cur = thread_current();
	cur->meta_priority = new_priority;
	thread_update_priority(cur);//无论是哪种情况，都先要更改meta,然后只要再依据现状做priority的update即可，已经在update的时候考虑了区分mlfqs
	thread_yield();//立刻重新调度（根据priority-change的test结果）
	intr_set_level(old_level);
}

void
thread_gold_set_priority(int new_priority)
{
	if (thread_mlfqs)
		return;

	enum intr_level old_level = intr_disable();

	struct thread* current_thread = thread_current();
	int old_priority = current_thread->priority;
	current_thread->meta_priority = new_priority;

	if (list_empty(&current_thread->locks) || new_priority > old_priority)
	{
		current_thread->priority = new_priority;
		thread_yield();
	}

	intr_set_level(old_level);
}
/* Returns the current thread's priority. */
//如果具有donated的情况，返回更高的donated priority,因为我们将priority设置为表征出来的优先级，所以这个函数不需要改边
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

//这个函数涉及到了struct thread，所以只能定义在thread.c并在thread.h中声明
bool 
thread_compare_priority(const struct list_elem *elem_new, const struct list_elem *elem_in_list, void * aux UNUSED)//实际aux是没有用的 传入null即可
{
  int priority_new,priority_in_list;
  priority_new = list_entry(elem_new,struct thread,elem)->priority;
  priority_in_list =  list_entry(elem_in_list,struct thread,elem)->priority;
  return priority_new > priority_in_list;
}
//这个是在更新对应锁状态之后做优先级更新的函数，原理是从meta_priority和所有locks的chain_priority之中选择最大的
void 
thread_update_priority(struct thread* t)
{
	enum intr_level old_level = intr_disable();//涉及到链表的操作，阻塞中断

	int max_priority = t->meta_priority;
	

	if (!list_empty(&t->locks))//只有在不是mlfqs的时候才考虑donate的情况
	{
		struct list_elem *lock_elem = list_max(&t->locks, lock_max_compare_priority, NULL);
		max_priority = list_entry(lock_elem, struct lock, elem)->chain_priority;//获取locks上的最大贡献的优先级
	}
	if (t->meta_priority > max_priority)//和meta作比较
		max_priority = t->meta_priority;

	t->priority = max_priority;
	intr_set_level(old_level);
}

/* Update priority. */
void
thread_gold_update_priority(struct thread* t)
{
	enum intr_level old_level = intr_disable();
	int max_priority = t->meta_priority;
	int lock_priority;

	if (!list_empty(&t->locks))
	{
		list_sort(&t->locks, lock_compare_priority, NULL);
		lock_priority = list_entry(list_front(&t->locks), struct lock, elem)->chain_priority;
		if (lock_priority > max_priority)
			max_priority = lock_priority;
	}

	t->priority = max_priority;
	intr_set_level(old_level);
}

//正确
void 
thread_modify_ready_list(struct thread* t)
{
	if (t->status != THREAD_READY)//如果不是就绪态不做执行
		return;
	enum intr_level old_level = intr_disable();
	list_remove(&t->elem);
	list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, NULL);
	intr_set_level(old_level);
}

/* Let thread hold a lock */
void
thread_hold_the_lock(struct lock* lock)
{
	enum intr_level old_level = intr_disable();
	list_insert_ordered(&thread_current()->locks, &lock->elem, lock_compare_priority, NULL);
	/*
	if (lock->chain_priority > thread_current()->priority)
	{
		thread_current()->priority = lock->chain_priority;
		thread_yield();
	}
	*/
	intr_set_level(old_level);
}

/* Donate current priority to thread t. */
void
thread_donate_priority(struct thread* t)
{
	enum intr_level old_level = intr_disable();
	thread_update_priority(t);

	if (t->status == THREAD_READY)
	{
		list_remove(&t->elem);
		list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, NULL);
	}
	intr_set_level(old_level);
}

/* Remove a lock. */
void
thread_remove_lock(struct lock* lock)
{
	enum intr_level old_level = intr_disable();
	list_remove(&lock->elem);
	thread_update_priority(thread_current());
	intr_set_level(old_level);
}


/* Sets the current thread's nice value to NICE. */
void
thread_set_nice(int nice)
{
	struct thread* cur = thread_current();
	cur->nice = nice;
	thread_mlfqs_update_priority(cur, NULL);
	struct list_elem* t_elem = list_min(&ready_list, thread_compare_priority, NULL);//ready_list中优先级最大的元素，min自洽
	int max_priority = list_entry(t_elem, struct thread, elem)->priority;
	if (max_priority > cur->priority)
		thread_yield();//存在更高优先级，yield

}

/* Returns the current thread's nice value. */
int
thread_get_nice(void)
{
	return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg(void)
{
	return FP_ROUND(FP_MULT_MIX(load_avg, 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu(void)
{
	return FP_ROUND(FP_MULT_MIX(thread_current()->recent_cpu, 100));
}


/* Idle thread.  Executes when no other thread is ready to run.当没有其他线程在运行的时候才会run

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}


/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));//这一步是将esp寄存器的值复制到栈之中
  return pg_round_down (esp);//根据pintos这个操作系统的设置，一个线程的页面大小为1<<12bits,也就是4kb
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);//这一步其实已经初始化为0了
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  if (!thread_mlfqs)
  {
	  t->priority = priority;
	  t->meta_priority = priority;
  }
  
  list_init(&t->locks);
  t->waiting_lock = NULL;//初始化为0不用重复进行，这里只是标志
  //t->sleep_ticks = 0;//刚开始初始化为0，然而这步不是必要的因为memset已经包含了相同的操作
  t->magic = THREAD_MAGIC;
  

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  //list_insert_ordered(&all_list,&t->allelem,thread_compare_priority,NULL);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
//因为要考虑到返回的优先级是最高的那个，因此要保证这个队列是优先队列
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))//这里设置一个优先级的问题，只有当ready是空着的时候，才可以返回idle_thread
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);//应该设置为返回优先级最高的那个
}

//这个函数的作用相当于在switch掉上下文信息之后做相关的切换收尾工作
//1.获取当前线程，非陪恢复之前执行的转台和现场（更改状态以及触发新的地址空间）
//2.如果prev是dying，回收相应的页
/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();//这里已经是被切换过的cur(当然可能和原来的cur是相同的，因为ready为空的时候，cur和next是一样的，都是idle)
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;//虽然栈指针等等已经重新设置，但是还没有更改cur状态，现在就是要更改cur状态

  /* Start new time slice. */
  //清零时间片开始新的线程切换时间片
  thread_ticks = 0;

#ifdef USERPROG //如果定义了用户程序
  /* Activate the new address space. */ //触发新的地址空间
  process_activate ();
  //1更新页目录表2更新任务现场信息
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) //我们不free掉initial thread因为它的存储获取不来自于palloc
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);//如果是dying状态，用palloc_free_page来释放这一个线程页
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();//正在运行的进程
  struct thread *next = next_thread_to_run ();//下一个要运行的进程
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);//断言已经处于阻塞中断状态
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)//如果下一个不是current执行switch
    prev = switch_threads (cur, next);//返回的其实就是被切换的线程栈指针
  //在这里已经完成了上下文的切换，那么运行running thread已经是被切换了的进程
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);




int
thread_count_ready(void)
{
	if (thread_current() == idle_thread)
		return list_size(&ready_list);
	else
		return list_size(&ready_list) + 1;//包括一个运行中的线程
}

void
thread_mlfqs_update_recent_cpu_for_current_thread_per_tick(void)
{
	struct thread* cur = thread_current();
	if (cur != idle_thread)//不考虑idle线程，其只是一个低能耗线程,不对他做更改
		cur->recent_cpu = FP_ADD_MIX(cur->recent_cpu, 1);

}

void thread_mlfqs_update_priority(struct thread* t, void* aux UNUSED)
{
	if (t == idle_thread)
		return;
	t->priority = FP_INT_PART(FP_SUB(FP_CONST(PRI_MAX), FP_ADD_MIX(FP_DIV_MIX(t->recent_cpu, 4), t->nice * 2)));
	t->priority = t->priority < PRI_MIN ? PRI_MIN : t->priority;
	t->priority = t->priority > PRI_MAX ? PRI_MAX : t->priority;
}

void 
thread_mlfqs_update_priority_every_four_ticks(void)
{
	//thread_foreach(thread_mlfqs_update_priority, NULL);
	thread_mlfqs_update_priority(thread_current(), NULL);
}

void 
thread_mlfqs_update_recent_cpu(struct thread* t,void *aux UNUSED)
{
	if (t == idle_thread)
		return;
	if (t->status == THREAD_DYING)
		return;
	t->recent_cpu = FP_ADD_MIX(FP_MULT(FP_DIV(FP_MULT_MIX(load_avg, 2), FP_ADD_MIX(FP_MULT_MIX(load_avg, 2), 1)), t->recent_cpu), t->nice);
}

void 
thread_mlfqs_update_recent_cpu_per_second(void)
{
	thread_foreach(thread_mlfqs_update_recent_cpu, NULL);
}


void 
thread_mlfqs_update_load_avg_per_second(void)
{
	load_avg = FP_ADD(FP_DIV_MIX(FP_MULT_MIX(load_avg, 59), 60), FP_DIV_MIX(FP_CONST(thread_count_ready()), 60));
}

