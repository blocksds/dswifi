// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifdef DSWIFI_ENABLE_LWIP

#include <nds.h>

#include "lwip/sys.h"
#include "lwip/tcpip.h"

#include "arm9/ipc.h"
#include "arm9/lwip/lwip_nds.h"
#include "arm9/wifi_arm9.h"

static volatile uint32_t wifi_sys_ms_counter;

void sys_msleep(u32_t ms)
{
    // This function doesn't seem to be used at all, so it doesn't make much
    // sense to optimize it with waits for interrupts.

    uint32_t start_time = wifi_sys_ms_counter;

    while (1)
    {
        uint32_t diff = wifi_sys_ms_counter - start_time;
        if (diff >= ms)
            break;
    }
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
                            int stacksize, int prio)
{
    (void)name;
    (void)prio;

    cothread_t ret = cothread_create((cothread_entrypoint_t)(void *)thread, arg,
                                     stacksize, COTHREAD_DETACHED);
    if (ret == -1)
        libndsCrash(__func__);

    return ret;
}

static void my_sys_counter(void)
{
    wifi_sys_ms_counter++;
}

static int wifi_update_thread(void *arg)
{
    (void)arg;

    while (1)
    {
        Wifi_Update();

        if (WifiData->curLibraryMode == DSWIFI_INTERNET)
        {
            if (wifi_lwip_enabled)
            {
                sys_check_timeouts();
            }
        }

        // Wait until we receive any FIFO message from the ARM7. The ARM7 sends
        // sync messages when there are new messages for the ARM9 to handle.
        //
        // This works even if there is some issue with the FIFO library (like a
        // missed sync event) because the ARM7 sends a message to the ARM9 with
        // the key input every frame, which would cause this thread to wake up.
        cothread_yield_irq(IRQ_RECV_FIFO);

        // TODO: We need a way to end this thread when DSWiFI is deinitialized
    }

    return 0;
}

void sys_init(void)
{
    // Setup timer 3 to fire 1000 times per second (1 ms per tick). We don't
    // need a resolution of 1 ms for the library.
    timerStart(3, ClockDivider_64, TIMER_FREQ_64(1000), my_sys_counter);

    cothread_t ret = cothread_create(wifi_update_thread, NULL, 8 * 1024,
                                     COTHREAD_DETACHED);
    if (ret == -1)
        libndsCrash(__func__);
}

u32_t sys_now(void)
{
    return wifi_sys_ms_counter;
}

// ============================================================================

sys_prot_t sys_arch_protect(void)
{
    return enterCriticalSection();
}

void sys_arch_unprotect(sys_prot_t pval)
{
    leaveCriticalSection(pval);
}

// ============================================================================

#define MBOX_NUMBER 10
#define MBOX_MAGIC  0xB10C4500 // Leave the low byte empty to fit an index

typedef struct
{
    size_t size;
    size_t in_ptr, out_ptr;
    uintptr_t arr[]; // Each entry in the array is a pointer
}
sys_mbox_internal;

static sys_mbox_internal *dswifi_mbox[MBOX_NUMBER];

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (mbox == NULL)
        return ERR_MEM;

    int chosen = -1;
    for (int i = 0; i < MBOX_NUMBER; i++)
    {
        if (dswifi_mbox[i] == NULL)
        {
            chosen = i;
            break;
        }
    }
    if (chosen == -1)
        return ERR_MEM;

    sys_mbox_internal *m = malloc(sizeof(sys_mbox_internal) + sizeof(uintptr_t) * size);
    if (m == NULL)
    {
        *mbox = 0;
        return ERR_MEM;
    }

    dswifi_mbox[chosen] = m;

    m->size = size;
    m->in_ptr = 0;
    m->out_ptr = 0;

    *mbox = MBOX_MAGIC | chosen;

    return ERR_OK;
}

