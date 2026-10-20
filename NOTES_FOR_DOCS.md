# Notes for the Thesis Documentation

This file explains, in plain English, how the kernel works after adding priority
scheduling, sleeping, semaphores, mutexes and message queues. It is meant as raw
material for the "Implementation" chapter.

---

## 1. Files and structs that were changed or created

### Modified

| File | What changed |
|------|--------------|
| `kernel/inc/task.h` | `TCB_t` got new fields: `waiting_obj`, `priority`, `base_priority`. `task_create()` takes a `priority`. `task_exit_handler()` is now declared. |
| `kernel/src/task.c` | Initial stacked **LR** is now `task_exit_handler` instead of `0xFFFFFFFD`. Initialises the new TCB fields. |
| `kernel/inc/sched.h` | New prototypes: `scheduler_tick()`, `task_delay()`, plus three blocking helpers (`scheduler_block_current()`, `scheduler_wake_one()`, `scheduler_reschedule()`). `scheduler_add_task()` takes a `priority`. |
| `kernel/src/sched.c` | `schedule()` is now **priority-based with round-robin between equal priorities**. Added the tick handler, `task_delay()` and the blocking helpers. Interrupts are disabled in `scheduler_init()` until the first task starts. The idle stack is 8-byte aligned. |
| `kernel/port/arm_cm7/port.c` | `SysTick_Handler` now calls `scheduler_tick()` and `schedule()`, and only pends PendSV if a different task should run. It returns early while `current_task == NULL`. |
| `app/main.c`, `app/main.h` | Three demo tasks (Modbus / Sensor / Heartbeat). PendSV and SysTick now have the **same** lowest NVIC priority. The task stacks are 8-byte aligned. |

### Created

| File | Content |
|------|---------|
| `kernel/inc/sync.h`, `kernel/src/sync.c` | `semaphore_t` and `mutex_t` with their functions. |
| `kernel/inc/queue.h`, `kernel/src/queue.c` | `queue_t`, a fixed-size `uint32_t` ring-buffer message queue. |

### Untouched

`context_switch.s` (`PendSV_Handler`) is unchanged, and so is the circular `tcb->next` list.
`sp` is still the **first** field of `TCB_t` (offset 0), which `PendSV_Handler` relies on
(`LDR R1, [R0]` / `STR R1, [R0]`).

### The structs

```c
typedef struct tcb {
    uint32_t *sp;            // saved stack pointer (offset 0, used by PendSV)
    struct tcb *next;        // next task in the circular list
    task_status_t status;    // READY, BLOCKED, SLEEPING
    uint32_t sleep_ticks;    // ticks left to sleep
    void *waiting_obj;       // what the task is blocked on (sem/mutex), else NULL
    uint8_t id;
    uint8_t priority;        // current (effective) priority, higher = more important
    uint8_t base_priority;   // original priority, restored after inheritance
} TCB_t;

typedef struct { uint32_t count; uint32_t max_count; } semaphore_t;
typedef struct { uint8_t locked; TCB_t *owner; }       mutex_t;

typedef struct {
    uint32_t *buffer; uint32_t capacity;
    uint32_t head, tail;          // read / write index
    semaphore_t items;            // filled slots
    semaphore_t spaces;           // free slots
} queue_t;
```

### Bug fixes worth mentioning

1. **Initial LR value.** The stacked LR was `0xFFFFFFFD`, an EXC_RETURN code. That value only
   means something to the CPU when it is loaded into LR *inside an exception handler*. If a
   task function ever returned, the CPU would jump to address `0xFFFFFFFC` and crash with a
   HardFault that's hard to trace. Now LR points to `task_exit_handler()`, so a task that
   returns ends up in a known, debuggable infinite loop.
