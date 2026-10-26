/* win32_host_arm32_0104_async.inl - UNIT "async" of the Minecraft PE 0.10.4.40 ARM32 host.
 *
 * Owner: the async implementer.  Brief: arm32/v0_10_4/units/async.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit async
 *
 * This file owns:
 *   1. ThreadPool statics (IThreadPoolStatics 6/7/8) - every Run* starts ONE REAL thread (never reused, MTA, 2 MiB stack) that calls the game's WorkItemHandler.Invoke
 *      (vtable slot 3) with the work item's IAsyncAction; exceptions are caught and turn the action into Error (R5).
 *   2. IAsyncAction + IAsyncInfo (one host class for the pool items and the CoreDispatcher.RunAsync results): Status is 0 (Started) until the work returned or Cancel
 *      (2, immediate and sticky, R12); late put_Completed handlers run asynchronously through the UI queue (R1); get_Status carries the render-loop throttle.
 *   3. CoreDispatcher (ICoreDispatcher 6..9): RunAsync ALWAYS queues (R1/R11), strict priority High > Normal > Low > Idle, FIFO inside a priority; ProcessEvents(ProcessUntilQuit)
 *      blocks the pool threads of the two input work items until Cancel / shutdown.
 *   4. DispatcherTimer (factory + IDispatcherTimer 6..12): ticks only from the UI pass, not before x104.f_size_changed_done (R3), never bursting.
 *   5. The UI thread primitives: x104_ui_post / x104_ui_pump_once / x104_ui_run_loop / x104_ui_quit.
 *
 * Locks: x104_async_st.lock guards the UI queue, the item list and the worker table; item->lock guards one action's status/handlers; x104_async_st.timer_lock guards
 * the timers.  All are leaf locks: nothing is called into the game (except the cheap AddRef/Release of a delegate the core's event code does as well) while one is held.
 * Objects are never freed (core model): a host action therefore needs no reference for the queue or the worker, only the game's delegates are reference counted. */
#ifndef WIN32_HOST_ARM32_0104_ASYNC_INL
#define WIN32_HOST_ARM32_0104_ASYNC_INL

#define X104_ASYNC_MAX_TIMERS        16
#define X104_ASYNC_MAX_WORKERS       64
#define X104_ASYNC_PUMP_CAP          64          /* queue items run per x104_ui_pump_once */
#define X104_ASYNC_MWMO_INPUTAVAILABLE 0x0004u   /* MsgWaitForMultipleObjectsEx dwFlags: MWMO_WAITALL is 0x1, MWMO_ALERTABLE 0x2, MWMO_INPUTAVAILABLE 0x4 */
#define X104_ASYNC_DRAIN_CAP         256        /* window messages dispatched per loop iteration before a pass runs */
#define X104_ASYNC_STACK_BYTES       (2u * 1024u * 1024u)
#define X104_ASYNC_SHUTDOWN_WAIT_MS  2000u
#define X104_ASYNC_SLOW_HANDLER_MS   500u
#define X104_ASYNC_PROCESS_POLL_MS   250u
#define X104_ASYNC_KIND_POOL         0
#define X104_ASYNC_KIND_DISPATCH     1
#define X104_ASYNC_PRIO_IDLE         (-2)        /* CoreDispatcherPriority */
#define X104_ASYNC_PRIO_LOW          (-1)
#define X104_ASYNC_PRIO_NORMAL       0
#define X104_ASYNC_PRIO_HIGH         1
#define X104_ASYNC_THREAD_PRIORITY_LOWEST  (-2)
#define X104_ASYNC_RPC_E_CHANGED_MODE      ((HRESULT)0x80010106L)
#define X104_ASYNC_DISPATCH_ID_BASE        0x40000000u   /* get_Id of a dispatcher action: base + RunAsync number (pool items use their own #n) */

/* ======================================================================================================================
 * 1. types and state
 * ====================================================================================================================== */
/* one IAsyncAction/IAsyncInfo: a thread-pool work item or the result of CoreDispatcher.RunAsync */
typedef struct x104_async_item {
    X104Object base;                     /* views[0] = IAsyncAction, views[1] = IAsyncInfo */
    struct x104_async_item *all_next;    /* list of the pool items (shutdown walks it) */
    LONG id;                             /* pool: item number #n; dispatcher: RunAsync number #n (get_Id adds X104_ASYNC_DISPATCH_ID_BASE) */
    int kind;
    int priority;                        /* WorkItemPriority (pool) / CoreDispatcherPriority (dispatcher) */
    int options;
    volatile LONG status;                /* AsyncStatus: 0 Started, 1 Completed, 2 Canceled, 3 Error (atomic loads on the poll path) */
    volatile LONG done;                  /* the work returned / the handler ran: the Completed handler has been (or will at once be) fired */
    HRESULT error;
    void *handler;                       /* IWorkItemHandler / DispatchedHandler (AddRef'd; released when the item finishes) */
    void *completed;                     /* AsyncActionCompletedHandler (AddRef'd) */
    HANDLE cancel_event;                 /* manual reset; pool items only; wakes ProcessEvents(ProcessUntilQuit) */
    DWORD thread_id;
    DWORD create_tick;
    DWORD start_tick;
    DWORD caller_rva;                    /* game rva of the call site that created the item */
    X104Lock lock;
    /* get_Status poll throttle (render loop) */
    DWORD poll_thread;
    DWORD poll_window;
    LONG poll_count;
    volatile LONG throttled;
} x104_async_item;

/* queue node: either a posted function (fn/arg) or a CoreDispatcher.RunAsync item (action/handler) */
typedef struct x104_async_node {
    struct x104_async_node *next;
    void (*fn)(void *);
    void *arg;
    x104_async_item *action;
    void *handler;
    int priority;
} x104_async_node;

typedef struct x104_async_timer {
    X104Object base;                     /* views[0] = IDispatcherTimer */
    LONGLONG interval_ticks;             /* TimeSpan, 100 ns */
    DWORD interval_ms;
    volatile LONG enabled;
    LONG gate_seen;                      /* the tick gate (f_size_changed_done) was observed open for this timer */
    DWORD next_due;                      /* GetTickCount */
    LONG number;
    LONG ticks;
    LONG tick_exceptions;
    X104Event tick;
} x104_async_timer;

typedef struct x104_async_late {
    x104_async_item *item;
    void *handler;
} x104_async_late;

static struct {
    X104Lock lock;                       /* queue, item list, worker table */
    HANDLE wake_event;                   /* auto reset: the loop waits on it */
    HANDLE quit_event;                   /* manual reset: set by x104_async_shutdown */
    X104Object *dispatcher;
    x104_async_node *head[4];            /* index = priority + 2 */
    x104_async_node *tail[4];
    volatile LONG queue_length;
    x104_async_item *items;              /* pool items, newest first */
    struct { DWORD tid; x104_async_item *item; } workers[X104_ASYNC_MAX_WORKERS];
    volatile LONG pool_serial;
    volatile LONG dispatch_serial;
    volatile LONG workers_running;
    volatile LONG shutdown_state;        /* 0 not started, 1 running, 2 done */
    volatile LONG in_pump;
    volatile LONG initialised;
    volatile LONG posted;
    volatile LONG executed;
    volatile LONG passes;
    X104Lock timer_lock;
    x104_async_timer *timers[X104_ASYNC_MAX_TIMERS];
    unsigned timer_count;
    DWORD gate_log_tick;
} x104_async_st;

/* "item" for a thread-pool work item, "RunAsync" for the action of a CoreDispatcher.RunAsync (log labels: "item #2" / "RunAsync #5") */
static const char *x104_async_name(const x104_async_item *item)
{
    return item->kind == X104_ASYNC_KIND_DISPATCH ? "RunAsync" : "item";
}

static void x104_async_wake(void)
{
    if (x104_async_st.wake_event) SetEvent(x104_async_st.wake_event);
}

/* a COM delegate object must at least have a readable vtable with the 4 IUnknown/Invoke slots */
static BOOL x104_async_valid_delegate(void *delegate)
{
    return delegate && !IsBadReadPtr(delegate, sizeof(void *)) && !IsBadReadPtr(*(void **)delegate, 4 * sizeof(void *));
}

