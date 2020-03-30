#include "hooks.h"

exec_binprm_t original_exec_binprm = NULL;
struct buffer_struct buffer_list_calls_exec_binprm;
static struct mutex g_mutex;
static bool g_unhooking  = false;
static bool g_infunction = false;

int find_exec_binprm(void)
{
    if (original_exec_binprm == NULL)
    {
        original_exec_binprm = (exec_binprm_t)kallsyms_lookup_name(
          "exec_binprm");

        if (original_exec_binprm == NULL)
        {
            return 0;
        }
    }

    return 1;
}

static int new_exec_binprm(struct linux_binprm* bprm)
{
    mm_segment_t old_fs;
    int result;

    g_infunction = true;

    result = original_exec_binprm(bprm);

    // c_printk("exec_binprm: created task with pid %i\n", current->pid);

    // Get old address limit
    old_fs = get_fs();
    // Set new address limit (kernel space)
    set_fs(KERNEL_DS);

    communicate_with_task(current);
    communicate_check_tasks();

    // Set old address limit
    set_fs(old_fs);

    if (g_unhooking)
    {
        mutex_unlock(&g_mutex);
    }

    g_infunction = false;

    return result;
}

void hook_callsof_exec_binprm(void)
{
    int ret;
    int count_calls;
    int i;
    char temp[64];
#ifdef BIT_64
    uint16_t movabs_ax;
#else
    uint8_t movabs_ax;
#endif
    uintptr_t* list_calls;

    g_unhooking = false;

    init_buffer(&buffer_list_calls_exec_binprm);

    ret = find_exec_binprm();

    if (!ret)
    {
        c_printk("couldn't find exec_binprm\n");
        return;
    }

    ret = convert_to_hexstring((uint8_t*)(&original_exec_binprm),
                               sizeof(ptr_t),
                               temp,
                               sizeof(temp));

    if (!ret)
    {
        c_printk("couldn't convert address of exec_binprm to "
                 "hexstring\n");
        return;
    }

    ret = scan_kernel("_text",
                      "_end",
                      temp,
                      strlen(temp) - 1,
                      &buffer_list_calls_exec_binprm);

    if (!ret)
    {
        c_printk("didn't find any signatures of calls of exec_binprm\n");
        return;
    }

    count_calls = buffer_list_calls_exec_binprm.size / sizeof(ptr_t);

#ifdef BIT_64
    list_calls = (uintptr_t*)buffer_list_calls_exec_binprm.addr;

    for (i = 0; i < count_calls; i++)
    {
        movabs_ax = *(uint16_t*)(list_calls[i] - 2);

        if (movabs_ax == 0xb848)
        {
            c_printk("removing call for exec_binprm at 0x%p\n",
                     (ptr_t)list_calls[i]);
            *(ptr_t*)list_calls[i] = (ptr_t)new_exec_binprm;
        }
    }
#else
    for (i = 0; i < count_calls; i++)
    {
        movabs_ax = *(uint16_t*)(list_calls[i] - 1);

        if (movabs_ax == 0xb8)
        {
            c_printk("removing call for exec_binprm at 0x%p\n",
                     (ptr_t)list_calls[i]);
            *(ptr_t*)list_calls[i] = (ptr_t)new_exec_binprm;
        }
    }
#endif
}

void unhook_callsof_exec_binprm(void)
{
    int count_calls;
    int i;
#ifdef BIT_64
    uint16_t movabs_ax;
#else
    uint8_t movabs_ax;
#endif
    uintptr_t* list_calls;

    g_unhooking = true;

    if (g_infunction)
    {
        mutex_lock(&g_mutex);
    }

    if (buffer_list_calls_exec_binprm.addr == NULL)
    {
        c_printk("nothing to unhook for exec_binprm\n");
        goto out;
    }

    count_calls = buffer_list_calls_exec_binprm.size / sizeof(ptr_t);

#ifdef BIT_64
    list_calls = (uintptr_t*)buffer_list_calls_exec_binprm.addr;

    for (i = 0; i < count_calls; i++)
    {
        movabs_ax = *(uint16_t*)(list_calls[i] - 2);

        if (movabs_ax == 0xb848)
        {
            c_printk("removing call for exec_binprm at 0x%p\n",
                     (ptr_t)list_calls[i]);
            *(ptr_t*)list_calls[i] = original_exec_binprm;
        }
    }
#else
    for (i = 0; i < count_calls; i++)
    {
        movabs_ax = *(uint16_t*)(list_calls[i] - 1);

        if (movabs_ax == 0xb8)
        {
            c_printk("removing call for exec_binprm at 0x%p\n",
                     (ptr_t)list_calls[i]);
            *(ptr_t*)list_calls[i] = original_exec_binprm;
        }
    }
#endif
out:
    free_buffer(&buffer_list_calls_exec_binprm);
}
