//
//  operationQueue.c
//  workdipatcher
//
//  Created by George Boumis on 11/12/13.
//  Copyright (c) 2013 George Boumis <developer.george.boumis@gmail.com>. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/queue.h>
#include <errno.h>

#include <memory_management/memory_management.h>
#include "operationQueue.h"

void WDOperationDealloc(void *block);
void WDOperationQueueDealloc(void *queue);
void *WDOperationQueueThreadF(void *args);
WDOperation *WDOperationQueuePopOperation(WDOperationQueue *restrict queue);
void WDOperationQueuePopAndPerform(WDOperationQueue *restrict queue);

void WDOperationPerform(WDOperation *restrict block);

struct _wd_operation_t {
	wd_operation_f queuef;
	void *argument;
	WDOperationQueue *queue;
	struct _wd_operation_wait_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} wait;
	wd_operation_flags_t flags;
};

struct _list_item {
	WDOperation *operation;
	TAILQ_ENTRY(_list_item) items;
};

struct _wd_operation_queue_t {
	TAILQ_HEAD(ListHead, _list_item) operations;
	
	const char *name;
	
	pthread_t thread;
	struct _wd_operation_queue_guard_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} guard;

	struct _wd_operation_queue_suspend_t {
		pthread_mutex_t mutex;
		pthread_cond_t condition;
	} suspend;
	
	struct _wd_operation_queue_flags_t {
		unsigned int stop:1;
		unsigned int suspend:1;
	} flags;
	
};



/* Operation Queue */

WDOperationQueue *WDOperationQueueAllocate(void) {
	WDOperationQueue *queue = MEMORY_MANAGEMENT_ALLOC(sizeof(WDOperationQueue));
	if ( queue == NULL ) return errno = ENOMEM, (WDOperationQueue *)NULL;
	
	TAILQ_INIT(&queue->operations);
	pthread_mutex_init(&queue->guard.mutex, NULL);
	pthread_cond_init(&queue->guard.condition, NULL);
	pthread_mutex_init(&queue->suspend.mutex, NULL);
	pthread_cond_init(&queue->suspend.condition, NULL);
	MEMORY_MANAGEMENT_ATTRIBUTE_SET_DEALLOC_FUNCTION(queue, WDOperationQueueDealloc);
	
	size_t size = (size_t)snprintf(NULL, 0, "WDOperationQueue %p", queue);
	char *name = calloc(size+1, sizeof(char));
	snprintf(name, size+1, "WDOperationQueue %p", queue);
	queue->name = name;
	
	pthread_create(&queue->thread, NULL, WDOperationQueueThreadF, queue);
	
	return queue;
}

void WDOperationQueueDealloc(void *_queue) {
	if (NULL == _queue) return;
	WDOperationQueue *queue = _queue;
	queue->flags.stop = 1;

	struct _list_item *item, *tmp;
	TAILQ_FOREACH_SAFE(item, &queue->operations, items, tmp) {
		TAILQ_REMOVE(&queue->operations, item, items);
		release(item->operation);
		release(item);
	}
	
	if (NULL != queue->name)
		free((void *)queue->name);
	
	pthread_mutex_destroy(&queue->guard.mutex);
	pthread_cond_destroy(&queue->guard.condition);
}

void WDOperationQueueSetName(WDOperationQueue *queue, const char *name) {
	if (NULL == queue) return;
	if (NULL != queue->name)
		free((void *)queue->name), queue->name = NULL;
	queue->name = strdup(name);
}

const char * WDOperationQueueGetName(WDOperationQueue *queue) {
	if (NULL == queue) return errno = EINVAL, NULL;
	return queue->name;
}

void *WDOperationQueueThreadF(void *args) {
	WDOperationQueue *queue = (WDOperationQueue *)args;
	
	while (!queue->flags.stop) {
		pthread_mutex_lock(&queue->suspend.mutex);
		if (queue->flags.suspend)
			pthread_cond_wait(&queue->suspend.condition, &queue->suspend.mutex);
		pthread_mutex_unlock(&queue->suspend.mutex);
		WDOperationQueuePopAndPerform(queue);
	}
	
	return (void *)NULL;
}

int WDOperationQueueAddOperation(WDOperationQueue *restrict queue, WDOperation *restrict operation) {
	if ( queue == NULL ) return errno = EINVAL, -1;
	if ( operation == NULL ) return errno = EINVAL, -1;
	if ( operation->queuef == NULL ) return errno = EINVAL, -1;
	
	pthread_mutex_lock(&(queue->guard.mutex));
	
	int wasEmpty = TAILQ_EMPTY(&queue->operations);
	struct _list_item *item = MEMORY_MANAGEMENT_ALLOC(sizeof(struct _list_item));
	if ( item == NULL ) return pthread_mutex_unlock(&(queue->guard.mutex)), errno = ENOMEM, -1;
	
	item->operation = retain(operation);
	TAILQ_INSERT_TAIL(&(queue->operations), item, items);
	operation->queue = queue;
	
	if (wasEmpty) pthread_cond_signal(&queue->guard.condition);
	
	pthread_mutex_unlock(&(queue->guard.mutex));
	return 0;
}