/* fn(arg) inside __try/__except (the queue items are plain C functions of the other units) */
static BOOL x104_async_call_fn_guarded(void (*fn)(void *), void *arg, X104Seh *seh)
{
    BOOL clean = TRUE;
    memset(seh, 0, sizeof(*seh));
    __try {
        fn(arg);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), seh)) {
        clean = FALSE;
    }
    return clean;
}

static void x104_async_log_seh(const char *what, const X104Seh *seh)
{
    x104_logf("X104 async: EXCEPTION escaped from %s: code=%08lx at %p (game rva %08lx) info=%08lx %08lx%s", what, (unsigned long)seh->code, seh->address,
              (unsigned long)seh->rva, (unsigned long)seh->info0, (unsigned long)seh->info1, seh->code == 0xE06D7363u ? " [C++ exception]" : "");
}

/* ======================================================================================================================
 * 2. IAsyncAction + IAsyncInfo
 * ====================================================================================================================== */
static x104_async_item *x104_async_item_new(int kind, int priority, int options, LONG id, void *handler, DWORD caller_rva);

/* Set the final status (only when the action is still Started: a Cancel stays Canceled), mark it done and fire the Completed handler exactly once.
 * The handler is invoked guarded as handler->Invoke(IAsyncAction, AsyncStatus) on the calling thread (a pool worker or the UI pass: never inside a game call). */
static void x104_async_finish(x104_async_item *item, int final_status, HRESULT error)
{
    void *completed;
    int status;
    x104_lock(&item->lock);
    if (item->status == X104_ASYNC_STARTED) {
        item->error = error;
        __atomic_store_n(&item->status, (LONG)final_status, __ATOMIC_RELEASE);
    }
    status = (int)item->status;
    item->done = 1;
    completed = item->completed;
    if (completed) x104_raw_addref(completed);
    x104_unlock(&item->lock);
    if (completed) {
        HRESULT hr = S_OK;
        X104Seh seh;
        if (!x104_call_guarded3(((void **)*(void ***)completed)[3], completed, x104_object_iface0(&item->base), (void *)(ULONG_PTR)status, &hr, &seh)) {
            x104_async_log_seh("a Completed handler", &seh);
        } else if (FAILED(hr)) {
            x104_logf("X104 async: %s #%ld Completed handler %p returned 0x%08lx %s", x104_async_name(item), (long)item->id, completed, (unsigned long)hr, x104_hr_name(hr));
        }
        x104_raw_release(completed);
    }
}

/* late put_Completed: runs on the UI thread from the queue (R1) */
static void x104_async_late_completed(void *argument)
{
    x104_async_late *late = (x104_async_late *)argument;
    HRESULT hr = S_OK;
    X104Seh seh;
    int status = (int)late->item->status;
    if (!x104_call_guarded3(((void **)*(void ***)late->handler)[3], late->handler, x104_object_iface0(&late->item->base), (void *)(ULONG_PTR)status, &hr, &seh)) {
        x104_async_log_seh("a late Completed handler", &seh);
    } else if (FAILED(hr)) {
        x104_logf("X104 async: %s #%ld late Completed handler %p returned 0x%08lx %s", x104_async_name(late->item), (long)late->item->id, late->handler, (unsigned long)hr, x104_hr_name(hr));
    }
    x104_raw_release(late->handler);
    HeapFree(GetProcessHeap(), 0, late);
}

static HRESULT X104_API x104_async_action_put_completed(void *self, void *handler)
{
    x104_async_item *item = X104_THIS(x104_async_item, self);
    void *old;
    BOOL late;
    static volatile LONG logged;
    if (handler && !x104_async_valid_delegate(handler)) {
        x104_logf("X104 async: %s #%ld put_Completed(handler=%p): not a COM object (unreadable vtable) -> E_POINTER", x104_async_name(item), (long)item->id, handler);
        return E_POINTER;
    }
    if (handler) x104_raw_addref(handler);                 /* the item's own reference */
    x104_lock(&item->lock);
    old = item->completed;
    item->completed = handler;
    late = handler != NULL && item->done != 0;
    x104_unlock(&item->lock);
    if (old) x104_raw_release(old);
    X104_LOG_LIMITED(&logged, 30, "X104 async: %s #%ld put_Completed(handler=%p) status=%ld%s", x104_async_name(item), (long)item->id, handler, (long)item->status,
                     late ? " - the item is already finished: the handler is queued for the UI thread (R1/R12)" : "");
    if (late) {
        x104_async_late *job = (x104_async_late *)HeapAlloc(GetProcessHeap(), X104_HEAP_ZERO_MEMORY, sizeof(*job));
        if (!job) {
            x104_logf("X104 async: %s #%ld put_Completed: out of memory queueing the late handler %p - NOT invoked", x104_async_name(item), (long)item->id, handler);
            return S_OK;
        }
        job->item = item;
        job->handler = handler;
        x104_raw_addref(handler);                          /* the job's reference */
        if (!x104_ui_post(x104_async_late_completed, job)) {
            x104_raw_release(handler);
            HeapFree(GetProcessHeap(), 0, job);
            x104_logf("X104 async: %s #%ld put_Completed: the UI queue is closed (shutdown) - handler %p NOT invoked", x104_async_name(item), (long)item->id, handler);
        }
    }
    return S_OK;
}

static HRESULT X104_API x104_async_action_get_completed(void *self, void **handler)
{
    x104_async_item *item = X104_THIS(x104_async_item, self);
    void *current;
    if (!handler) return E_POINTER;
    x104_lock(&item->lock);
    current = item->completed;
    if (current) x104_raw_addref(current);
    x104_unlock(&item->lock);
    *handler = current;
    return S_OK;
}

/* GetResults: S_OK (the game never reads a result); an Error action reports its HRESULT */
static HRESULT X104_API x104_async_action_get_results(void *self)
{
    x104_async_item *item = X104_THIS(x104_async_item, self);
    return item->status == X104_ASYNC_ERROR ? item->error : S_OK;
}

static HRESULT X104_API x104_async_info_get_id(void *self, UINT32 *id)
{
    if (!id) return E_POINTER;
    {
        const x104_async_item *item = X104_THIS(x104_async_item, self);
        *id = (UINT32)(item->kind == X104_ASYNC_KIND_DISPATCH ? (LONG)X104_ASYNC_DISPATCH_ID_BASE + item->id : item->id);
    }
    return S_OK;
}

/* Called continuously by the render thread (the loop body 0x1DA160 polls it once per iteration; before the game is ready it spins without any sleep).  If the same
 * thread polls an unfinished pool action more than ~200 times within 10 ms while the game is NOT ready, Sleep(1) (logged once); once the game is ready never sleep,
 * only hand the CPU over (SwitchToThread) every 64th poll so that the UI thread is never starved on a 2-core Surface RT. */
static void x104_async_poll_throttle(x104_async_item *item)
{
    DWORD thread = GetCurrentThreadId();
    LONG count;
    if (item->poll_thread != thread) {
        item->poll_thread = thread;
        item->poll_count = 0;
        item->poll_window = GetTickCount();
    }
    count = ++item->poll_count;
    if ((count & 63) != 0) return;                         /* 63 of 64 polls cost one increment */
    if (x104_game_is_ready()) {
        SwitchToThread();
        item->poll_count = 0;
        item->poll_window = GetTickCount();
        return;
    }
    if (count >= 192) {
        DWORD now = GetTickCount();
        if (now - item->poll_window <= 10) {
            if (__sync_val_compare_and_swap(&item->throttled, 0, 1) == 0) {
                x104_logf("X104 async: item #%ld throttle engaged (thread %04lx polled IAsyncInfo.Status %ld times in %lu ms while the game is not ready): Sleep(1) from now on",
                          (long)item->id, (unsigned long)thread, (long)count, (unsigned long)(now - item->poll_window));
            }
            Sleep(1);
        }
        item->poll_count = 0;
        item->poll_window = GetTickCount();
    }
}

