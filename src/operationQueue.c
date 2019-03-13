/*!
 *  @file operationQueue.c
 *
 *  Created by @author George Boumis
 *  @date 2013/12/11.
 *	@version 1.1
 *  @copyright Copyright (c) 2013 George Boumis <developer.george.boumis@gmail.com>. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/queue.h>
#include <errno.h>

#include <memory_management/memory_management.h>
#include "operationQueue.h"

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)          \
    for ((var) = TAILQ_FIRST((head));               \
        (var) && ((tvar) = TAILQ_NEXT((var), field), 1);        \
        (var) = (tvar))
#endif

static void __initMainQueue(void) __attribute__((constructor));

void WDOperationDealloc(void *const block) __attribute__((visibility("internal")));
void WDOperationQueueDealloc(void *const queue) __attribute__((visibility("internal")));
void *WDOperationQueueThreadF(void *args) __attribute__((visibility("internal")));
WDOperation *WDOperationQueuePopOperation(WDOperationQueue *const queue) __attribute__((visibility("internal")));
void WDOperationQueuePopAndPerform(WDOperationQueue *const queue) __attribute__((visibility("internal")));
void WDOperationPerform(WDOperation *const block) __attribute__((visibility("internal")));

/*!
 *  @struct _wd_operation_t
 *  @brief The operation structure.
 *  @ingroup wd
 */
struct _wd_operation_t {
	wd_operation_f queuef; /*!< the operation's function */
	void *argument; /*!< the operation's argument */
	WDOperationQueue *queue; /*!< the associated queue that launched this operation */
	struct _wd_operation_guard_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} guard; /*!< the data used for thread safety */
	struct _wd_operation_wait_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} wait; /*!< the associated data for use with @ref WDOperationWaitUntilFinished */
	wd_operation_flags_t flags; /*!< the flags of the operations */
};

struct _list_item {
	WDOperation *operation;
	TAILQ_ENTRY(_list_item) items;
};

/*!
 *  @struct _wd_operation_queue_t
 *  @brief The operation queue structure.
 *  @ingroup wd
 */
struct _wd_operation_queue_t {
	TAILQ_HEAD(ListHead, _list_item) operations; /*!< the operation list */

	const char *name; /*!< the name of the operation queue */
	pthread_t thread; /*!< the operations queue's private thread */

	WDOperation *executingOperation;
	
	struct _wd_operation_queue_guard_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} guard; /*!< the data used for thread safety */

	struct _wd_operation_queue_suspend_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} suspend; /*!< the data associated to suspend operations */
	
	struct _wd_operation_queue_flags_t {
		bool stop:1; /*!< indicates whether the queue should stop and not to shcedule any further operations for execution */
		bool suspend:1; /*!< indicates whether the queue is suspended */
	} flags; /*!< the flags of the operations */
};

static struct _wd_operation_queue_t __mainQueue;


/* Operation Queue */

static void __initMainQueue() {
	__mainQueue = (struct _wd_operation_queue_t){
		{ (struct _list_item *)NULL , (struct _list_item **)NULL },		/* operations */
		"WDOperationQueue Main Queue",	/* name */
		pthread_self(),	/* thread */
        (WDOperation *)NULL,    /* executingOperation */
		{ PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER }, /* guard */
		{ PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER }, /* suspend */
		{ 0U, 0U }		/* flags */
	};
	TAILQ_INIT(&__mainQueue.operations);
}


WDOperationQueue *WDOperationQueueAllocate(void) {
	WDOperationQueue *const queue = MEMORY_MANAGEMENT_ALLOC(sizeof(WDOperationQueue));
    if (queue == NULL) {
        errno = ENOMEM;
        return (WDOperationQueue *)NULL;
    }
	
	TAILQ_INIT(&queue->operations);
	pthread_mutex_init(&queue->guard.mutex, (const pthread_mutexattr_t *)NULL);
	pthread_cond_init(&queue->guard.condition, (const pthread_condattr_t *)NULL);
	pthread_mutex_init(&queue->suspend.mutex, (const pthread_mutexattr_t *)NULL);
	pthread_cond_init(&queue->suspend.condition, (const pthread_condattr_t *)NULL);
	MEMORY_MANAGEMENT_ATTRIBUTE_SET_DEALLOC_FUNCTION(queue, WDOperationQueueDealloc);
	
	size_t size = (size_t)snprintf((char *)NULL,
                                   (size_t)0,
                                   "WDOperationQueue %p",
                                   (void *)queue);
	char *const name = calloc(size+1, sizeof(char));
    if (name == NULL) {
        errno = ENOMEM;
        return (WDOperationQueue *)NULL;
    }
	snprintf(name, size+1, "WDOperationQueue %p", (void *)queue);
	queue->name = (const char *)name;
	
    const int threadCreation = pthread_create(&queue->thread,
                                              (const pthread_attr_t *)NULL,
                                              WDOperationQueueThreadF,
                                              queue);
    if (threadCreation != 0) {
        release(queue);
        return (WDOperationQueue *)NULL;
    }
	return queue;
}