void WDOperationQueueSuspend(WDOperationQueue *restrict queue, int choice) {
	if (NULL == queue) return;
	if (choice < 0) return;
	
	pthread_mutex_lock(&queue->suspend.mutex);
	if (choice != queue->flags.suspend) {
		if (choice > 0)
			queue->flags.suspend = 1;
		else {
			queue->flags.suspend = 0;
			pthread_cond_signal(&queue->suspend.condition);
		}
	}
	
	pthread_mutex_unlock(&queue->suspend.mutex);
}

int WDOperationQueueIsSuspended(WDOperationQueue *restrict queue) {
	if (NULL == queue) return errno = EINVAL, -1;
	return queue->flags.suspend;
}

WDOperation *WDOperationQueuePopOperation(WDOperationQueue *restrict queue) {
	if (queue == NULL) return errno = EINVAL, NULL;
	
	pthread_mutex_lock(&(queue->guard.mutex));
	WDOperation *operation = NULL;
	
	int isEmpty = TAILQ_EMPTY(&queue->operations);
	if (isEmpty)
		pthread_cond_wait(&queue->guard.condition, &queue->guard.mutex);
	
	struct _list_item *item = TAILQ_FIRST(&queue->operations);
	if ( item == NULL ) return pthread_mutex_unlock(&(queue->guard.mutex)), (WDOperation *)NULL;
	operation = (WDOperation *) item->operation;
	TAILQ_REMOVE(&(queue->operations), item, items);
	release(item);
	
	pthread_mutex_unlock(&(queue->guard.mutex));
	return operation;
}

void WDOperationQueuePopAndPerform(WDOperationQueue *restrict queue) {
	if (queue == NULL) { errno = EINVAL; return; }
	WDOperation *operation = WDOperationQueuePopOperation(queue);
	WDOperationPerform(operation);
	operation->queue = NULL;
	release(operation);
}

void WDOperationQueueCancelAllOperations(WDOperationQueue *queue) {
	if (NULL == queue) return;
	
	pthread_mutex_lock(&queue->guard.mutex);
	
	struct _list_item *item;
	TAILQ_FOREACH(item, &queue->operations, items) {
		item->operation->flags.canceled = 1;
	}
	
	pthread_mutex_unlock(&queue->guard.mutex);
}

void WDOperationQueueWaitAllOperations(WDOperationQueue *queue) {
	if (NULL == queue) return;
	while (!TAILQ_EMPTY(&queue->operations)) {
		struct _list_item *item = TAILQ_LAST(&queue->operations, ListHead);
		WDOperationWaitUntilFinished(item->operation);
	}
}


/* Operations */

WDOperation *WDOperationAllocate(const wd_operation_f function, void *restrict argument) {
	if ( function == NULL ) return errno = EINVAL, (WDOperation *)NULL;
	
	WDOperation *operation = MEMORY_MANAGEMENT_ALLOC(sizeof(WDOperation));
	if ( operation == NULL ) return errno = ENOMEM, (WDOperation *)NULL;
	
	operation->queuef = function;
	operation->argument = retain((void *)argument);
	pthread_mutex_init(&operation->wait.mutex, NULL);
	pthread_cond_init(&operation->wait.condition, NULL);
	
	MEMORY_MANAGEMENT_ATTRIBUTE_SET_DEALLOC_FUNCTION(operation, WDOperationDealloc);
	return operation;
}

void WDOperationDealloc(void *_operation) {
	if (NULL == _operation) return;
	WDOperation *operation = _operation;
	pthread_mutex_destroy(&operation->wait.mutex);
	pthread_cond_destroy(&operation->wait.condition);
	release((void *)operation->argument);
}

void WDOperationPerform(WDOperation *restrict operation) {
	if ( operation == NULL ) return;
	if ( operation->queuef == NULL ) return;
	
	if (!operation->flags.canceled) {
		operation->flags.executing = 1;
		operation->queuef(operation, (void *)operation->argument);
		operation->flags.executing = 0;
	}
	
	pthread_mutex_lock(&operation->wait.mutex);
	operation->flags.finished = 1;
	pthread_cond_broadcast(&operation->wait.condition);
	pthread_mutex_unlock(&operation->wait.mutex);
}

WDOperationQueue *WDOperationCurrentOperationQueue(WDOperation *operation) {
	if (operation == NULL) return errno = EINVAL, NULL;
	return operation->queue;
}

void WDOperationCancel(WDOperation *operation) {
	if (NULL == operation) { errno = EINVAL; return; }
	operation->flags.canceled = 1;
}

wd_operation_flags_t WDOperationGetFlags(WDOperation *operation) {
	wd_operation_flags_t flags = { 0, 0, 0 };
	if (NULL == operation) return errno = EINVAL, flags;
	flags = operation->flags;
	return flags;
}

void WDOperationWaitUntilFinished(WDOperation *operation) {
	if (NULL == operation) return;
	pthread_mutex_lock(&operation->wait.mutex);
	if (!operation->flags.finished)
		pthread_cond_wait(&operation->wait.condition, &operation->wait.mutex);
	pthread_mutex_unlock(&operation->wait.mutex);
}


