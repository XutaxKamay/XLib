#include "main.h"

communicate_error_t
communicate_read__read_struct(task_t* task,
                              uintptr_t address,
                              communicate_read_t* communicate_read)
{
    if (c_copy_from_user(task,
                         communicate_read,
                         (ptr_t)address,
                         sizeof(communicate_read_t)))
    {
        c_printk_error("couldn't read communicate read struct from task "
                       "%i\n",
                       task->pid);

        return COMMUNICATE_ERROR_STRUCT_COPY_FROM;
    }

    return COMMUNICATE_ERROR_NONE;
}

communicate_error_t
communicate_read__write_struct(task_t* task,
                               uintptr_t address,
                               communicate_write_t* communicate_write)
{
    if (c_copy_from_user(task,
                         communicate_write,
                         (ptr_t)address,
                         sizeof(communicate_write_t)))
    {
        c_printk_error("couldn't read communicate write struct from task "
                       "%i\n",
                       task->pid);

        return COMMUNICATE_ERROR_STRUCT_COPY_FROM;
    }

    return COMMUNICATE_ERROR_NONE;
}

communicate_error_t communicate_read__remote_mmap_struct(
  task_t* task,
  uintptr_t address,
  communicate_remote_mmap_t* communicate_remote_mmap)
{
    if (c_copy_from_user(task,
                         communicate_remote_mmap,
                         (ptr_t)address,
                         sizeof(communicate_remote_mmap_t)))
    {
        c_printk_error("couldn't read communicate remote mmap struct "
                       "from task "
                       "%i\n",
                       task->pid);

        return COMMUNICATE_ERROR_STRUCT_COPY_FROM;
    }

    return COMMUNICATE_ERROR_NONE;
}

communicate_error_t communicate_read__remote_munmap_struct(
  task_t* task,
  uintptr_t address,
  communicate_remote_munmap_t* communicate_remote_munmap)
{
    if (c_copy_from_user(task,
                         communicate_remote_munmap,
                         (ptr_t)address,
                         sizeof(communicate_remote_munmap_t)))
    {
        c_printk_error("couldn't read communicate remote munmap struct "
                       "from task "
                       "%i\n",
                       task->pid);

        return COMMUNICATE_ERROR_STRUCT_COPY_FROM;
    }

    return COMMUNICATE_ERROR_NONE;
}

communicate_error_t communicate_read__remote_clone_struct(
  task_t* task,
  uintptr_t address,
  communicate_remote_clone_t* communicate_remote_clone)
{
    if (c_copy_from_user(task,
                         communicate_remote_clone,
                         (ptr_t)address,
                         sizeof(communicate_remote_clone_t)))
    {
        c_printk_error("couldn't read communicate remote clone struct "
                       "from task "
                       "%i\n",
                       task->pid);

        return COMMUNICATE_ERROR_STRUCT_COPY_FROM;
    }

    return COMMUNICATE_ERROR_NONE;
}

communicate_error_t communicate_process_cmd_read(uintptr_t address)
{
    communicate_error_t error;
    communicate_read_t communicate_read;
    buffer_t temp_buffer;
    task_t* remote_task;

    init_buffer(&temp_buffer);

    error = communicate_read__read_struct(current,
                                          address,
                                          &communicate_read);

    if (error != COMMUNICATE_ERROR_NONE)
    {
        goto out;
    }

    if (communicate_read.vm_size > COMMUNICATE_MAX_BUFFER)
    {
        error = COMMUNICATE_ERROR_BUFFER_TOO_LARGE;
        goto out;
    }

    alloc_buffer(communicate_read.vm_size, &temp_buffer);

    remote_task = find_task_from_pid(communicate_read.pid_target);

    if (remote_task == NULL)
    {
        error = COMMUNICATE_ERROR_TARGET_PID_NOT_FOUND;
        goto out;
    }

    if (c_copy_from_user(remote_task,
                         temp_buffer.addr,
                         (ptr_t)communicate_read.vm_remote_address,
                         temp_buffer.size))
    {
        error = COMMUNICATE_ERROR_COPY_FROM;
    }

    put_task_struct(remote_task);

    if (c_copy_to_user(current,
                       (ptr_t)communicate_read.vm_local_address,
                       (ptr_t)temp_buffer.addr,
                       temp_buffer.size))
    {
        error = COMMUNICATE_ERROR_COPY_TO;
    }

out:
    free_buffer(&temp_buffer);
    return error;
}

