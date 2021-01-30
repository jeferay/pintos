#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
//对于donate的机制，锁不只是要保有自身的信息，还要拥有关于chain的信息
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
	int chain_priority;//保持链上最大的优先级,之所以要维护这个成员变量是因为每个线程可能拥有多个锁
	struct list_elem elem;//因为一个thread要维护一个lock 的list所以要加入这个元素以便于操作
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
void lock_gold_acquire(struct lock* lock);
bool lock_try_acquire (struct lock *);
void lock_gold_release (struct lock *);
void lock_release(struct lock* lock);
bool lock_held_by_current_thread (const struct lock *);
//比较锁目前的chain_priority
//该函数用于max的时候
bool lock_max_compare_priority(const struct list_elem* a,const struct list_elem* b,void* aux);
//该函数用于sort的时候
bool lock_compare_priority(const struct list_elem* a, const struct list_elem* b, void* aux);
/* Condition variable. */
//只需要包含一个等待队列
//condvar：条件变量,应用的场景是等待某一个条件合适为止，被唤醒而可以执行
//如果没有这个同步机制而只有锁，那么当一个线程等待另一个线程的时候只能持续的加锁再释放锁并检查，这样子很占用CPU；
//如果可以在等待条件的时候被block，条件好了的时候再被唤醒，那么就可以节约掉这部分的开销
//所以引入一个wait,当条件满足的时候，由原先的thread执行signal唤醒等待的thread
//因为可能不只有一个thread在等待，同样是要维护一个等待队列
//注意到条件变量的实现要结合锁才可以，是因为当一个thread要更改某个条件变量的状态的时候，要防止在wait之前就已经修改了，那样将永远无法唤醒
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
bool cond_sema_cmp_priority(const struct list_elem* a, const struct list_elem* b, void* aux);
void cond_signal (struct condition *, struct lock *);
void cond_my_signal(struct condition* cond, struct lock* lock);
void cond_broadcast (struct condition *, struct lock *);



/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
