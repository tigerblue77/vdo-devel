/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef UDS_THREADS_H
#define UDS_THREADS_H

#include <linux/atomic.h>
#ifdef __KERNEL__
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include "event-count.h"
#else
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdbool.h>
#endif

#include "compiler.h"
#include "errors.h"
#include "time-utils.h"

#ifdef __KERNEL__
struct cond_var {
	struct event_count *event_count;
};
struct thread;

struct barrier {
	struct semaphore mutex; /* Mutex for this barrier object */
	struct semaphore wait;  /* Semaphore for threads waiting at the barrier */
	int arrived;            /* Number of threads which have arrived */
	int thread_count;       /* Total number of threads using this barrier */
};
#else
struct cond_var {
	pthread_cond_t condition;
};

struct mutex {
	pthread_mutex_t mutex;
};

struct semaphore {
	sem_t semaphore;
};

struct thread {
	pthread_t thread;
};

struct barrier {
	pthread_barrier_t barrier;
};

#ifndef NDEBUG
#define UDS_MUTEX_INITIALIZER PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
#define UDS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

extern const bool UDS_DO_ASSERTIONS;
#endif

#ifdef __KERNEL__
#ifdef TEST_INTERNAL
/**
 * Apply a function to every thread that we have created.
 *
 * @param apply_func  The function to apply
 * @param argument    The first argument to apply_func
 *
 **/
void uds_apply_to_threads(void apply_func(void *, struct task_struct *),
			  void *argument);
#endif  /* TEST_INTERNAL */
#endif  /* __KERNEL__ */

/**
 * Create a thread, logging any cause of failure.
 *
 * @param thread_func  function to run in new thread
 * @param thread_data  private data for new thread
 * @param name         name of the new thread
 * @param new_thread   where to store the new thread id
 *
 * @return       success or failure indication
 **/
int __must_check uds_create_thread(void (*thread_func)(void *),
				   void *thread_data,
				   const char *name,
				   struct thread **new_thread);

/**
 * Retrieve the current numbers of cores.
 *
 * This is either the total number or the number of cores that this
 * process has been limited to.
 *
 * @return      number of cores
 **/
unsigned int uds_get_num_cores(void);

/**
 * Return the id of the current thread.
 *
 * @return the thread id
 **/
pid_t __must_check uds_get_thread_id(void);

#ifndef __KERNEL__
/**
 * Get the name of the current thread.
 *
 * @param name   a buffer of size at least 16 to write the name to
 **/
void uds_get_thread_name(char *name);
#endif

/**
 * Thread safe once only initialization.
 *
 * @param once_state     pointer to object to record that initialization
 *                       has been performed
 * @param init_function  called if once_state does not indicate
 *                       initialization has been performed
 *
 * @note Generally the following declaration of once_state is performed in
 *       at file scope:
 *
 *       static atomic_t once_state = ATOMIC_INIT(0);
 **/
void perform_once(atomic_t *once_state, void (*init_function) (void));

/**
 * Wait for termination of another thread.
 *
 *
 * @param th             The thread for which to wait.
 *
 * @return               UDS_SUCCESS or error code
 **/
int uds_join_threads(struct thread *th);

#ifdef __KERNEL__
/**
 * Exit the current thread.  This is a kernel-only function that is intended to
 * be an alternative to using BUG() or BUG_ON().
 **/
__attribute__((noreturn)) void uds_thread_exit(void);
#endif

/**
 * Initialize a thread synchronization barrier (also known as a rendezvous).
 *
 * @param barrier       the barrier to initialize
 * @param thread_count  the number of threads that must enter the barrier
 *                      before any threads are permitted to leave it
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check uds_initialize_barrier(struct barrier *barrier,
					unsigned int thread_count);

/**
 * Destroy a thread synchronization barrier.
 *
 * @param barrier   the barrier to destroy
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_destroy_barrier(struct barrier *barrier);

/**
 * Enter a thread synchronization barrier, waiting for the configured number
 * of threads to have entered before exiting the barrier. Exactly one thread
 * will be arbitrarily selected to be flagged as the "winner" of a barrier.
 *
 * @param barrier   the barrier to enter
 * @param winner    if non-NULL, a pointer to the flag indicating whether the
 *                  calling thread was the unique winner
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_enter_barrier(struct barrier *barrier, bool *winner);

/**
 * Initialize a condition variable with default attributes.
 *
 * @param cond       condition variable to init
 *
 * @return           UDS_SUCCESS or error code
 **/
