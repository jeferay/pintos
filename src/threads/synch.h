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
//����donate�Ļ��ƣ�����ֻ��Ҫ�����������Ϣ����Ҫӵ�й���chain����Ϣ
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
	int chain_priority;//���������������ȼ�,֮����Ҫά�������Ա��������Ϊÿ���߳̿���ӵ�ж����
	struct list_elem elem;//��Ϊһ��threadҪά��һ��lock ��list����Ҫ�������Ԫ���Ա��ڲ���
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
void lock_gold_acquire(struct lock* lock);
bool lock_try_acquire (struct lock *);
void lock_gold_release (struct lock *);
void lock_release(struct lock* lock);
bool lock_held_by_current_thread (const struct lock *);
//�Ƚ���Ŀǰ��chain_priority
//�ú�������max��ʱ��
bool lock_max_compare_priority(const struct list_elem* a,const struct list_elem* b,void* aux);
//�ú�������sort��ʱ��
bool lock_compare_priority(const struct list_elem* a, const struct list_elem* b, void* aux);
/* Condition variable. */
//ֻ��Ҫ����һ���ȴ�����
//condvar����������,Ӧ�õĳ����ǵȴ�ĳһ����������Ϊֹ�������Ѷ�����ִ��
//���û�����ͬ�����ƶ�ֻ��������ô��һ���̵߳ȴ���һ���̵߳�ʱ��ֻ�ܳ����ļ������ͷ�������飬�����Ӻ�ռ��CPU��
//��������ڵȴ�������ʱ��block���������˵�ʱ���ٱ����ѣ���ô�Ϳ��Խ�Լ���ⲿ�ֵĿ���
//��������һ��wait,�����������ʱ����ԭ�ȵ�threadִ��signal���ѵȴ���thread
//��Ϊ���ܲ�ֻ��һ��thread�ڵȴ���ͬ����Ҫά��һ���ȴ�����
//ע�⵽����������ʵ��Ҫ������ſ��ԣ�����Ϊ��һ��threadҪ����ĳ������������״̬��ʱ��Ҫ��ֹ��wait֮ǰ���Ѿ��޸��ˣ���������Զ�޷�����
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