void WDOperationQueueDealloc(void *const _queue) {
    if (NULL == _queue) {
        errno = EINVAL;
        return;
    }
	WDOperationQueue *const queue = _queue;
	/* Indicate that the internal thread should stop */
	pthread_mutex_lock(&queue->guard.mutex);
    queue->flags.stop = true;
	pthread_mutex_unlock(&queue->guard.mutex);

	/* Remove all pending operations, they will never be executed */
	struct _list_item *item, *tmp;
	TAILQ_FOREACH_SAFE(item, &queue->operations, items, tmp) {
        if (NULL == item) { break; }
		TAILQ_REMOVE(&queue->operations, item, items);
		release(item->operation);
		release(item);
	}
	
    if (NULL != queue->name) {
		free((void *)queue->name);
    }
	
	/* 
	 If there is no operation running on the queue then cancel the thread
	 This will has as result to wake up the thread that probably is
	 blocked in a pthread_mutex_wait() call in WDOperationQueuePopAndPerform().
	 */
    if (queue->executingOperation == NULL) {
		pthread_cancel(queue->thread);
    }
	/* Otherwise cancel the running operation */
    else {
		WDOperationCancel(queue->executingOperation);
    }
	
	/* Wait the worker/internal thread to finish */
	pthread_join(queue->thread, NULL);
	
	/* Clean up */
	pthread_mutex_destroy(&queue->guard.mutex);
	pthread_cond_destroy(&queue->guard.condition);
}

WDOperationQueue *WDOperationQueueRetain(WDOperationQueue *const queue) {
    if (NULL == queue) {
        errno = EINVAL;
        return (WDOperationQueue *)NULL;
    }
	return retain(queue);
}

void WDOperationQueueRelease(WDOperationQueue *const queue) {
    if (NULL == queue) {
        errno = EINVAL;
        return;
    }
	release(queue);
}

void WDOperationQueueSetName(WDOperationQueue *const queue, const char *const name) {
    if (NULL == queue) {
        errno = EINVAL;
        return;
    }
    if (NULL != queue->name) {
        free((void *)queue->name);
        queue->name = (const char *)NULL;
    }
	queue->name = strdup(name);
}

const char *WDOperationQueueGetName(WDOperationQueue *const queue) {
    if (NULL == queue) {
        errno = EINVAL;
        return (const char *)NULL;
    }
	return queue->name;
}

void *WDOperationQueueThreadF(void *args) {
	WDOperationQueue *const queue = (WDOperationQueue *)args;
	
	while (!queue->flags.stop) {
		pthread_mutex_lock(&queue->suspend.mutex);
        if (queue->flags.suspend) {
			pthread_cond_wait(&queue->suspend.condition, &queue->suspend.mutex);
        }
		pthread_mutex_unlock(&queue->suspend.mutex);
		WDOperationQueuePopAndPerform(queue);
	}
	/* If any operaitons are still in the queue then WDOperationQueueDealloc() will take care of them */
	return (void *)NULL;
}

bool WDOperationQueueAddOperation(WDOperationQueue *const restrict queue, WDOperation *const restrict operation) {
    if (queue == NULL) {
        errno = EINVAL;
        return false;
    }
    if (operation == NULL) {
        errno = EINVAL;
        return false;
    }
    if (operation->queuef == NULL) {
        errno = EINVAL;
        return false;
    }
	
	pthread_mutex_lock(&(queue->guard.mutex));
	
	/* If the queue is stoped then it should not accept new operations */
    if (queue->flags.stop) {
        pthread_mutex_unlock(&(queue->guard.mutex));
        errno = EINVAL;
        return false;
    }
	
	/* If the queue is already on another queue */
	pthread_mutex_lock(&operation->guard.mutex);
    if (operation->queue) {
        pthread_mutex_unlock(&(queue->guard.mutex));
        pthread_mutex_unlock(&operation->guard.mutex);
        errno = EINVAL;
        return false;
    }
	pthread_mutex_unlock(&operation->guard.mutex);
	
	/* if it was already executed */
	pthread_mutex_unlock(&operation->wait.mutex);
    if (operation->flags.finished) {
        pthread_mutex_unlock(&(queue->guard.mutex));
        pthread_mutex_unlock(&operation->wait.mutex);
        errno = EINVAL;
        return false;
    }
	pthread_mutex_unlock(&operation->wait.mutex);
	
	int wasEmpty = TAILQ_EMPTY(&queue->operations);
	struct _list_item *item = MEMORY_MANAGEMENT_ALLOC(sizeof(struct _list_item));
    if (item == NULL) {
        pthread_mutex_unlock(&(queue->guard.mutex));
        errno = ENOMEM;
        return false;
    }
	
	/* Add the operation to the queue */
	item->operation = retain(operation);
	TAILQ_INSERT_TAIL(&(queue->operations), item, items);
	pthread_mutex_lock(&operation->guard.mutex);
	operation->queue = queue;
	pthread_mutex_unlock(&operation->guard.mutex);
	
	/* Inform that the queue is no more empty */
    if (wasEmpty) {
        pthread_cond_signal(&queue->guard.condition);
    }
	
	pthread_mutex_unlock(&(queue->guard.mutex));
	return true;
}

