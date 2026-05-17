#include <linux/mip_wrapper.h>
#include <linux/slab.h>

int mpi_add_group(pid_t pid, int group)
{
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return -ENOMEM;
    
    struct mpi_group_entry *entry;

    entry = kmalloc(sizeof(struct mpi_group_entry), GFP_KERNEL);

    if (!entry)
        return -ENOMEM;

    entry->group = group;

    list_add(&entry->list, &task->mpi_groups);

    return 0;
}

int mpi_remove_group(pid_t pid, int group){
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return NULL;
    
    struct list_head *pos;
    struct list_head *n;
    struct mpi_group_entry *entry;


    list_for_each_safe(pos, n, &task->mpi_groups) {
        entry = list_entry(pos, struct mpi_group_entry, list);

        if (entry->group == group) {
            list_del(pos);
            kfree(entry);
            return 0;
        }
    }
    return 0;
}

int mpi_is_in_group(pid_t pid, int group){
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return NULL;
    
    struct list_head *pos;
    struct list_head *n;
    struct mpi_group_entry *entry;


    list_for_each_safe(pos, n, &task->mpi_groups) {
        entry = list_entry(pos, struct mpi_group_entry, list);

        if (entry->group == group) {
            return 1;
        }
    }
    return 0;
}

void mpi_clear_groups(pid_t pid){
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return;
    
    //delete all messages before deleting all groups
    mpi_clear_messages_by_group(pid, -1);

    
    struct list_head *pos;
    struct list_head *n;
    struct mpi_group_entry *entry;


    list_for_each_safe(pos, n, &task->mpi_groups) {
        entry = list_entry(pos, struct mpi_group_entry, list);
        list_del(pos);
        kfree(entry);
    }
    
    INIT_LIST_HEAD(&task->mpi_groups);
    task->mpi_registered = 0;
    return;
}

int is_same_group(pid_t pid)
{
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return 0;
    
    struct list_head *pos_current;
    struct list_head *pos_other;
    struct mpi_group_entry *current_group;
    struct mpi_group_entry *other_group;


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

struct mpi_message* mpi_message_alloc(pid_t sender, pid_t reciver, int group, const char* data, ssize_t size){
    
        struct mpi_message *msg;

        if (!data || size < 1)
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

        memcpy(msg->data, data, size);
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

void mpi_clear_messages_by_group(pid_t pid, int group){
    struct task_struct *task = find_task_by_pid(pid);
    if (!task){
        return;
    }
    struct list_head *pos;
    struct list_head *n;
    struct mpi_message *msg;


    list_for_each_safe(pos, n, &task->mpi_queue) {
        msg = list_entry(pos, struct mpi_message, list);
        
        if (group == -1 || msg->group == group) {
            list_del(pos);
            mpi_message_free(msg);
        }
        
        if (group == -1){
            INIT_LIST_HEAD(&task->mpi_queue);
        }
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
            mpi_clear_groups(child->pid);
            child->mpi_registered = 0;
            return -ENOMEM;
        }

        child_entry->group = parent_entry->group;
        INIT_LIST_HEAD(&child_entry->list);
        list_add_tail(&child_entry->list, &child->mpi_groups);
    }

    return 0;
}