2. **SysTick/PendSV priority.** The code used `NVIC_SetPriority(PendSV, 0xFF)` and
   `NVIC_SetPriority(SysTick, 0xFE)`. The STM32F7 only implements 4 priority bits, and
   `NVIC_SetPriority()` shifts the value left by 4, so these became levels **15** and **14**.
   SysTick could therefore interrupt PendSV in the middle of a context switch and change
   `next_task` between PendSV's two reads of it. Both are now set to level 15, the lowest.
   Two handlers at the same priority can never preempt each other.
3. **Stack alignment.** The ARM procedure call standard (AAPCS) requires an 8-byte aligned
   stack at public function boundaries. The stack arrays now have `__attribute__((aligned(8)))`.
4. **Interrupts before start.** `SysTick_Config()` starts the timer before the scheduler runs.
   Interrupts are now off from `scheduler_init()` until `port_start_first_task()`, and
   `SysTick_Handler` also ignores ticks while `current_task == NULL`.

---

## 2. How each part works

### 2.1 The circular task list

```
   idle_tcb ──► heartbeat ──► sensor ──► modbus ──┐
      ▲                                           │
      └───────────────────────────────────────────┘
```

`scheduler_init()` creates the idle task and points it at itself. Each
`scheduler_add_task()` inserts the new TCB right after `idle_tcb`. The list is
never broken, so any loop of the form

```c
t = start;
do { ...; t = t->next; } while (t != start);
```

visits every task exactly once.

### 2.2 Priority scheduler with round-robin tie-breaking (`schedule()`)

1. Start at `current_task->next`. Before the first task runs, `current_task` is NULL,
   so start at `idle_tcb.next` instead.
2. Walk once around the whole ring.
3. Skip the idle task. Among the `TASK_READY` tasks, remember the one with the
   **highest** `priority`. A task only replaces the current best if its priority is
   **strictly greater** (`>`), not `>=`.
4. If a task was found, it becomes `next_task`. Otherwise `next_task = &idle_tcb`.

**Why this gives round-robin for equal priorities:** because of the strict `>`,
the *first* task of the top priority found after `current_task` wins. The current task
is the *last* node visited, so it only keeps the CPU if no other task of the same
priority is ready. Example with A, B, C all at priority 2:
A runs → search starts at B → B wins → next tick search starts at C → C wins → ... → A.

**Why it is preemptive:** `schedule()` runs on every SysTick and every time a
semaphore/mutex/queue wakes a task. If the woken task has a higher priority, it is
picked straight away and PendSV switches to it.

A task that is running keeps `status == TASK_READY`. In this kernel READY simply means
"runnable", whether or not the task is on the CPU right now. `TASK_RUNNING` stays in the enum but is not used.

**Context switch path:** `schedule()` only *chooses* the task by writing `next_task`.
`port_yield()` sets the PendSV pending bit, and the unchanged `PendSV_Handler`
does the actual register save/restore once no other interrupt is active.

### 2.3 Sleeping: `task_delay(ticks)` and `scheduler_tick()`

`task_delay(ticks)`, called by a task:

1. Disable interrupts so SysTick can't run halfway through.
2. Set `sleep_ticks = ticks` and `status = TASK_SLEEPING`.
3. Call `schedule()`, which won't pick this task because it is no longer READY.
4. Call `port_yield()`. PendSV is now *pending*, but it can't run yet.
5. Enable interrupts. PendSV fires immediately and the task is switched out.
6. Execution continues after `__enable_irq()` once the task is READY and picked again.

`task_delay(0)` skips step 2, so it is just a "yield".

`scheduler_tick()`, called from `SysTick_Handler` every 1 ms:

- Walk the ring. For each `TASK_SLEEPING` task, decrement `sleep_ticks`. When it reaches 0,
  set the task to `TASK_READY`.