int __must_check uds_init_cond(struct cond_var *cond);

/**
 * Signal a condition variable.
 *
 * @param cond  condition variable to signal
 *
 * @return      UDS_SUCCESS or error code
 **/
int uds_signal_cond(struct cond_var *cond);

/**
 * Broadcast a condition variable.
 *
 * @param cond  condition variable to broadcast
 *
 * @return      UDS_SUCCESS or error code
 **/
int uds_broadcast_cond(struct cond_var *cond);

/**
 * Wait on a condition variable.
 *
 * @param cond    condition variable to wait on
 * @param mutex   mutex to release while waiting
 *
 * @return        UDS_SUCCESS or error code
 **/
int uds_wait_cond(struct cond_var *cond, struct mutex *mutex);

/**
 * Wait on a condition variable with a timeout.
 *
 * @param cond     condition variable to wait on
 * @param mutex    mutex to release while waiting
 * @param timeout  the relative time until the timeout expires
 *
 * @return error code (ETIMEDOUT if the deadline is hit)
 **/
int uds_timed_wait_cond(struct cond_var *cond,
			struct mutex *mutex,
			ktime_t timeout);

/**
 * Destroy a condition variable.
 *
 * @param cond  condition variable to destroy
 *
 * @return      UDS_SUCCESS or error code
 **/
int uds_destroy_cond(struct cond_var *cond);

#ifndef __KERNEL__
/**
 * Initialize a mutex, optionally asserting if the mutex initialization fails.
 * This function should only be called directly in places where making
 * assertions is not safe.
 *
 * @param mutex            the mutex to initialize
 * @param assert_on_error  if <code>true</code>, an error initializing the
 *                         mutex will make an assertion
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_initialize_mutex(struct mutex *mutex, bool assert_on_error);
#endif

/**
 * Initialize the default type (error-checking during development) mutex.
 *
 * @param mutex the mutex to initialize
 *
 * @return UDS_SUCCESS or an error code
 **/
#ifdef __KERNEL__
static INLINE int __must_check uds_init_mutex(struct mutex *mutex)
{
	mutex_init(mutex);
	return UDS_SUCCESS;
}
#else
int __must_check uds_init_mutex(struct mutex *mutex);
#endif

/**
 * Destroy a mutex (with error checking during development).
 *
 * @param mutex mutex to destroy
 *
 * @return UDS_SUCCESS or error code
 **/
#ifdef __KERNEL__
static INLINE int uds_destroy_mutex(struct mutex *mutex)
{
	return UDS_SUCCESS;
}
#else
int uds_destroy_mutex(struct mutex *mutex);
#endif

/**
 * Lock a mutex, with optional error checking during development.
 *
 * @param mutex mutex to lock
 **/
#ifdef __KERNEL__
static INLINE void uds_lock_mutex(struct mutex *mutex)
{
	mutex_lock(mutex);
}
#else
void uds_lock_mutex(struct mutex *mutex);
#endif

/**
 * Unlock a mutex, with optional error checking during development.
 *
 * @param mutex mutex to unlock
 **/
#ifdef __KERNEL__
static INLINE void uds_unlock_mutex(struct mutex *mutex)
{
	mutex_unlock(mutex);
}
#else
void uds_unlock_mutex(struct mutex *mutex);
#endif

/**
 * Initialize a semaphore used among threads in the same process.
 *
 * @param semaphore the semaphore to initialize
 * @param value     the initial value of the semaphore
 *
 * @return UDS_SUCCESS or an error code
 **/
#ifdef __KERNEL__
static INLINE int __must_check
uds_initialize_semaphore(struct semaphore *semaphore, unsigned int value)
{
	sema_init(semaphore, value);
	return UDS_SUCCESS;
}
#else
int __must_check uds_initialize_semaphore(struct semaphore *semaphore,
					  unsigned int value);
#endif