static HRESULT X104_API x104_async_info_get_status(void *self, int *status)
{
    x104_async_item *item;
    int value;
    if (!status) return E_POINTER;
    item = X104_THIS(x104_async_item, self);
    value = (int)__atomic_load_n(&item->status, __ATOMIC_ACQUIRE);
    *status = value;
    if (value == X104_ASYNC_STARTED && item->kind == X104_ASYNC_KIND_POOL) x104_async_poll_throttle(item);
    return S_OK;
}

static HRESULT X104_API x104_async_info_get_error(void *self, HRESULT *hr)
{
    if (!hr) return E_POINTER;
    *hr = X104_THIS(x104_async_item, self)->error;
    return S_OK;
}

/* Cancel: Started -> Canceled immediately and for ever (R12); wakes a ProcessEvents(ProcessUntilQuit) that blocks this item's thread.  Nothing else happens: the
 * game's loop sees Status != 0 and returns by itself. */
static HRESULT X104_API x104_async_info_cancel(void *self)
{
    x104_async_item *item = X104_THIS(x104_async_item, self);
    int before;
    x104_lock(&item->lock);
    before = (int)item->status;
    if (before == X104_ASYNC_STARTED) __atomic_store_n(&item->status, (LONG)X104_ASYNC_CANCELED, __ATOMIC_RELEASE);
    x104_unlock(&item->lock);
    if (item->cancel_event) SetEvent(item->cancel_event);
    x104_logf("X104 async: %s #%ld Cancel() on thread %04lx (status was %d%s)", x104_async_name(item), (long)item->id, (unsigned long)GetCurrentThreadId(), before,
              before == X104_ASYNC_STARTED ? " Started -> Canceled" : ", unchanged");
    return S_OK;
}

static HRESULT X104_API x104_async_info_close(void *self)
{
    x104_async_item *item = X104_THIS(x104_async_item, self);
    static volatile LONG logged;
    if (item->status == X104_ASYNC_STARTED) X104_LOG_LIMITED(&logged, 10, "X104 async: %s #%ld Close() while Started - ignored (S_OK)", x104_async_name(item), (long)item->id);
    return S_OK;
}

X104_VTBL(x104_async_vt_action, IAsyncAction) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncAction, put_Completed, x104_async_action_put_completed),
    X104_SLOT(IAsyncAction, get_Completed, x104_async_action_get_completed),
    X104_SLOT(IAsyncAction, GetResults, x104_async_action_get_results)
};
X104_VTBL(x104_async_vt_info, IAsyncInfo) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncInfo, get_Id, x104_async_info_get_id),
    X104_SLOT(IAsyncInfo, get_Status, x104_async_info_get_status),
    X104_SLOT(IAsyncInfo, get_ErrorCode, x104_async_info_get_error),
    X104_SLOT(IAsyncInfo, Cancel, x104_async_info_cancel),
    X104_SLOT(IAsyncInfo, Close, x104_async_info_close)
};
static const X104IfaceDesc x104_async_action_ifaces[] = {
    X104_IFACE(IAsyncAction, x104_async_vt_action),
    X104_IFACE(IAsyncInfo, x104_async_vt_info)
};
X104_CLASS_DEF(x104_async_action_class, "AsyncAction", L"Windows.Foundation.IAsyncAction", x104_async_action_ifaces, X104_CLASS_AGILE, NULL);

static x104_async_item *x104_async_item_new(int kind, int priority, int options, LONG id, void *handler, DWORD caller_rva)
{
    x104_async_item *item = (x104_async_item *)x104_object_new(&x104_async_action_class, sizeof(x104_async_item));
    if (!item) return NULL;
    item->kind = kind;
    item->priority = priority;
    item->options = options;
    item->id = id;
    item->handler = handler;
    item->caller_rva = caller_rva;
    item->create_tick = GetTickCount();
    if (kind == X104_ASYNC_KIND_POOL) {
        item->cancel_event = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!item->cancel_event) {
            x104_logf("X104 async: ERROR CreateEvent for item #%ld failed (error %lu)", (long)id, (unsigned long)GetLastError());
            return NULL;
        }
    }
    return item;
}

/* ======================================================================================================================
 * 3. ThreadPool
 * ====================================================================================================================== */
static void x104_async_worker_register(x104_async_item *item)
{
    unsigned index;
    BOOL stored = FALSE;
    x104_lock(&x104_async_st.lock);
    for (index = 0; index < X104_ASYNC_MAX_WORKERS; ++index) {
        if (!x104_async_st.workers[index].tid) {
            x104_async_st.workers[index].tid = item->thread_id;
            x104_async_st.workers[index].item = item;
            stored = TRUE;
            break;
        }
    }
    x104_unlock(&x104_async_st.lock);
    if (!stored) x104_logf("X104 async: WARNING worker table full (%u): item #%ld on thread %04lx cannot be found by ProcessEvents", (unsigned)X104_ASYNC_MAX_WORKERS, (long)item->id, (unsigned long)item->thread_id);
}

static void x104_async_worker_unregister(x104_async_item *item)
{
    unsigned index;
    x104_lock(&x104_async_st.lock);
    for (index = 0; index < X104_ASYNC_MAX_WORKERS; ++index) {
        if (x104_async_st.workers[index].item == item) {
            x104_async_st.workers[index].tid = 0;
            x104_async_st.workers[index].item = NULL;
        }
    }
    x104_unlock(&x104_async_st.lock);
}

/* the pool item running on the calling thread, NULL if the caller is not a pool worker */
static x104_async_item *x104_async_current_item(void)
{
    DWORD thread = GetCurrentThreadId();
    x104_async_item *found = NULL;
    unsigned index;
    x104_lock(&x104_async_st.lock);
    for (index = 0; index < X104_ASYNC_MAX_WORKERS; ++index) {
        if (x104_async_st.workers[index].tid == thread && x104_async_st.workers[index].item) {
            found = x104_async_st.workers[index].item;
            break;
        }
    }
    x104_unlock(&x104_async_st.lock);
    return found;
}

/* the pool thread of one work item */
static DWORD WINAPI x104_async_worker(void *parameter)
{
    x104_async_item *item = (x104_async_item *)parameter;
    HRESULT ro;
    HRESULT hr = S_OK;
    X104Seh seh;
    BOOL clean = TRUE;
    BOOL invoked = FALSE;
    int final_status = X104_ASYNC_COMPLETED;
    HRESULT final_error = S_OK;
    void *handler = item->handler;
    DWORD elapsed;

    SetThreadPriority(GetCurrentThread(), item->priority > 0 ? X104_THREAD_PRIORITY_BELOW_NORMAL
                                                           : (item->priority < 0 ? X104_ASYNC_THREAD_PRIORITY_LOWEST : X104_THREAD_PRIORITY_NORMAL));
    ro = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(ro) && ro != X104_ASYNC_RPC_E_CHANGED_MODE) {
        x104_logf("X104 async: WARNING item #%ld thread %04lx: RoInitialize(MTA) returned 0x%08lx %s (continuing)", (long)item->id, (unsigned long)GetCurrentThreadId(), (unsigned long)ro,
                  x104_hr_name(ro));
    }
    item->thread_id = GetCurrentThreadId();
    item->start_tick = GetTickCount();
    x104_async_worker_register(item);
    x104_logf("X104 async: item #%ld started on thread %04lx (%lu ms after creation, RoInitialize 0x%08lx)", (long)item->id, (unsigned long)item->thread_id,
              (unsigned long)(item->start_tick - item->create_tick), (unsigned long)ro);

    if (item->status != X104_ASYNC_STARTED) {
        x104_logf("X104 async: item #%ld was cancelled before it started (status %ld): the handler is NOT invoked", (long)item->id, (long)item->status);
    } else {
        invoked = TRUE;
        clean = x104_call_guarded3(((void **)*(void ***)handler)[3], handler, x104_object_iface0(&item->base), NULL, &hr, &seh);
    }
    x104_async_worker_unregister(item);

    if (invoked) {
        if (!clean) {
            final_status = X104_ASYNC_ERROR;
            final_error = E_FAIL;
            x104_logf("X104 async: item #%ld THREW code=%08lx at %p (game rva %08lx) info=%08lx %08lx%s - the action ends as Error (E_FAIL)", (long)item->id, (unsigned long)seh.code,
                      seh.address, (unsigned long)seh.rva, (unsigned long)seh.info0, (unsigned long)seh.info1, seh.code == 0xE06D7363u ? " [C++ exception: see the _CxxThrowException line above]" : "");
        } else if (FAILED(hr)) {
            final_status = X104_ASYNC_ERROR;
            final_error = hr;
            x104_logf("X104 async: item #%ld handler returned the failure 0x%08lx %s - the action ends as Error", (long)item->id, (unsigned long)hr, x104_hr_name(hr));
        }
    } else {
        final_status = X104_ASYNC_CANCELED;
    }
    x104_async_finish(item, final_status, final_error);
    elapsed = GetTickCount() - item->start_tick;
    x104_logf("X104 async: item #%ld finished status=%ld hr=%08lx after %lu ms", (long)item->id, (long)item->status, (unsigned long)(invoked ? hr : S_OK), (unsigned long)elapsed);
    if (handler) x104_raw_release(handler);
    InterlockedDecrement(&x104_async_st.workers_running);
    return 0;
}

