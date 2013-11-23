/*
 * Cooperative multitasking micro kernel
 *
 * Author: Manuel Jander
 * License: BSD
 * 
 * Gruss an die hochgradig gefiltertern und sonstige separatisten :)
 */

#define KERNEL_MAX_TASK 3

#if defined(__linux__) || defined(_WIN32)
#define TASK_FUNC(x) void x(void)
#define KERNEL_STACK_SIZE (2*8192*KERNEL_MAX_TASK)

#elif defined(__MSP430__)
#define TASK_FUNC(x) void /* __attribute__ ((naked))*/ x(void)

/* Work around backward compatibility. */
#ifdef __MSP430F149__
#define __MSP430_149__
#endif
#ifdef __MSP430F169__
#define __MSP430_169__
#endif
#ifdef __MSP430F1161__
#define __MSP430_1161__
#endif
#ifdef __MSP430F449__
#define __MSP430_449__
#endif

#if defined(__MSP430_449__)
#define KERNEL_STACK_SIZE (96*KERNEL_MAX_TASK)
#else
#define KERNEL_STACK_SIZE (64*KERNEL_MAX_TASK) 
#endif
#elif defined(__arm__)
#define TASK_FUNC(x) void x(void)
#define KERNEL_STACK_SIZE (256*KERNEL_MAX_TASK)
#endif

typedef void(*kernel_task_t)(void);

/**
 * \brief Initialize the poeli kernel
 */
void kernel_init(void);

/**
 * Register a task.
 */
int kernel_task_register(kernel_task_t task, int stack_size);

/**
 * Start the poeli kernel
 */
void kernel_run(void);

/**
 * check if kernel is runnung or not
 */
int kernel_running(void);

/**
 * \brief set sleep time in jiffies. Call kernel_yield afterwards to do actual sleep.
 */
void kernel_sleep(unsigned int j);

/**
 * \brief set suspend current task. Call kernel_yield afterwards to do actual suspend.
 */
void kernel_suspend(void);

/*
 * \brief give execution thread back to the kernel.
 */
void kernel_yield(void);

/**
 * \brief Set given task as running and not sleeping
 * \param task the task index to wake up. If -1, then all
 *        currently registered will be woken up.
 */
void kernel_wakeup(int task);

/**
 * \brief get current task ID. The returned value can be used for kernel_wakeup()
 */
unsigned int kernel_getTask(void);