void WDOperationQueueSuspend(WDOperationQueue *const queue, const bool choice) {
    if (NULL == queue) {
        errno = EINVAL;
        return;
    }
	/* Cannot suspend the main queue */
    if (queue == &__mainQueue) {
        errno = EINVAL;
        return;
    }
	
	pthread_mutex_lock(&queue->suspend.mutex);
	if (choice != queue->flags.suspend) {
        if (choice == true) {
            queue->flags.suspend = true;
        }
		else {
			const unsigned int wasSuspened = queue->flags.suspend;
            queue->flags.suspend = false;
            if (wasSuspened) {
				pthread_cond_signal(&queue->suspend.condition);
            }
		}
	}
	
	pthread_mutex_unlock(&queue->suspend.mutex);
}

bool WDOperationQueueIsSuspended(WDOperationQueue *const queue) {
    if (NULL == queue) {
        errno = EINVAL;
        return false;
    }
	return (bool)queue->flags.suspend;
}

WDOperation *WDOperationQueuePopOperation(WDOperationQueue *const queue) {
    if (queue == NULL) {
        errno = EINVAL;
        return (WDOperation *)NULL;
    }
	
	pthread_mutex_lock(&(queue->guard.mutex));
    
    int isEmpty = TAILQ_EMPTY(&queue->operations);
    /* Block if there is no operation in the queue */
    if (isEmpty) {
        pthread_cond_wait(&queue->guard.condition, &queue->guard.mutex);
        isEmpty = TAILQ_EMPTY(&queue->operations);
        /* If the queue is still empty then the operation queue was stopped see WDOperationQueueDealloc() */
        if (isEmpty) {
            pthread_mutex_unlock(&(queue->guard.mutex));
            return (WDOperation *)NULL;
        }
    }
    /* If it was signaled and the queue is suspended it should*/
    pthread_mutex_lock(&queue->suspend.mutex);
    if (queue->flags.suspend) {
        pthread_mutex_unlock(&queue->suspend.mutex);
        pthread_mutex_unlock(&(queue->guard.mutex));
        return (WDOperation *)NULL;
    }
    pthread_mutex_unlock(&queue->suspend.mutex);
    
    /* Remove the operation from the internal list */
    struct _list_item *item = TAILQ_FIRST(&queue->operations);
    if (item == NULL) {
        pthread_mutex_unlock(&(queue->guard.mutex));
        return (WDOperation *)NULL;
    }
    WDOperation *const operation = (WDOperation *) item->operation;
	TAILQ_REMOVE(&(queue->operations), item, items);
	release(item);
	
	pthread_mutex_unlock(&(queue->guard.mutex));
	/* Return the operation */
	return operation;
}

void WDOperationQueuePopAndPerform(WDOperationQueue *const queue) {
	if (queue == NULL) {
        errno = EINVAL;
        return;
    }
	WDOperation *const operation = WDOperationQueuePopOperation(queue);
	WDOperationPerform(operation);
	release(operation);
}

void WDOperationQueueCancelAllOperations(WDOperationQueue *const queue) {
    if (NULL == queue) {
        errno = EINVAL;
        return;
    }
	
	pthread_mutex_lock(&queue->guard.mutex);
	
	struct _list_item *item;
	TAILQ_FOREACH(item, &queue->operations, items) {
		item->operation->flags.canceled = true;
	}
	
	pthread_mutex_unlock(&queue->guard.mutex);
}

void WDOperationQueueWaitAllOperations(WDOperationQueue *const queue) {
    if (NULL == queue) {
        errno = EINVAL;
        return;
    }
	while (!TAILQ_EMPTY(&queue->operations)) {
		struct _list_item *item = TAILQ_LAST(&queue->operations, ListHead);
		WDOperationWaitUntilFinished(item->operation);
	}
}


/**************/
/* Operations */
/**************/