/* Start a pool item.  *operation (AddRef'd) is stored BEFORE the thread exists: the game releases it right after the call (createApp 0x1D9FAE) and the worker
 * already uses the action as the Invoke argument. */
static HRESULT x104_async_pool_run(void *handler, int priority, int options, void **operation, const char *kind, ULONG_PTR caller)
{
    x104_async_item *item;
    LONG id;
    DWORD caller_rva = x104_game_rva_of(caller);
    HANDLE thread;
    DWORD thread_id = 0;
    if (operation) *operation = NULL;
    if (!operation) {
        x104_logf("X104 async: ERROR ThreadPool.%s(handler=%p): NULL operation out pointer -> E_POINTER (caller rva %08lx)", kind, handler, (unsigned long)caller_rva);
        return E_POINTER;
    }
    if (!x104_async_valid_delegate(handler)) {
        x104_logf("X104 async: ERROR ThreadPool.%s: handler=%p is not a COM delegate (NULL or unreadable vtable) -> E_POINTER (caller rva %08lx)", kind, handler, (unsigned long)caller_rva);
        return E_POINTER;
    }
    id = InterlockedIncrement(&x104_async_st.pool_serial);
    item = x104_async_item_new(X104_ASYNC_KIND_POOL, priority, options, id, handler, caller_rva);
    if (!item) {
        x104_logf("X104 async: ERROR ThreadPool.%s: could not create item #%ld (out of memory / event) -> E_OUTOFMEMORY", kind, (long)id);
        return E_OUTOFMEMORY;
    }
    x104_logf("X104 async: item #%ld created kind=%s prio=%d opts=%d handler=%p caller=rva %08lx", (long)id, kind, priority, options, handler, (unsigned long)caller_rva);
    x104_lock(&x104_async_st.lock);
    item->all_next = x104_async_st.items;
    x104_async_st.items = item;
    x104_unlock(&x104_async_st.lock);
    x104_raw_addref(handler);                              /* released when the item finishes */
    if (x104_async_st.shutdown_state != 0) {
        x104_logf("X104 async: item #%ld created during shutdown: it is returned Canceled and never started", (long)id);
        __atomic_store_n(&item->status, (LONG)X104_ASYNC_CANCELED, __ATOMIC_RELEASE);
        item->done = 1;
        item->handler = NULL;
        x104_raw_release(handler);
        return x104_object_qi(&item->base, &X104_IID_IAsyncAction, operation);
    }
    {
        HRESULT hr = x104_object_qi(&item->base, &X104_IID_IAsyncAction, operation);
        if (FAILED(hr)) return hr;
    }
    InterlockedIncrement(&x104_async_st.workers_running);
    thread = CreateThread(NULL, X104_ASYNC_STACK_BYTES, x104_async_worker, item, 0, &thread_id);
    if (!thread) {
        DWORD error = GetLastError();
        InterlockedDecrement(&x104_async_st.workers_running);
        x104_logf("X104 async: ERROR item #%ld: CreateThread failed (error %lu) -> E_OUTOFMEMORY", (long)id, (unsigned long)error);
        __atomic_store_n(&item->status, (LONG)X104_ASYNC_ERROR, __ATOMIC_RELEASE);
        item->error = E_OUTOFMEMORY;
        item->done = 1;
        item->handler = NULL;
        x104_raw_release(handler);
        x104_raw_release(*operation);
        *operation = NULL;
        return E_OUTOFMEMORY;
    }
    CloseHandle(thread);
    return S_OK;
}

static HRESULT X104_API x104_async_pool_run_async(void *self, void *handler, void **operation)
{
    (void)self;
    return x104_async_pool_run(handler, X104_ASYNC_PRIO_NORMAL, 0, operation, "RunAsync", (ULONG_PTR)__builtin_return_address(0));
}

static HRESULT X104_API x104_async_pool_run_priority(void *self, void *handler, int priority, void **operation)
{
    (void)self;
    return x104_async_pool_run(handler, priority, 0, operation, "RunWithPriorityAsync", (ULONG_PTR)__builtin_return_address(0));
}

static HRESULT X104_API x104_async_pool_run_priority_options(void *self, void *handler, int priority, int options, void **operation)
{
    (void)self;
    return x104_async_pool_run(handler, priority, options, operation, "RunWithPriorityAndOptionsAsync", (ULONG_PTR)__builtin_return_address(0));
}

X104_VTBL(x104_async_vt_threadpool, IThreadPoolStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IThreadPoolStatics, RunAsync, x104_async_pool_run_async),
    X104_SLOT(IThreadPoolStatics, RunWithPriorityAsync, x104_async_pool_run_priority),
    X104_SLOT(IThreadPoolStatics, RunWithPriorityAndOptionsAsync, x104_async_pool_run_priority_options)
};
static const X104IfaceDesc x104_async_threadpool_ifaces[] = { X104_IFACE(IThreadPoolStatics, x104_async_vt_threadpool) };
X104_CLASS_DEF(x104_async_threadpool_class, "ThreadPoolStatics", L"Windows.System.Threading.ThreadPool", x104_async_threadpool_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_async_threadpool_factory, x104_async_threadpool_class, X104Object)

/* ======================================================================================================================
 * 4. UI queue (x104_ui_post + CoreDispatcher.RunAsync), x104_ui_pump_once
 * ====================================================================================================================== */
static void x104_async_queue_push(x104_async_node *node)
{
    int index = node->priority + 2;
    if (index < 0) index = 0;
    if (index > 3) index = 3;
    node->next = NULL;
    x104_lock(&x104_async_st.lock);
    if (x104_async_st.tail[index]) x104_async_st.tail[index]->next = node;
    else x104_async_st.head[index] = node;
    x104_async_st.tail[index] = node;
    ++x104_async_st.queue_length;
    x104_unlock(&x104_async_st.lock);
    x104_async_wake();
}

/* strict priority (High first), FIFO inside a priority */
static x104_async_node *x104_async_queue_pop(void)
{
    x104_async_node *node = NULL;
    int index;
    x104_lock(&x104_async_st.lock);
    for (index = 3; index >= 0; --index) {
        node = x104_async_st.head[index];
        if (node) {
            x104_async_st.head[index] = node->next;
            if (!node->next) x104_async_st.tail[index] = NULL;
            --x104_async_st.queue_length;
            break;
        }
    }
    x104_unlock(&x104_async_st.lock);
    return node;
}

static BOOL x104_ui_post(void (*function)(void *), void *argument)
{
    x104_async_node *node;
    static volatile LONG refused;
    if (x104.f_quit) {
        X104_LOG_LIMITED(&refused, 10, "X104 async: x104_ui_post(%p) refused: x104_ui_quit already ran (the UI loop is leaving)", (void *)function);
        return FALSE;
    }
    if (!function) {
        x104_logf("X104 async: ERROR x104_ui_post(NULL function) refused");
        return FALSE;
    }
    node = (x104_async_node *)HeapAlloc(GetProcessHeap(), X104_HEAP_ZERO_MEMORY, sizeof(*node));
    if (!node) {
        x104_logf("X104 async: ERROR x104_ui_post(%p): out of memory", (void *)function);
        return FALSE;
    }
    node->fn = function;
    node->arg = argument;
    node->priority = X104_ASYNC_PRIO_NORMAL;
    InterlockedIncrement(&x104_async_st.posted);
    x104_async_queue_push(node);
    return TRUE;
}

