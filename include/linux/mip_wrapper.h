#ifndef _MIP_WRAPPER_H
#define _MIP_WRAPPER_H

#include <linux/sched.h>
#include <linux/list.h>


struct mpi_message{
    pid_t sender_pid;
    pid_t reciever_pid;
    int group;
    ssize_t size;
    char *data;
    struct list_head  list;
};


struct mpi_group_entry {
    int group;
    struct list_head list;
};

/*
 ** mpi_add_group
 ** add the given group to the task's group list
 ** if already exists - do nothing
 ** return 0 on success, -ENOMEM on allocation failure
 */
int mpi_add_group(pid_t pid, int group);


/*
 ** mpi_remove_group
 ** remove the given group from the task's group list
 ** if group does not exist - do nothing
 ** free associated memory
 ** return 0
 */
int mpi_remove_group(pid_t pid, int group);

/*
 ** mpi_is_in_group
 ** check if the task is part of the given group
 ** return 1 if yes, 0 otherwise
 */
int mpi_is_in_group(pid_t pid, int group);

/*
 ** mpi_clear_groups
 ** remove all groups from the task
 ** free all associated memory
 ** used on unregister(-1) or task termination
 */
void mpi_clear_groups(pid_t pid);


/*
** is_same_group
** recieve pid
** check if current->task_struck->groups has a common group with pid->task_struct->groups
** retunr 0 if not; else return common group number
*/
int is_same_group(pid_t pid);


/*
 ** is_registered
 ** return 0 if not: else 1
 */
int is_registered(pid_t pid);

/*
 ** mpi_message_alloc
 ** allocating memory for message
 ** return a pointer to the message
 ** if allocation failed return NULL
 */
struct mpi_message* mpi_message_alloc(pid_t sender, pid_t reciver, int group, const char* data, ssize_t size);

// release memory, call it only after deletion from queue
void mpi_message_free(struct mpi_message *msg);

/*
 ** mpi_clear_messages_by_group
 ** delete each message in queue by group
 ** if group = -1, delete all
 ** used for termination and fork
 */
void mpi_clear_messages_by_group(pid_t pid, int group);


/*
 **
 ** copy all goups from parent proccess to child
 */
int mpi_copy_groups(struct task_struct *child, struct task_struct *parent);

#endif // !_MIP_WRAPPER_H
