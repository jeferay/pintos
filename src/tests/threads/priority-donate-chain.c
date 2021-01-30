/* The main thread set its priority to PRI_MIN and creates 7 threads 
   (thread 1..7) with priorities PRI_MIN + 3, 6, 9, 12, ...
   The main thread initializes 8 locks: lock 0..7 and acquires lock 0.

   When thread[i] starts, it first acquires lock[i] (unless i == 7.)
   Subsequently, thread[i] attempts to acquire lock[i-1], which is held by
   thread[i-1], except for lock[0], which is held by the main thread.
   Because the lock is held, thread[i] donates its priority to thread[i-1],
   which donates to thread[i-2], and so on until the main thread
   receives the donation.

   After threads[1..7] have been created and are blocked on locks[0..7],
   the main thread releases lock[0], unblocking thread[1], and being
   preempted by it.
   Thread[1] then completes acquiring lock[0], then releases lock[0],
   then releases lock[1], unblocking thread[2], etc.
   Thread[7] finally acquires & releases lock[7] and exits, allowing 
   thread[6], then thread[5] etc. to run and exit until finally the 
   main thread exits.

   In addition, interloper threads are created at priority levels
   p = PRI_MIN + 2, 5, 8, 11, ... which should not be run until the 
   corresponding thread with priority p + 1 has finished.
  
   Written by Godmar Back <gback@cs.vt.edu> */ 

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define NESTING_DEPTH 8

struct lock_pair
  {
    struct lock *second;
    struct lock *first;
  };

static thread_func donor_thread_func;
static thread_func interloper_thread_func;

//实质上依然是测试nest的情况，注意release lock之后被抢占
void
test_priority_donate_chain (void) 
{
  int i;  
  struct lock locks[NESTING_DEPTH - 1];
  struct lock_pair lock_pairs[NESTING_DEPTH];

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  thread_set_priority (PRI_MIN);

  for (i = 0; i < NESTING_DEPTH - 1; i++)
    lock_init (&locks[i]);

  lock_acquire (&locks[0]);
  msg ("%s got lock.", thread_name ());

  for (i = 1; i < NESTING_DEPTH; i++)//新创建depth-1个线程（7个）
    {
      char name[16];
      int thread_priority;

      snprintf (name, sizeof name, "thread %d", i);//执行的是复制功能
      thread_priority = PRI_MIN + i * 3;
      lock_pairs[i].first = i < NESTING_DEPTH - 1 ? locks + i: NULL;//与标号相同
      lock_pairs[i].second = locks + i - 1;//比标号减一

      thread_create (name, thread_priority, donor_thread_func, lock_pairs + i);
      msg ("%s should have priority %d.  Actual priority: %d.",
          thread_name (), thread_priority, thread_get_priority ());

      snprintf (name, sizeof name, "interloper %d", i);
      thread_create (name, thread_priority - 1, interloper_thread_func, NULL);//因为优先级较低，所以暂时不执行
    }

  lock_release (&locks[0]);
  msg ("%s finishing with priority %d.", thread_name (),
                                         thread_get_priority ());
}

static void
donor_thread_func (void *locks_) 
{
  struct lock_pair *locks = locks_;

  if (locks->first)//所有的线程都先得到与之对应的first lock
    lock_acquire (locks->first); 

  lock_acquire (locks->second);//每个线程的first同时是前一个线程的second
  msg ("%s got lock", thread_name ());

  lock_release (locks->second);//得到后也就释放了，作为后续操作
  msg ("%s should have priority %d. Actual priority: %d", 
        thread_name (), (NESTING_DEPTH - 1) * 3,
        thread_get_priority ());

  if (locks->first)//释放first,那么下一个线程可以得到second,因为优先级下降，所以被立刻调度
    lock_release (locks->first);

  msg ("%s finishing with priority %d.", thread_name (),
                                         thread_get_priority ());
}

static void
interloper_thread_func (void *arg_ UNUSED)
{
  msg ("%s finished.", thread_name ());
}

// vim: sw=2
