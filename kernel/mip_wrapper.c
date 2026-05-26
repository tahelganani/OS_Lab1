#include <linux/mip_wrapper.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <../include/asm-i386/uaccess.h>

int mpi_add_group(pid_t pid, int group)
{
    struct task_struct *task = find_task_by_pid(pid);
    struct mpi_group_entry *entry;
    if (!task)
        return -ENOMEM;

    if(mpi_is_in_group(pid, group)) return 0;

    entry = kmalloc(sizeof(struct mpi_group_entry), GFP_KERNEL);

    if (!entry)
        return -ENOMEM;

    entry->group = group;

    list_add(&entry->list, &task->mpi_groups);
    task->mpi_registered = 1;

    return 0;
}

int mpi_remove_group(pid_t pid, int group){
    struct task_struct *task = find_task_by_pid(pid);
    struct list_head *pos;
    struct list_head *n;
    struct mpi_group_entry *entry;

    if (!task)
        return 0;

    list_for_each_safe(pos, n, &task->mpi_groups) {
        entry = list_entry(pos, struct mpi_group_entry, list);

        if (entry->group == group) {
            list_del(pos);
            kfree(entry);
            if(list_empty(&task->mpi_groups)) task->mpi_registered = 0;
            return 0;
        }
    }
    return 0;
}

int mpi_is_in_group(pid_t pid, int group){
    struct task_struct *task = find_task_by_pid(pid);
    struct list_head *pos;
    struct list_head *n;
    struct mpi_group_entry *entry;

    if (!task)
        return 0;

    list_for_each_safe(pos, n, &task->mpi_groups) {
        entry = list_entry(pos, struct mpi_group_entry, list);

        if (entry->group == group) {
            return 1;
        }
    }
    return 0;
}
void mpi_clear_groups_task(struct task_struct* task) {
    struct list_head *pos;
    struct list_head *n;
    struct mpi_group_entry *entry;

    if (!task) return;

    list_for_each_safe(pos, n, &task->mpi_groups) {
        entry = list_entry(pos, struct mpi_group_entry, list);
        list_del(pos);
        kfree(entry);
    }
    INIT_LIST_HEAD(&task->mpi_groups);
    task->mpi_registered = 0;
}

void mpi_clear_groups(pid_t pid) {
    struct task_struct *task = find_task_by_pid(pid);
    if (!task) return;
    
    //delete all messages before deleting all groups
    mpi_clear_messages_by_group(pid, -1);
    mpi_clear_groups_task(task);
}

int is_same_group(pid_t pid)
{
    struct task_struct *task = find_task_by_pid(pid);
    struct list_head *pos_current;
    struct list_head *pos_other;
    struct mpi_group_entry *current_group;
    struct mpi_group_entry *other_group;

    if (!task)
        return 0;

    if (!current->mpi_registered || !task->mpi_registered)
        return 0;

    list_for_each(pos_current, &current->mpi_groups) {
        current_group = list_entry(pos_current, struct mpi_group_entry, list);

        list_for_each(pos_other, &task->mpi_groups) {
            other_group = list_entry(pos_other, struct mpi_group_entry, list);

            if (current_group->group == other_group->group)
                return current_group->group;
        }
    }

    return 0;
}

int is_registered(pid_t pid){
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return 0;

    return task->mpi_registered ? 1 : 0;
    
}

struct mpi_message* mpi_message_alloc(pid_t sender, pid_t reciver, int group, ssize_t size){
    
        struct mpi_message *msg;

        if (size < 1)
            return NULL;

        msg = kmalloc(sizeof(struct mpi_message), GFP_KERNEL);
        if (!msg)
            return NULL;

        msg->data = kmalloc(size, GFP_KERNEL);
        if (!msg->data) {
            kfree(msg);
            return NULL;
        }

        msg->sender_pid = sender;
        msg->reciever_pid = reciver;
        msg->group = group;
        msg->size = size;

        INIT_LIST_HEAD(&msg->list);

        return msg;
    
}

void mpi_message_free(struct mpi_message *msg)
{
    if (!msg)
        return;

    if (msg->data)
        kfree(msg->data);

    kfree(msg);
}

void mpi_clear_messages_by_group(pid_t pid, int group) {
    struct task_struct *task = find_task_by_pid(pid);
    struct list_head *pos;
    struct list_head *n;
    struct mpi_message *msg;

    if (!task){
        return;
    }

    list_for_each_safe(pos, n, &task->mpi_queue) {
        msg = list_entry(pos, struct mpi_message, list);
        
        if (group == -1 || msg->group == group) {
            list_del(pos);
            mpi_message_free(msg);
        }
    }
    if (group == -1) {
        INIT_LIST_HEAD(&task->mpi_queue);
    }
}

int mpi_copy_groups(struct task_struct *child, struct task_struct *parent)
{
    struct list_head *pos;
    struct mpi_group_entry *parent_entry;
    struct mpi_group_entry *child_entry;

    if (!child || !parent)
        return -EINVAL;

    INIT_LIST_HEAD(&child->mpi_queue);
    INIT_LIST_HEAD(&child->mpi_groups);

    child->mpi_registered = parent->mpi_registered;

    if (!parent->mpi_registered)
        return 0;

    list_for_each(pos, &parent->mpi_groups) {
        parent_entry = list_entry(pos, struct mpi_group_entry, list);

        child_entry = kmalloc(sizeof(struct mpi_group_entry), GFP_KERNEL);
        if (!child_entry) {
            mpi_clear_groups_task(child);
            child->mpi_registered = 0;
            return -ENOMEM;
        }

        child_entry->group = parent_entry->group;
        INIT_LIST_HEAD(&child_entry->list);
        list_add_tail(&child_entry->list, &child->mpi_groups);
    }

    return 0;
}

asmlinkage int sys_mpi_register(int mpi_gid) {
    return mpi_add_group(current->pid, mpi_gid);
}

asmlinkage int sys_mpi_send(pid_t pid, char *message, ssize_t message_size) {
    struct mpi_message* msg;
    struct task_struct* receiver;
    int group;

    if(!message || message_size < 1) {return -EINVAL;}
    receiver = find_task_by_pid(pid);
    if(!receiver) {return -ESRCH;}
    group = is_same_group(pid);
    if(!group) {return -EPERM;}
    msg = mpi_message_alloc(current->pid, pid, group, message_size);
    if(!msg) {return -ENOMEM;}
    if(copy_from_user(msg->data, message, message_size)) {
        mpi_message_free(msg);
        return -EFAULT;
    }
    list_add_tail(&msg->list, &receiver->mpi_queue);
    return 0;
}

asmlinkage int sys_mpi_receive(pid_t pid, char *message, ssize_t message_size) {

}

asmlinkage int sys_mpi_unregister(int mpi_gid) {
    
}