WDOperation *WDOperationCreate(const wd_operation_f function, void *const argument) {
    if (function == NULL) {
        errno = EINVAL;
        return (WDOperation *)NULL;
    }
	
	WDOperation *const operation = MEMORY_MANAGEMENT_ALLOC(sizeof(WDOperation));
    if (operation == NULL) {
        errno = ENOMEM;
        return (WDOperation *)NULL;
    }
	
	operation->queuef = function;
	operation->argument = retain((void *)argument);
	pthread_mutex_init(&operation->wait.mutex, NULL);
	pthread_cond_init(&operation->wait.condition, NULL);
	pthread_mutex_init(&operation->guard.mutex, NULL);
	pthread_cond_init(&operation->guard.condition, NULL);
	
	MEMORY_MANAGEMENT_ATTRIBUTE_SET_DEALLOC_FUNCTION(operation, WDOperationDealloc);
	return operation;
}

void WDOperationDealloc(void *const _operation) {
    if (NULL == _operation) {
        errno = EINVAL;
        return;
    }
	WDOperation *const operation = _operation;
	pthread_mutex_destroy(&operation->wait.mutex);
	pthread_cond_destroy(&operation->wait.condition);
	pthread_mutex_destroy(&operation->guard.mutex);
	pthread_cond_destroy(&operation->guard.condition);
	
	release((void *)operation->argument);
}

WDOperation *WDOperationRetain(WDOperation *const operation) {
    if (NULL == operation) {
        errno = EINVAL;
        return (WDOperation *)NULL;
    }
	return retain(operation);
}

void WDOperationRelease(WDOperation *const operation) {
    if (NULL == operation) {
        errno = EINVAL;
        return;
    }
	release(operation);
}

void WDOperationPerform(WDOperation *const operation) {
    if (operation == NULL) {
        errno = EINVAL;
        return;
    }
    if (operation->queuef == NULL) {
        errno = EINVAL;
        return;
    }
	
	pthread_mutex_lock(&operation->guard.mutex);
	/* If the operation was not canceled */
	if (!operation->flags.canceled) {
		/* Indicate that it is executing */
        operation->flags.executing = true;
		/* Associate the operation to the operation queue's executing operation */
		operation->queue->executingOperation = retain(operation);
		/* Execute the operation with its argument */
		pthread_mutex_unlock(&operation->guard.mutex);
		operation->queuef(operation, (void *)operation->argument);
		pthread_mutex_lock(&operation->guard.mutex);
		/* Indicate that the operation is not executing any more */
        operation->flags.executing = false;
		/* Disassociate the operation from the queue */
		operation->queue->executingOperation = (WDOperation *)NULL;
		release(operation);
		operation->queue = (WDOperationQueue *)NULL;
	}
	pthread_mutex_unlock(&operation->guard.mutex);
	
	pthread_mutex_lock(&operation->wait.mutex);
	/* Mark the operation as finished */
    operation->flags.finished = true;
	/* Inform any one waiting in WDOperationWaitUntilFinished() call */
	pthread_cond_broadcast(&operation->wait.condition);
	pthread_mutex_unlock(&operation->wait.mutex);
}

WDOperationQueue *WDOperationCurrentOperationQueue(WDOperation *const operation) {
    if (operation == NULL) {
        errno = EINVAL;
        return (WDOperationQueue *)NULL;
    }
	pthread_mutex_lock(&operation->guard.mutex);
	WDOperationQueue *queue = operation->queue;
	pthread_mutex_unlock(&operation->guard.mutex);
	return queue;
}

void WDOperationCancel(WDOperation *const operation) {
	if (NULL == operation) {
        errno = EINVAL;
        return;
    }
	pthread_mutex_lock(&operation->guard.mutex);
    operation->flags.canceled = true;
	pthread_mutex_unlock(&operation->guard.mutex);
}

wd_operation_flags_t WDOperationGetFlags(WDOperation *const operation) {
	wd_operation_flags_t flags = { false, false, false };
    if (NULL == operation) {
        errno = EINVAL;
        return flags;
    }
	flags = operation->flags;
	return flags;
}

void WDOperationWaitUntilFinished(WDOperation *const operation) {
    if (NULL == operation) {
        errno = EINVAL;
        return;
    }
	pthread_mutex_lock(&operation->wait.mutex);
	/* Block if the operaiton is not finished */
    if (!operation->flags.finished) {
		pthread_cond_wait(&operation->wait.condition, &operation->wait.mutex);
    }
	pthread_mutex_unlock(&operation->wait.mutex);
}


WDOperationQueue *WDOperationQueueMainQueue() {
	return &__mainQueue;
}

bool WDOperationQueueMainQueueLoop() {
	WDOperationQueueThreadF(&__mainQueue);
	return true;
}