- `SysTick_Handler` then calls `schedule()`. If the newly woken task has a higher priority
  (or it is another equal-priority task's turn), it pends PendSV.

Resolution: the delay ends on a tick edge, so `task_delay(n)` sleeps between `n-1` and `n` ms.

### 2.4 Semaphore

A counting semaphore holds `count` tokens, with at most `max_count`.

- **`sem_wait`**: with interrupts off, `while (count == 0)` do: mark self BLOCKED with
  `waiting_obj = sem`, reschedule, briefly enable interrupts so PendSV can switch away,
  then disable them again. Once `count > 0`, take a token (`count--`).
- **`sem_try_wait`**: same thing, but never blocks. It returns -1 if no token is available.
- **`sem_give`**: if `count < max_count`, do `count++`, wake the *highest-priority* task
  whose `waiting_obj == sem`, and reschedule. A woken task with a higher priority than the
  caller preempts the caller right away. Returns -1 if the semaphore was already full.
  `sem_give` is safe to call from an ISR, e.g. the future UART RX interrupt for Modbus.

**Why a `while` loop and not an `if`:** being woken only means "a token *was* added". Before
the woken task gets the CPU, a higher-priority task may already have taken that token.
The loop checks again and goes back to sleep if needed.

### 2.5 Mutex (with basic priority inheritance)

- **`mutex_lock`**: `while (locked)`: if the owner has a lower priority than me, raise
  `owner->priority` to mine (*priority inheritance*), then block on the mutex just like
  `sem_wait`. When the mutex is free, set `locked = 1` and `owner = current_task`.
- **`mutex_unlock`**: only the owner may unlock (otherwise it returns -1). Clear `locked` and
  `owner`, restore `priority = base_priority`, wake the highest-priority waiter, and reschedule.

**What priority inheritance prevents (priority inversion):** Low takes the mutex. High wants it
and blocks. Without inheritance, Medium (which doesn't need the mutex) could run for as long
as it likes, because it beats Low, and High waits on Medium indirectly. With inheritance, Low
temporarily runs at High's priority, so Medium can't preempt it. Low finishes its critical
section quickly, unlocks, drops back to its base priority, and High runs.

Mutex vs. semaphore: a mutex has an **owner**. Only the owner can unlock it, and the owner's
priority can be boosted. A semaphore is just a counter that anyone, including an ISR, can give.

### 2.6 Message queue

The queue is a ring buffer of `uint32_t` items, plus two semaphores. This is the textbook
"bounded buffer" or "producer–consumer" solution:

```
spaces = free slots   (starts at capacity)
items  = filled slots (starts at 0)

queue_send:    sem_wait(spaces) → buffer[tail] = item, tail = (tail+1) % cap → sem_give(items)
queue_receive: sem_wait(items)  → item = buffer[head], head = (head+1) % cap → sem_give(spaces)
```

- When the queue is **full**, `spaces` is 0, so senders block.
- When the queue is **empty**, `items` is 0, so receivers block.
- `queue_try_send` / `queue_try_receive` use `sem_try_wait` and return -1 instead of blocking.
  That makes them usable from ISRs and from tasks that must not stall.
- The short index update happens with interrupts disabled, so two senders never write the same slot.

Since the queue is built on the semaphores, it contains **no** blocking code of its own.
Senders wait on `&q->spaces` and receivers on `&q->items`, so the two kinds of waiters
can never be confused with each other.

To send something larger than 32 bits (e.g. a Modbus frame), send a pointer or a buffer
index cast to `uint32_t`, or pack small fields into one word.

### 2.7 The demo application (`app/main.c`)

| Task | Prio | Behaviour |
|------|------|-----------|
| `modbus_task` | 3 (high) | `sem_wait(&modbus_request_sem)`, then drains `sensor_queue` without blocking and reads `sensor_data` under the mutex. |
| `sensor_task` | 2 (medium) | Every 100 ticks: writes fake readings into `sensor_data` under the mutex and does `queue_try_send` of the temperature. |
| `heartbeat_task` | 1 (low) | Every 500 ticks: toggles `led_state` and, as a stand-in for the UART ISR, does `sem_give(&modbus_request_sem)`. |
| idle | 0 | `WFI` whenever every task is sleeping or blocked. |

Things to watch in Renode or a debugger: `heartbeat_counter`, `sensor_reads`,
`modbus_requests_handled`, `modbus_last_sample`, `sensor_queue_drops`, `current_task`.
Expected: `sensor_reads ≈ 5 × heartbeat_counter`, `modbus_requests_handled == heartbeat_counter`,
`sensor_queue_drops == 0`, and `current_task` is usually `idle_tcb`.

---

## 3. How `waiting_obj` unblocks tasks without per-object wait lists

Most RTOSes (FreeRTOS, Zephyr) give every semaphore or mutex its **own linked list** of
waiting tasks. That list has to be kept sorted by priority and updated on every block and
wake-up, which means extra pointers, extra code and extra ways to get it wrong.

This kernel uses the task list it already has:

1. **Blocking**: the task writes *which object it is waiting for* into its own TCB:
   ```c
   current_task->status      = TASK_BLOCKED;
   current_task->waiting_obj = sem;          // just the address of the object
   ```
   The semaphore struct itself stores nothing about its waiters.
2. **Waking** (`scheduler_wake_one(obj)`): walk the circular task list once and look for
   tasks with `status == TASK_BLOCKED && waiting_obj == obj`. Pick the one with the highest
   priority, set it to `TASK_READY`, and clear `waiting_obj`.
3. **Scheduling**: a BLOCKED task is simply skipped by `schedule()`. It becomes visible again
   as soon as it is READY.

The object's **address** works as a unique "wait channel" ID. Any kernel object can use the
same mechanism with no extra fields. The queue shows this: senders and receivers wait on two
different addresses (`&q->spaces` and `&q->items`).

**Trade-off:** waking costs O(N), one pass over all N tasks, instead of O(1). With a handful of
tasks (here 4 including idle) the loop is a few dozen instructions, so the cost is negligible.
In exchange you get much less code, no per-object memory, and logic you can explain in two
sentences. The highest-priority waiter is found by the same comparison `schedule()` uses, so
the wake order is still priority-based.

---

## 4. Known limitations (good for a "Future work" section)

- **ISRs and PendSV.** PendSV and SysTick share the lowest priority, so they can't
  interrupt each other. A *higher*-priority peripheral ISR that calls `sem_give()` could still
  interrupt PendSV between its two reads of `next_task`. The robust fix is to wrap the
  PendSV body in `CPSID I` / `CPSIE I` (a two-line change in `context_switch.s`).
- **Critical sections don't nest.** `__disable_irq()/__enable_irq()` is a plain on/off switch.
  Don't call kernel functions while interrupts are already disabled, because they would
  re-enable them on exit. A nesting counter, or saving and restoring PRIMASK, would fix that.
- **No FPU context.** The build uses soft-float, so PendSV only saves R4–R11. Enabling the
  hardware FPU (`-mfpu=fpv5-d16 -mfloat-abi=hard`) would also require saving S16–S31 and
  handling the EXC_RETURN value correctly.
- **Priority inheritance is single-level.** It is not transitive (if the owner is itself
  blocked on another mutex). Unlocking restores `base_priority` directly, which is correct
  as long as a task holds only one mutex at a time.
- **Mutexes are not recursive.** A task that locks the same mutex twice deadlocks.
- **Blocking calls only from tasks.** `sem_wait`, `mutex_lock`, `queue_send`/`receive` and
  `task_delay` must not be called from an ISR. `sem_give`, `sem_try_wait` and `queue_try_*` can be.
- **O(N) scans** in `schedule()`, `scheduler_tick()` and `scheduler_wake_one()`. That is fine for
  small task counts. Priority bitmaps and ready lists are the classic upgrade.
- **Renode timing.** The NVIC in `renode/stm32f756.repl` has no `systickFrequency`, so
  simulated ticks run faster than 1 ms. Adding `systickFrequency: 16000000` (matching
  `SystemCoreClock` = 16 MHz) to the `nvic` entry should make the timing realistic.