communicate_error_t communicate_process_cmd_write(uintptr_t address)
{
    communicate_error_t error;
    communicate_write_t communicate_write;
    buffer_t temp_buffer;
    task_t* remote_task;

    init_buffer(&temp_buffer);

    error = communicate_read__write_struct(current,
                                           address,
                                           &communicate_write);

    if (error != COMMUNICATE_ERROR_NONE)
    {
        goto out;
    }

    if (communicate_write.vm_size > COMMUNICATE_MAX_BUFFER)
    {
        error = COMMUNICATE_ERROR_BUFFER_TOO_LARGE;
        goto out;
    }

    alloc_buffer(communicate_write.vm_size, &temp_buffer);

    if (c_copy_from_user(current,
                         temp_buffer.addr,
                         (ptr_t)communicate_write.vm_local_address,
                         temp_buffer.size))
    {
        error = COMMUNICATE_ERROR_COPY_FROM;
    }

    remote_task = find_task_from_pid(communicate_write.pid_target);

    if (remote_task == NULL)
    {
        error = COMMUNICATE_ERROR_TARGET_PID_NOT_FOUND;
        goto out;
    }

    if (c_copy_to_user(remote_task,
                       (ptr_t)communicate_write.vm_remote_address,
                       (ptr_t)temp_buffer.addr,
                       temp_buffer.size))
    {
        error = COMMUNICATE_ERROR_COPY_TO;
    }

    put_task_struct(remote_task);

out:
    free_buffer(&temp_buffer);
    return error;
}