/**
 * Destroy a semaphore used among threads in the same process.
 *
 * @param semaphore the semaphore to destroy
 *
 * @return UDS_SUCCESS or an error code
 **/
#ifdef __KERNEL__
static INLINE int uds_destroy_semaphore(struct semaphore *semaphore)
{
	return UDS_SUCCESS;
}
#else
int uds_destroy_semaphore(struct semaphore *semaphore);
#endif

/**
 * Acquire a permit from a semaphore, waiting if none are currently available.
 *
 * @param semaphore the semaphore to acquire
 **/
#ifdef __KERNEL__
static INLINE void uds_acquire_semaphore(struct semaphore *semaphore)
{
	/*
	 * Do not use down(semaphore).  Instead use down_interruptible so that
	 * we do not get 120 second stall messages in kern.log.
	 */
	while (down_interruptible(semaphore) != 0) {
		/*
		 * If we're called from a user-mode process (e.g., "dmsetup
		 * remove") while waiting for an operation that may take a
		 * while (e.g., UDS index save), and a signal is sent (SIGINT,
		 * SIGUSR2), then down_interruptible will not block. If that
		 * happens, sleep briefly to avoid keeping the CPU locked up in
		 * this loop. We could just call cond_resched, but then we'd
		 * still keep consuming CPU time slices and swamp other threads
		 * trying to do computational work. [VDO-4980]
		 */
		fsleep(1000);
	}
}
#else
void uds_acquire_semaphore(struct semaphore *semaphore);
#endif

/**
 * Attempt to acquire a permit from a semaphore.
 *
 * If a permit is available, it is claimed and the function immediately
 * returns true. If a timeout is zero or negative, the function immediately
 * returns false. Otherwise, this will wait either a permit to become
 * available (returning true) or the relative timeout to expire (returning
 * false).
 *
 * @param semaphore the semaphore to decrement
 * @param timeout   the relative time until the timeout expires
 *
 * @return true if a permit was acquired, otherwise false
 **/
#ifdef __KERNEL__
static INLINE bool
__must_check uds_attempt_semaphore(struct semaphore *semaphore,
				   ktime_t timeout)
{
	if (timeout <= 0) {
		/* No timeout, just try to grab the semaphore. */
		return down_trylock(semaphore) == 0;
	} else {
		unsigned int jiffies = nsecs_to_jiffies(timeout);
		return down_timeout(semaphore, jiffies) == 0;
	}
}
#else
bool __must_check uds_attempt_semaphore(struct semaphore *semaphore,
					ktime_t timeout);
#endif

/**
 * Release a semaphore, incrementing the number of available permits.
 *
 * @param semaphore the semaphore to increment
 **/
#ifdef __KERNEL__
static INLINE void uds_release_semaphore(struct semaphore *semaphore)
{
	up(semaphore);
}
#else
void uds_release_semaphore(struct semaphore *semaphore);
#endif

#ifndef __KERNEL__
/**
 * Allocate a thread specific key for thread specific data.
 *
 * @param key            points to location for new key
 * @param destr_function destructor function called when thread exits
 *
 * @return               UDS_SUCCESS or error code
 **/
int uds_create_thread_key(pthread_key_t *key, void (*destr_function)(void *));

/**
 * Delete a thread specific key for thread specific data.
 *
 * @param key  key to delete
 *
 * @return     UDS_SUCCESS or error code
 **/
int uds_delete_thread_key(pthread_key_t key);

/**
 * Set pointer for thread specific data.
 *
 * @param key      key to be associated with pointer
 * @param pointer  data associated with key
 *
 * @return         UDS_SUCCESS or error code
 **/
int uds_set_thread_specific(pthread_key_t key, const void *pointer);

/**
 * Get pointer for thread specific data.
 *
 * @param key  key identifying the thread specific data
 **/
void *uds_get_thread_specific(pthread_key_t key);

/**
 * Simulate the action of cond_resched() for user programs.
 **/
static inline void cond_resched(void)
{
	/*
	 * On Linux sched_yield always succeeds so the result can be
	 * safely ignored.
	 */
	(void) sched_yield();
}
#endif

#endif /* UDS_THREADS_H */