/* execute one node on the UI thread (top level of the pass) and free it */
static void x104_async_run_node(x104_async_node *node)
{
    DWORD start = GetTickCount();
    DWORD elapsed;
    X104Seh seh;
    if (node->action) {
        x104_async_item *item = node->action;
        BOOL run;
        HRESULT hr = S_OK;
        BOOL clean = TRUE;
        x104_lock(&item->lock);
        run = item->status == X104_ASYNC_STARTED;
        x104_unlock(&item->lock);
        if (run) {
            clean = x104_call_guarded3(((void **)*(void ***)node->handler)[3], node->handler, NULL, NULL, &hr, &seh);
            if (!clean) {
                x104_async_log_seh("a CoreDispatcher.RunAsync handler", &seh);
                x104_async_finish(item, X104_ASYNC_ERROR, E_FAIL);
            } else if (FAILED(hr)) {
                x104_logf("X104 async: dispatched handler %p (RunAsync #%ld) returned 0x%08lx %s", node->handler, (long)item->id, (unsigned long)hr, x104_hr_name(hr));
                x104_async_finish(item, X104_ASYNC_ERROR, hr);
            } else {
                x104_async_finish(item, X104_ASYNC_COMPLETED, S_OK);
            }
        } else {
            x104_logf("X104 async: dispatched handler %p (RunAsync #%ld) skipped: its action was cancelled (status %ld)", node->handler, (long)item->id,
                      (long)item->status);
            x104_async_finish(item, X104_ASYNC_CANCELED, S_OK);
        }
        item->handler = NULL;
        x104_raw_release(node->handler);
    } else if (node->fn) {
        if (!x104_async_call_fn_guarded(node->fn, node->arg, &seh)) {
            x104_logf("X104 async: a UI queue item (function %p, rva %08lx) threw:", (void *)node->fn, (unsigned long)x104_game_rva_of((ULONG_PTR)node->fn));
            x104_async_log_seh("a UI queue item", &seh);
        }
    }
    elapsed = GetTickCount() - start;
    if (elapsed > X104_ASYNC_SLOW_HANDLER_MS) {
        x104_logf("X104 async: dispatched handler took %lu ms (%s %p)", (unsigned long)elapsed, node->action ? "RunAsync handler" : "queue item function", node->action ? node->handler : (void *)node->fn);
    }
    InterlockedIncrement(&x104_async_st.executed);
    HeapFree(GetProcessHeap(), 0, node);
}

/* ---- DispatcherTimer delivery (part of the pump) ---- */
/* sender = the timer's IInspectable view, args = NULL (EventHandler<Object>); every handler guarded and rate-limited in the log */
static unsigned x104_async_invoke_tick(x104_async_timer *timer)
{
    void *snapshot[X104_EVENT_MAX];
    unsigned count = 0;
    unsigned index;
    void *sender = x104_object_iface0(&timer->base);
    x104_lock(&timer->tick.lock);
    for (index = 0; index < X104_EVENT_MAX; ++index) {
        if (timer->tick.handler[index]) snapshot[count++] = timer->tick.handler[index];
    }
    for (index = 0; index < count; ++index) x104_raw_addref(snapshot[index]);
    x104_unlock(&timer->tick.lock);
    InterlockedIncrement(&timer->tick.invocations);
    for (index = 0; index < count; ++index) {
        HRESULT hr = S_OK;
        X104Seh seh;
        if (!x104_call_guarded3(((void **)*(void ***)snapshot[index])[3], snapshot[index], sender, NULL, &hr, &seh)) {
            LONG number = InterlockedIncrement(&timer->tick_exceptions);
            if (number <= 10 || (number % 100) == 0) {
                x104_logf("X104 async: timer #%ld Tick handler %p threw (exception #%ld):", (long)timer->number, snapshot[index], (long)number);
                x104_async_log_seh("a DispatcherTimer Tick handler", &seh);
            }
        } else if (FAILED(hr)) {
            static volatile LONG failed;
            X104_LOG_LIMITED(&failed, 20, "X104 async: timer #%ld Tick handler %p returned 0x%08lx %s", (long)timer->number, snapshot[index], (unsigned long)hr, x104_hr_name(hr));
        }
        x104_raw_release(snapshot[index]);
    }
    return count;
}

static void x104_async_run_timers(void)
{
    unsigned index;
    BOOL gate = x104.f_size_changed_done != 0;
    BOOL waiting = FALSE;
    for (index = 0; index < X104_ASYNC_MAX_TIMERS; ++index) {
        x104_async_timer *timer;
        BOOL deliver = FALSE;
        DWORD now = GetTickCount();
        x104_lock(&x104_async_st.timer_lock);
        timer = x104_async_st.timers[index];
        if (timer && timer->enabled) {
            if (!gate) {
                waiting = TRUE;
            } else if (!timer->gate_seen) {                 /* the gate just opened: the first tick is due one interval from now (R3) */
                timer->gate_seen = 1;
                timer->next_due = now + timer->interval_ms;
            } else if ((LONG)(now - timer->next_due) >= 0) {
                timer->next_due = now + timer->interval_ms;  /* a late UI thread never bursts */
                deliver = TRUE;
            }
        }
        x104_unlock(&x104_async_st.timer_lock);
        if (deliver) {
            LONG number = InterlockedIncrement(&timer->ticks);
            unsigned handlers;
            if (number == 1) x104_logf("X104 async: timer #%ld first Tick delivered (interval %lu ms, %lu handler(s), size-changed gate open)", (long)timer->number, (unsigned long)timer->interval_ms,
                                       (unsigned long)x104_event_count(&timer->tick));
            else if ((number % 200) == 0) x104_logf("X104 async: timer #%ld Tick #%ld delivered", (long)timer->number, (long)number);
            handlers = x104_async_invoke_tick(timer);
            if (handlers == 0 && number == 1) x104_logf("X104 async: timer #%ld has no Tick handler registered", (long)timer->number);
        }
    }
    if (waiting) {
        DWORD now = GetTickCount();
        if (!x104_async_st.gate_log_tick || now - x104_async_st.gate_log_tick >= 2000u) {
            x104_async_st.gate_log_tick = now ? now : 1;
            x104_logf("X104 async: timer tick gate closed (no SizeChanged yet)");
        }
    }
}

/* ms the loop may sleep: <= 10, shorter when a timer is due earlier, 1 when the queue still holds items */
static DWORD x104_async_wait_ms(void)
{
    DWORD wait = 10;
    unsigned index;
    DWORD now = GetTickCount();
    if (x104_async_st.queue_length > 0) return 1;
    if (x104_app_rendering_active()) return 1;     /* CompositionTarget.Rendering frames (REDCON 1): the game's limiter + Present(1,0) pace them */
    if (!x104.f_size_changed_done) return wait;
    x104_lock(&x104_async_st.timer_lock);
    for (index = 0; index < X104_ASYNC_MAX_TIMERS; ++index) {
        x104_async_timer *timer = x104_async_st.timers[index];
        if (timer && timer->enabled && timer->gate_seen) {
            LONG remaining = (LONG)(timer->next_due - now);
            if (remaining < 1) remaining = 1;
            if ((DWORD)remaining < wait) wait = (DWORD)remaining;
        }
    }
    x104_unlock(&x104_async_st.timer_lock);
    return wait;
}

/* UI thread, called only by x104_ui_pass(): queued items (<= 64), then the due timers.  Never re-entered. */
static void x104_ui_pump_once(void)
{
    int processed = 0;
    x104_async_node *node;
    if (x104.ui_thread_id && !x104_on_ui_thread()) {
        static volatile LONG wrong_thread;
        X104_LOG_LIMITED(&wrong_thread, 5, "X104 async: ERROR x104_ui_pump_once called on thread %04lx, the UI thread is %04lx - ignored", (unsigned long)GetCurrentThreadId(),
                         (unsigned long)x104.ui_thread_id);
        return;
    }
    if (__sync_val_compare_and_swap(&x104_async_st.in_pump, 0, 1) != 0) return;
    while (processed < X104_ASYNC_PUMP_CAP && (node = x104_async_queue_pop()) != NULL) {
        x104_async_run_node(node);
        ++processed;
    }
    if (processed >= X104_ASYNC_PUMP_CAP && x104_async_st.queue_length > 0) x104_async_wake();   /* the rest runs in the next pass, after the message pump had a turn */
    x104_async_run_timers();
    InterlockedIncrement(&x104_async_st.passes);
    InterlockedExchange(&x104_async_st.in_pump, 0);
}