communicate_error_t communicate_process_cmd_remote_mmap(uintptr_t address)
{
    mm_segment_t old_fs;
    communicate_error_t error;
    communicate_remote_mmap_t communicate_remote_mmap;
    task_t *old_current, *remote_task;
    mm_t* backup_mm;
    static DEFINE_SPINLOCK(spinlock);

    error = communicate_read__remote_mmap_struct(current,
                                                 address,
                                                 &communicate_remote_mmap);

    if (error != COMMUNICATE_ERROR_NONE)
    {
        goto out;
    }

    if (offset_in_page(communicate_remote_mmap.offset) != 0)
    {
        error = COMMUNICATE_ERROR_MMAP_PGOFF_FAILED;
        goto out;
    }

    remote_task = find_task_from_pid(communicate_remote_mmap.pid_target);

    if (remote_task == NULL)
    {
        error = COMMUNICATE_ERROR_TARGET_PID_NOT_FOUND;
        goto out;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    // We can trick the kernel by saying the current task is the
    // targeted one for a moment.
    old_current = get_current();

    spin_lock(&spinlock);
    read_lock(ptasklist_lock);

    switch_to_task(remote_task);

    // This is needed for kernel threads.
    backup_mm = get_task_mm(remote_task);

    /**
     * We must do this in case mm is null
     */
    if (backup_mm == NULL && remote_task->active_mm)
        remote_task->mm = remote_task->active_mm;

    if (remote_task->mm)
    {
        /**
         * TODO: we might use our own function for more stability
         */
        communicate_remote_mmap.ret
          = ksys_mmap_pgoff(communicate_remote_mmap.vm_remote_address,
                            communicate_remote_mmap.vm_size,
                            communicate_remote_mmap.prot,
                            communicate_remote_mmap.flags,
                            communicate_remote_mmap.fd,
                            communicate_remote_mmap.offset >> PAGE_SHIFT);
    }

    remote_task->mm = backup_mm;

    if (backup_mm)
    {
        mmput(backup_mm);
    }

    switch_to_task(old_current);

    read_unlock(ptasklist_lock);
    spin_unlock(&spinlock);

    set_fs(old_fs);

    put_task_struct(remote_task);

    if (c_copy_to_user(current,
                       (ptr_t)address,
                       (ptr_t)&communicate_remote_mmap,
                       sizeof(communicate_remote_mmap_t)))
    {
        c_printk_error("couldn't write communicate remote mmap "
                       "struct "
                       "from task "
                       "%i\n",
                       current->pid);

        error = COMMUNICATE_ERROR_COPY_TO;
    }

out:
    return error;
}

communicate_error_t
communicate_process_cmd_remote_munmap(uintptr_t address)
{
    communicate_error_t error;
    communicate_remote_munmap_t communicate_remote_munmap;
    task_t* remote_task;
    mm_segment_t old_fs;

    error = communicate_read__remote_munmap_struct(
      current,
      address,
      &communicate_remote_munmap);

    if (error != COMMUNICATE_ERROR_NONE)
    {
        goto out;
    }

    remote_task = find_task_from_pid(communicate_remote_munmap.pid_target);

    if (remote_task == NULL)
    {
        error = COMMUNICATE_ERROR_TARGET_PID_NOT_FOUND;
        goto out;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    communicate_remote_munmap.ret
      = c___vm_munmap(remote_task,
                      communicate_remote_munmap.vm_remote_address,
                      communicate_remote_munmap.vm_size,
                      true);

    set_fs(old_fs);

    put_task_struct(remote_task);

    if (communicate_remote_munmap.ret < 0)
    {
        error = COMMUNICATE_ERROR_VM_MUNMAP_FAILED;
    }

    if (c_copy_to_user(current,
                       (ptr_t)address,
                       (ptr_t)&communicate_remote_munmap,
                       sizeof(communicate_remote_munmap_t)))
    {
        c_printk_error("couldn't write communicate remote munmap "
                       "struct "
                       "from task "
                       "%i\n",
                       current->pid);

        error = COMMUNICATE_ERROR_COPY_TO;
    }

out:
    return error;
}

communicate_error_t communicate_process_cmd_remote_clone(uintptr_t address)
{
    mm_segment_t old_fs;
    communicate_error_t error;
    communicate_remote_clone_t communicate_remote_clone;
    task_t* remote_task;
    struct kernel_clone_args clone_args;

    error
      = communicate_read__remote_clone_struct(current,
                                              address,
                                              &communicate_remote_clone);

    if (error != COMMUNICATE_ERROR_NONE)
    {
        goto out;
    }

    memcpy(&clone_args,
           &communicate_remote_clone,
           sizeof(struct kernel_clone_args));

    remote_task = find_task_from_pid(communicate_remote_clone.pid_target);

    if (remote_task == NULL)
    {
        error = COMMUNICATE_ERROR_TARGET_PID_NOT_FOUND;
        goto out;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    communicate_remote_clone.ret
      = c_do_fork(remote_task,
                  &clone_args,
                  (struct pt_regs*)&communicate_remote_clone.regs,
                  &communicate_remote_clone.regs_set);

    if (communicate_remote_clone.ret < 0)
    {
        error = COMMUNICATE_ERROR_CLONE_FAILED;
    }

    set_fs(old_fs);

    put_task_struct(remote_task);

    if (c_copy_to_user(current,
                       (ptr_t)address,
                       (ptr_t)&communicate_remote_clone,
                       sizeof(communicate_remote_clone_t)))
    {
        c_printk_error("couldn't write communicate remote clone "
                       "struct "
                       "from task "
                       "%i\n",
                       current->pid);

        error = COMMUNICATE_ERROR_COPY_TO;
    }

out:
    return error;
}