static sys_mbox_internal *get_mbox(sys_mbox_t value)
{
    unsigned int index = value & 0xFF;
    unsigned int magic = value & 0xFFFFFF00;

    if (magic != MBOX_MAGIC)
        return NULL;

    if (index > MBOX_NUMBER)
        return NULL;

    return dswifi_mbox[index];
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    err_t ret = ERR_OK;

    int oldIME = enterCriticalSection();

    sys_mbox_internal *m = get_mbox(*mbox);

    if (m == NULL)
    {
        ret = ERR_VAL;
    }
    else
    {
        size_t next_in_ptr = m->in_ptr + 1;
        if (next_in_ptr == m->size)
            next_in_ptr = 0;

        if (next_in_ptr == m->out_ptr)
        {
            ret = ERR_MEM;
        }
        else
        {
            m->arr[m->in_ptr] = (uintptr_t)msg;
            m->in_ptr = next_in_ptr;
        }
    }

    leaveCriticalSection(oldIME);

    // Wake up all threads using this mailbox, even if we fail to add a new
    // entry to the mailbox.
    cothread_send_signal((uintptr_t)mbox);

    return ret;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    while (1)
    {
        err_t err = sys_mbox_trypost(mbox, msg);
        if (err == ERR_OK)
            break;

        // If this happens, the mailbox is invalid. We have lost this message
        if (err == ERR_VAL)
            break;

        // sys_mbox_trypost() sends a signal to wake up all threads using this
        // mailbox. Go to sleep until one of them wakes us up and we can try to
        // post the message again.
        cothread_yield_signal((uintptr_t)mbox);
    }
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    u32_t ret = 0;

    int oldIME = enterCriticalSection();

    sys_mbox_internal *m = get_mbox(*mbox);

    if (m == NULL)
    {
        ret = SYS_MBOX_EMPTY;
    }
    else
    {
        if (m->in_ptr == m->out_ptr)
        {
            ret = SYS_MBOX_EMPTY;
        }
        else
        {
            size_t next_out_ptr = m->out_ptr + 1;
            if (next_out_ptr == m->size)
                next_out_ptr = 0;

            // "msg" can be NULL. In that case, drop the message
            if (msg)
                *msg = (void *)m->arr[m->out_ptr];

            m->out_ptr = next_out_ptr;
        }
    }

    leaveCriticalSection(oldIME);

    // Wake up all threads using this mailbox even if the mailbox was empty.
    cothread_send_signal((uintptr_t)mbox);

    return ret;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    uint32_t start_time = wifi_sys_ms_counter;

    while (1)
    {
        err_t err = sys_arch_mbox_tryfetch(mbox, msg);
        if ((unsigned long)err != SYS_MBOX_EMPTY)
            break;

        // A timeout of 0 means that this function doesn't timeout
        if (timeout > 0)
        {
            uint32_t diff = wifi_sys_ms_counter - start_time;
            if (diff >= timeout)
                return SYS_ARCH_TIMEOUT;
        }

        // sys_arch_mbox_tryfetch() sends a signal to wake up all threads using
        // this mailbox. Go to sleep until one of them wakes us up and we can
        // try to fetch a message again.
        cothread_yield_signal((uintptr_t)mbox);
    }

    return 0;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    unsigned int index = *mbox & 0xFF;
    unsigned int magic = *mbox & 0xFFFFFF00;

    if (magic != MBOX_MAGIC)
        return;

    if (index > MBOX_NUMBER)
        return;

    int oldIME = enterCriticalSection();

    if (dswifi_mbox[index] != NULL)
    {
        free(dswifi_mbox[index]);
        dswifi_mbox[index] = NULL;
        *mbox = 0; // Invalidate handle
    }

    leaveCriticalSection(oldIME);
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    if (mbox == NULL)
        return 0;

    sys_mbox_internal *m = get_mbox(*mbox);
    if (m == NULL)
        return 0;

    return 1;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    // Do nothing, sys_mbox_free() is always called before this function.
    (void)mbox;
}

// ============================================================================

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    if (comutex_init(mutex))
        return ERR_OK;

    return ERR_MEM;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    comutex_acquire(mutex);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    comutex_release(mutex);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    // Nothing to do
    (void)mutex;
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
    if (mutex == NULL)
        return 0;

    return 1;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    // Nothing to do
    (void)mutex;
}

// ============================================================================

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    if (cosema_init(sem, count))
        return ERR_OK;

    return ERR_MEM;
}

void sys_sem_signal(sys_sem_t *sem)
{
    cosema_signal(sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    if (timeout == 0)
    {
        cosema_wait(sem);
    }
    else
    {
        uint32_t start_time = wifi_sys_ms_counter;

        while (cosema_try_wait(sem) == false)
        {
            uint32_t diff = wifi_sys_ms_counter - start_time;
            if (diff >= timeout)
                return SYS_ARCH_TIMEOUT;

            cothread_yield_signal(cosema_to_signal_id(sem));
        }
    }

    // Return anything other than SYS_ARCH_TIMEOUT on success
    return 0;
}

void sys_sem_free(sys_sem_t *sem)
{
    // Nothing to do
    (void)sem;
}

int sys_sem_valid(sys_sem_t *sem)
{
    if (sem == NULL)
        return 0;

    return 1;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    // Nothing to do
    (void)sem;
}

#endif // DSWIFI_ENABLE_LWIP