/* ======================================================================================================================
 * 5. CoreDispatcher
 * ====================================================================================================================== */
static HRESULT X104_API x104_async_disp_get_has_thread_access(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_on_ui_thread() ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_async_disp_process_events(void *self, int options)
{
    x104_async_item *item;
    const char *reason = "?";
    static volatile LONG logged;
    (void)self;
    if (x104_on_ui_thread()) {
        if (options == 2) {
            x104_logf("X104 async: ERROR CoreDispatcher.ProcessEvents(ProcessUntilQuit) called on the UI thread - refused (S_OK): it would block the host loop");
            return S_OK;
        }
        /* R1: a fake called by the game never runs queued handlers (the caller may hold the AppMain lock).  The game only calls ProcessEvents(ProcessUntilQuit) on pool
         * threads; on the UI thread the loop's own pass delivers the queue, so this is a logged no-op. */
        X104_LOG_LIMITED(&logged, 10, "X104 async: CoreDispatcher.ProcessEvents(%d) on the UI thread: nothing is run inside a game call (R1), the UI loop delivers the queue", options);
        return S_OK;
    }
    if (options != 2) {
        X104_LOG_LIMITED(&logged, 10, "X104 async: CoreDispatcher.ProcessEvents(%d) on pool thread %04lx: nothing to process, returning S_OK", options, (unsigned long)GetCurrentThreadId());
        return S_OK;
    }
    item = x104_async_current_item();
    x104_logf("X104 async: ProcessEvents(ProcessUntilQuit) blocks thread %04lx (item #%ld) until Cancel or shutdown", (unsigned long)GetCurrentThreadId(), item ? (long)item->id : -1L);
    for (;;) {
        if (item) {
            /* x104_async_shutdown cancels every pool item (status 2 + cancel event) before it does anything else, so a pool item only needs its own status/event */
            if (item->status != X104_ASYNC_STARTED) { reason = "the item's status is no longer Started (Cancel / shutdown)"; break; }
            if (WaitForSingleObject(item->cancel_event, X104_ASYNC_PROCESS_POLL_MS) == 0) { reason = "Cancel / shutdown"; break; }
        } else {
            if (x104_async_st.shutdown_state != 0 || x104.f_quit) { reason = "shutdown"; break; }
            if (x104_async_st.quit_event) {
                if (WaitForSingleObject(x104_async_st.quit_event, X104_ASYNC_PROCESS_POLL_MS) == 0) { reason = "shutdown event"; break; }
            } else {
                Sleep(X104_ASYNC_PROCESS_POLL_MS);
            }
        }
    }
    x104_logf("X104 async: ProcessEvents(ProcessUntilQuit) on thread %04lx (item #%ld) returns: %s", (unsigned long)GetCurrentThreadId(), item ? (long)item->id : -1L, reason);
    return S_OK;
}

/* queue a handler; ALWAYS asynchronous (R1/R11), also from the UI thread */
static HRESULT x104_async_dispatch(int priority, void *handler, void **action, const char *kind, ULONG_PTR caller)
{
    x104_async_item *item;
    x104_async_node *node;
    LONG number;
    static volatile LONG logged;
    if (action) *action = NULL;
    if (!action) {
        x104_logf("X104 async: ERROR CoreDispatcher.%s(handler=%p): NULL action out pointer -> E_POINTER (caller rva %08lx)", kind, handler, (unsigned long)x104_game_rva_of(caller));
        return E_POINTER;
    }
    if (!x104_async_valid_delegate(handler)) {
        x104_logf("X104 async: ERROR CoreDispatcher.%s: handler=%p is not a COM delegate -> E_POINTER (caller rva %08lx)", kind, handler, (unsigned long)x104_game_rva_of(caller));
        return E_POINTER;
    }
    if (priority < X104_ASYNC_PRIO_IDLE || priority > X104_ASYNC_PRIO_HIGH) {
        x104_logf("X104 async: WARNING CoreDispatcher.%s priority %d out of range, clamped", kind, priority);
        priority = priority < X104_ASYNC_PRIO_IDLE ? X104_ASYNC_PRIO_IDLE : X104_ASYNC_PRIO_HIGH;
    }
    number = InterlockedIncrement(&x104_async_st.dispatch_serial);
    item = x104_async_item_new(X104_ASYNC_KIND_DISPATCH, priority, 0, number, handler, x104_game_rva_of(caller));
    node = (x104_async_node *)HeapAlloc(GetProcessHeap(), X104_HEAP_ZERO_MEMORY, sizeof(*node));
    if (!item || !node) {
        if (node) HeapFree(GetProcessHeap(), 0, node);
        x104_logf("X104 async: ERROR CoreDispatcher.%s #%ld: out of memory -> E_OUTOFMEMORY", kind, (long)number);
        return E_OUTOFMEMORY;
    }
    X104_LOG_LIMITED(&logged, 40, "X104 async: RunAsync #%ld prio=%d from thread %04lx (ui=%d) handler=%p caller=rva %08lx (%s)", (long)number, priority, (unsigned long)GetCurrentThreadId(),
                     (int)x104_on_ui_thread(), handler, (unsigned long)item->caller_rva, kind);
    if (x104_async_st.shutdown_state != 0 || x104.f_quit) {
        x104_logf("X104 async: RunAsync #%ld during shutdown: returned Canceled, the handler is not queued", (long)number);
        item->handler = NULL;
        HeapFree(GetProcessHeap(), 0, node);
        x104_async_finish(item, X104_ASYNC_CANCELED, S_OK);
        return x104_object_qi(&item->base, &X104_IID_IAsyncAction, action);
    }
    x104_raw_addref(handler);                              /* released after the handler ran (or was skipped) */
    node->action = item;
    node->handler = handler;
    node->priority = priority;
    {
        HRESULT hr = x104_object_qi(&item->base, &X104_IID_IAsyncAction, action);
        if (FAILED(hr)) {
            x104_raw_release(handler);
            HeapFree(GetProcessHeap(), 0, node);
            return hr;
        }
    }
    InterlockedIncrement(&x104_async_st.posted);
    x104_async_queue_push(node);
    return S_OK;
}

static HRESULT X104_API x104_async_disp_run_async(void *self, int priority, void *handler, void **action)
{
    (void)self;
    return x104_async_dispatch(priority, handler, action, "RunAsync", (ULONG_PTR)__builtin_return_address(0));
}

static HRESULT X104_API x104_async_disp_run_idle_async(void *self, void *handler, void **action)
{
    (void)self;
    return x104_async_dispatch(X104_ASYNC_PRIO_IDLE, handler, action, "RunIdleAsync", (ULONG_PTR)__builtin_return_address(0));
}

X104_VTBL(x104_async_vt_dispatcher, ICoreDispatcher) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreDispatcher, get_HasThreadAccess, x104_async_disp_get_has_thread_access),
    X104_SLOT(ICoreDispatcher, ProcessEvents, x104_async_disp_process_events),
    X104_SLOT(ICoreDispatcher, RunAsync, x104_async_disp_run_async),
    X104_SLOT(ICoreDispatcher, RunIdleAsync, x104_async_disp_run_idle_async)
};
static const X104IfaceDesc x104_async_dispatcher_ifaces[] = { X104_IFACE(ICoreDispatcher, x104_async_vt_dispatcher) };
X104_CLASS_DEF(x104_async_dispatcher_class, "CoreDispatcher", L"Windows.UI.Core.CoreDispatcher", x104_async_dispatcher_ifaces, X104_CLASS_AGILE, NULL);

/* ======================================================================================================================
 * 6. DispatcherTimer
 * ====================================================================================================================== */
/* TimeSpan ticks (100 ns) -> ms, rounded up, at least 1.  Double math: no 64-bit division helper needed. */
static DWORD x104_async_ticks_to_ms(LONGLONG ticks)
{
    double ms = ((double)ticks + 9999.0) / 10000.0;
    if (ms > 2147483647.0) ms = 2147483647.0;
    return ms < 1.0 ? 1u : (DWORD)ms;
}

static HRESULT X104_API x104_async_timer_get_interval(void *self, LONGLONG *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(x104_async_timer, self)->interval_ticks;
    return S_OK;
}

static HRESULT X104_API x104_async_timer_put_interval(void *self, LONGLONG interval)
{
    x104_async_timer *timer = X104_THIS(x104_async_timer, self);
    DWORD ms;
    if (interval < 0) {
        x104_logf("X104 async: ERROR timer #%ld put_Interval(%08lx%08lx): negative TimeSpan -> E_INVALIDARG", (long)timer->number, (unsigned long)(interval >> 32), (unsigned long)(UINT32)interval);
        return E_INVALIDARG;
    }
    ms = x104_async_ticks_to_ms(interval);
    x104_lock(&x104_async_st.timer_lock);
    timer->interval_ticks = interval;
    timer->interval_ms = ms;
    if (timer->enabled) timer->next_due = GetTickCount() + ms;
    x104_unlock(&x104_async_st.timer_lock);
    x104_logf("X104 async: timer #%ld interval = %08lx%08lx ticks (%lu ms)", (long)timer->number, (unsigned long)(interval >> 32), (unsigned long)(UINT32)interval, (unsigned long)ms);
    x104_async_wake();
    return S_OK;
}

static HRESULT X104_API x104_async_timer_get_enabled(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(x104_async_timer, self)->enabled ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_async_timer_add_tick(void *self, void *handler, LONGLONG *token)
{
    return x104_event_add(&X104_THIS(x104_async_timer, self)->tick, handler, token);
}

static HRESULT X104_API x104_async_timer_remove_tick(void *self, LONGLONG token)
{
    return x104_event_remove(&X104_THIS(x104_async_timer, self)->tick, token);
}

/* Start: enabled, next tick one interval from now; the UI pass delivers it (never inside this call) once the size-changed gate is open */
static HRESULT X104_API x104_async_timer_start(void *self)
{
    x104_async_timer *timer = X104_THIS(x104_async_timer, self);
    LONG was;
    DWORD ms;
    x104_lock(&x104_async_st.timer_lock);
    was = timer->enabled;
    timer->enabled = 1;
    ms = timer->interval_ms ? timer->interval_ms : 1;
    timer->next_due = GetTickCount() + ms;
    x104_unlock(&x104_async_st.timer_lock);
    x104_logf("X104 async: timer #%ld Start (interval %lu ms, was enabled=%ld, gate state: %s) on thread %04lx", (long)timer->number, (unsigned long)ms, (long)was,
              x104.f_size_changed_done ? "open" : "waiting for the first SizeChanged", (unsigned long)GetCurrentThreadId());
    x104_flag_set(&x104.f_timer_started, "f_timer_started: DispatcherTimer.Start");
    x104_async_wake();
    return S_OK;
}

/* Stop: state only (the game stops its timer from inside its own Tick).  Stop while the game is ready = the READY tick. */
static HRESULT X104_API x104_async_timer_stop(void *self)
{
    x104_async_timer *timer = X104_THIS(x104_async_timer, self);
    LONG was;
    BOOL ready;
    x104_lock(&x104_async_st.timer_lock);
    was = timer->enabled;
    timer->enabled = 0;
    x104_unlock(&x104_async_st.timer_lock);
    ready = x104_game_is_ready();
    x104_logf("X104 async: timer #%ld Stop (was enabled=%ld, game ready (AppMain+0x3C)=%d) on thread %04lx", (long)timer->number, (long)was, (int)ready, (unsigned long)GetCurrentThreadId());
    if (ready) x104_flag_set(&x104.f_ready_tick, "f_ready_tick - READY tick: the game stopped its timer (AppMain+0x3C==1)");
    return S_OK;
}

X104_VTBL(x104_async_vt_timer, IDispatcherTimer) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IDispatcherTimer, get_Interval, x104_async_timer_get_interval),
    X104_SLOT(IDispatcherTimer, put_Interval, x104_async_timer_put_interval),
    X104_SLOT(IDispatcherTimer, get_IsEnabled, x104_async_timer_get_enabled),
    X104_SLOT(IDispatcherTimer, add_Tick, x104_async_timer_add_tick),
    X104_SLOT(IDispatcherTimer, remove_Tick, x104_async_timer_remove_tick),
    X104_SLOT(IDispatcherTimer, Start, x104_async_timer_start),
    X104_SLOT(IDispatcherTimer, Stop, x104_async_timer_stop)
};
static const X104IfaceDesc x104_async_timer_ifaces[] = { X104_IFACE(IDispatcherTimer, x104_async_vt_timer) };
X104_CLASS_DEF(x104_async_timer_class, "DispatcherTimer", L"Windows.UI.Xaml.DispatcherTimer", x104_async_timer_ifaces, X104_CLASS_AGILE, NULL);

/* IDispatcherTimerFactory::CreateInstance(outer, &inner, &timer): outer is NULL in the game, inner may be NULL; *timer = the IDispatcherTimer view */
static HRESULT X104_API x104_async_timerfactory_create(void *self, void *outer, void **inner, void **timer)
{
    X104Object *object = NULL;
    x104_async_timer *created;
    HRESULT hr;
    unsigned slot;
    (void)self;
    if (!timer) {
        x104_logf("X104 async: ERROR IDispatcherTimerFactory.CreateInstance: NULL timer out pointer -> E_POINTER");
        return E_POINTER;
    }
    *timer = NULL;
    x104_lock(&x104_async_st.timer_lock);
    slot = x104_async_st.timer_count;
    if (slot < X104_ASYNC_MAX_TIMERS) x104_async_st.timer_count = slot + 1;
    x104_unlock(&x104_async_st.timer_lock);
    if (slot >= X104_ASYNC_MAX_TIMERS) {
        x104_logf("X104 async: ERROR IDispatcherTimerFactory.CreateInstance: the timer table is full (%u) -> E_OUTOFMEMORY", (unsigned)X104_ASYNC_MAX_TIMERS);
        return E_OUTOFMEMORY;
    }
    hr = x104_aggregate_create(&x104_async_timer_class, sizeof(x104_async_timer), outer, &X104_IID_IDispatcherTimer, inner, timer, &object, "IDispatcherTimerFactory.CreateInstance");
    if (FAILED(hr) || !object) {
        x104_logf("X104 async: ERROR IDispatcherTimerFactory.CreateInstance failed: 0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        return FAILED(hr) ? hr : E_FAIL;
    }
    created = (x104_async_timer *)object;
    created->number = (LONG)slot + 1;
    created->interval_ms = 1;                              /* a real DispatcherTimer starts with a zero TimeSpan */
    x104_event_init(&created->tick, "DispatcherTimer.Tick");
    x104_lock(&x104_async_st.timer_lock);
    x104_async_st.timers[slot] = created;
    x104_unlock(&x104_async_st.timer_lock);
    x104_logf("X104 async: timer #%ld created (outer=%p, obj=%p, initially stopped, interval 0)", (long)created->number, outer, (void *)created);
    return S_OK;
}

X104_VTBL(x104_async_vt_timerfactory, IDispatcherTimerFactory) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IDispatcherTimerFactory, CreateInstance, x104_async_timerfactory_create)
};
static const X104IfaceDesc x104_async_timerfactory_ifaces[] = { X104_IFACE(IDispatcherTimerFactory, x104_async_vt_timerfactory) };
X104_CLASS_DEF(x104_async_timerfactory_class, "DispatcherTimerFactory", L"Windows.UI.Xaml.DispatcherTimer", x104_async_timerfactory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_async_timerfactory, x104_async_timerfactory_class, X104Object)

/* ======================================================================================================================
 * 7. contract functions
 * ====================================================================================================================== */
static BOOL x104_async_install(BYTE *image)
{
    (void)image;
    x104_logf("X104 async: install ok (nothing to patch in the game image)");
    return TRUE;
}

static BOOL x104_async_init(HWND window)
{
    if (!x104.ui_thread_id) {
        x104.ui_thread_id = GetCurrentThreadId();
        x104_logf("X104 async: WARNING x104.ui_thread_id was 0, set to the calling thread %04lx", (unsigned long)x104.ui_thread_id);
    }
    x104_async_st.wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    x104_async_st.quit_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!x104_async_st.wake_event || !x104_async_st.quit_event) {
        x104_logf("X104 async: ERROR init: CreateEvent failed (wake=%p quit=%p, error %lu)", x104_async_st.wake_event, x104_async_st.quit_event, (unsigned long)GetLastError());
        return FALSE;
    }
    x104_async_st.dispatcher = x104_object_new(&x104_async_dispatcher_class, sizeof(X104Object));
    if (!x104_async_st.dispatcher) {
        x104_logf("X104 async: ERROR init: could not create the CoreDispatcher object");
        return FALSE;
    }
    if (!window) {
        x104_logf("X104 async: WARNING init without a window: no modal-loop pump timer (queue and timers run only while the loop spins)");
    } else if (!SetTimer(window, X104_TIMER_PUMP, 10, NULL)) {
        x104_logf("X104 async: WARNING init: SetTimer(modal-loop pump) failed (error %lu): the queue and the timers stop during window move/size drags", (unsigned long)GetLastError());
    }
    x104_async_st.initialised = 1;
    x104_logf("X104 async: init ok (ui thread %04lx, dispatcher %p)", (unsigned long)x104.ui_thread_id, (void *)x104_async_st.dispatcher);
    return TRUE;
}

static void x104_async_shutdown(void)
{
    x104_async_item *item;
    x104_async_node *node;
    DWORD start;
    int running = 0;
    int dropped_fn = 0;
    int dropped_actions = 0;
    unsigned index;
    char list[200];
    unsigned used = 0;
    if (__sync_val_compare_and_swap(&x104_async_st.shutdown_state, 0, 1) != 0) return;     /* idempotent */
    start = GetTickCount();
    x104_logf("X104 async: shutdown begins (workers running %ld, queued %ld, posted %ld, executed %ld)", (long)x104_async_st.workers_running, (long)x104_async_st.queue_length,
              (long)x104_async_st.posted, (long)x104_async_st.executed);
    if (x104_async_st.quit_event) SetEvent(x104_async_st.quit_event);

    /* cancel every running pool action: status 2 at once, the cancel event wakes a blocked ProcessEvents */
    x104_lock(&x104_async_st.lock);
    item = x104_async_st.items;
    x104_unlock(&x104_async_st.lock);
    for (; item; item = item->all_next) {
        x104_lock(&item->lock);
        if (item->status == X104_ASYNC_STARTED && !item->done) {
            __atomic_store_n(&item->status, (LONG)X104_ASYNC_CANCELED, __ATOMIC_RELEASE);
            ++running;
        }
        x104_unlock(&item->lock);
        if (item->cancel_event) SetEvent(item->cancel_event);
    }

    /* queued items that never ran: dispatcher actions end Canceled, posted functions are dropped (counted) */
    while ((node = x104_async_queue_pop()) != NULL) {
        if (node->action) {
            node->action->handler = NULL;
            x104_async_finish(node->action, X104_ASYNC_CANCELED, S_OK);
            x104_raw_release(node->handler);
            ++dropped_actions;
        } else {
            ++dropped_fn;
        }
        HeapFree(GetProcessHeap(), 0, node);
    }

    /* timers stop */
    x104_lock(&x104_async_st.timer_lock);
    for (index = 0; index < X104_ASYNC_MAX_TIMERS; ++index) {
        if (x104_async_st.timers[index]) x104_async_st.timers[index]->enabled = 0;
    }
    x104_unlock(&x104_async_st.timer_lock);

    /* wait at most 2 s IN TOTAL for the workers (never for the init job beyond that) */
    while (x104_async_st.workers_running > 0 && GetTickCount() - start < X104_ASYNC_SHUTDOWN_WAIT_MS) Sleep(5);
    list[0] = 0;
    if (x104_async_st.workers_running > 0) {
        x104_lock(&x104_async_st.lock);
        item = x104_async_st.items;
        x104_unlock(&x104_async_st.lock);
        for (; item; item = item->all_next) {
            if (item->thread_id && !item->done && used + 24 < sizeof(list)) used += (unsigned)wsprintfA(list + used, " #%ld", (long)item->id);
            else if (!item->thread_id && !item->done && item->kind == X104_ASYNC_KIND_POOL && used + 28 < sizeof(list)) used += (unsigned)wsprintfA(list + used, " #%ld(unstarted)", (long)item->id);
        }
        x104_logf("X104 async: shutdown: %ld items still running after %lu ms:%s", (long)x104_async_st.workers_running, (unsigned long)(GetTickCount() - start), list);
    } else {
        x104_logf("X104 async: shutdown: all workers finished after %lu ms (cancelled %d running, dropped %d queued functions and %d queued dispatcher actions)",
                  (unsigned long)(GetTickCount() - start), running, dropped_fn, dropped_actions);
    }
    if (x104.window) KillTimer(x104.window, X104_TIMER_PUMP);
    x104_async_st.shutdown_state = 2;
}

static HRESULT x104_async_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    static const X104FactoryDef defs[] = {
        { L"Windows.System.Threading.ThreadPool", x104_async_threadpool_factory },
        { L"Windows.UI.Xaml.DispatcherTimer", x104_async_timerfactory }
    };
    if (!out) return E_POINTER;
    *out = NULL;
    if (!class_name || !iid) return E_POINTER;
    return x104_activate_from_table(defs, (unsigned)ARRAYSIZE(defs), class_name, iid, out);
}

static X104Object *x104_async_dispatcher_object(void)
{
    return x104_async_st.dispatcher;
}

static void x104_ui_quit(int exit_code)
{
    x104.exit_code = exit_code;
    x104_flag_set(&x104.f_quit, "f_quit: x104_ui_quit - the UI loop is leaving");
    InterlockedExchange(&x104.f_loop_exit, 1);
    x104_async_wake();
    if (x104.window) PostMessageW(x104.window, X104_WM_QUIT, (WPARAM)exit_code, 0);
}

static int x104_ui_run_loop(void)
{
    MSG message;
    int drained;
    x104_logf("X104 async: UI loop entered (thread %04lx)", (unsigned long)GetCurrentThreadId());
    for (;;) {
        drained = 0;
        while (drained < X104_ASYNC_DRAIN_CAP && PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            ++drained;
            if (message.message == X104_WM_QUIT) {
                x104.exit_code = (int)message.wParam;
                goto leave;
            }
            if (message.message == WM_QUIT) {
                if (!x104.f_loop_exit) x104.exit_code = (int)message.wParam;      /* a code stored by x104_ui_quit wins over PostQuitMessage(0) */
                goto leave;
            }
            if (message.message == X104_WM_WAKE) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);                                           /* window proc -> units: they only update state / post (R1) */
            if (x104.f_loop_exit) goto leave;                                     /* the window proc consumed an X104_WM_QUIT dispatched by a nested modal loop */
        }
        if (x104.f_loop_exit) goto leave;
        x104_ui_pass();                                                           /* core: x104_ui_pump_once() + the panel/input/app ticks + progress bump */
        if (x104.f_loop_exit) goto leave;                                         /* after EVERY pass: x104_ui_quit sets it from any thread */
        MsgWaitForMultipleObjectsEx(x104_async_st.wake_event ? 1u : 0u, x104_async_st.wake_event ? &x104_async_st.wake_event : NULL, x104_async_wait_ms(), X104_QS_ALLINPUT, X104_ASYNC_MWMO_INPUTAVAILABLE);
    }
leave:
    x104_logf("X104 async: UI loop left (code %d; passes %ld, queue items executed %ld, still queued %ld)", x104.exit_code, (long)x104_async_st.passes, (long)x104_async_st.executed,
              (long)x104_async_st.queue_length);
    return x104.exit_code;
}

#endif /* WIN32_HOST_ARM32_0104_ASYNC_INL */
