/*!
 *  @file helper.c
 *  @brief ui
 *
 *  Created by @author George Boumis
 *  @date 13/4/13.
 *  @copyright   Copyright (c) 2013 George Boumis. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <errno.h>

#include <helper.h>
//#include <cobj.h>
#include <memory_management/memory_management.h>

void UIDeallocDispatchBlock(void *block);
void UIDeallocDispatchWorker(void *worker);

struct _dispatchBlock {
	MEMORY_MANAGEMENT_ENABLE();
	voidf workerf;
	const void *argument;
};

struct _DisptachItem {
	MEMORY_MANAGEMENT_ENABLE();
	const UIDispatchBlock *item;
	TAILQ_ENTRY(_DisptachItem) items;
};

struct _dispatchWorker {
	MEMORY_MANAGEMENT_ENABLE();
	TAILQ_HEAD(ListHead, _DisptachItem) listOfDispatches;
	pthread_mutex_t _dispatcherGuardian;
};

//#undef COBJ
#ifdef COBJ
static MutableArrayRef _listOfDisptaches = NULL;
#else

#endif

UIDispatchWorker *UIAllocateDispatchWorker(void) {
	UIDispatchWorker *worker = calloc(1, sizeof(UIDispatchWorker));
	if ( worker == NULL ) return errno = ENOMEM, (UIDispatchWorker *)NULL;
	MEMORY_MANAGEMENT_INITIALIZE(worker);
	memory_management_attributes_set_dealloc_function(worker, UIDeallocDispatchWorker);
	TAILQ_INIT(&(worker->listOfDispatches));
//	worker->_dispatcherGuardian = PTHREAD_MUTEX_INITIALIZER;
	return worker;
}

UIDispatchBlock *UIAllocateDispatchBlock(const voidf function, const void *restrict argument) {
	if ( function == NULL ) return errno = EINVAL, (UIDispatchBlock *)NULL;
	
	UIDispatchBlock *block = calloc(1, sizeof(UIDispatchBlock));
	if ( block == NULL ) return errno = ENOMEM, (UIDispatchBlock *)NULL;
	
	MEMORY_MANAGEMENT_INITIALIZE(block);
	memory_management_attributes_set_dealloc_function(block, UIDeallocDispatchBlock);
	
	block->workerf = function;
	block->argument = argument;
	
	return block;
}

void UIDeallocDispatchBlock(void *block) {
}

void UIDeallocDispatchWorker(void *worker) {
}



void UIPerformDispatchBlock(UIDispatchWorker *restrict worker, UIDispatchBlock *restrict block) {
	if ( block == NULL ) return;
	if ( block->workerf != NULL )
		block->workerf((void *)block->argument), release(block);
}


int UIAddDispatchBlock(UIDispatchWorker *restrict worker, UIDispatchBlock *restrict block) {
	if ( block == NULL ) return -1;
	
	pthread_mutex_lock(&(worker->_dispatcherGuardian));
	
#ifdef COBJ
	if ( _listOfDisptaches == NULL ) {
		_listOfDisptaches = new(MutableArray, NULL);
		if ( _listOfDisptaches == NULL ) return pthread_mutex_unlock(&_dispatcherGuardian), -1;
	}
	
	ValueRef blockValue = new(Value, block, NULL);
	if ( blockValue == NULL ) return pthread_mutex_unlock(&_dispatcherGuardian), -1;
	insertObject(_listOfDisptaches, blockValue);
	release(blockValue);
#else
	
	struct _DisptachItem *item = calloc(1, sizeof(struct _DisptachItem));
	MEMORY_MANAGEMENT_INITIALIZE(item);
	if ( item == NULL ) return pthread_mutex_unlock(&(worker->_dispatcherGuardian)), errno = ENOMEM, -1;
	item->item = block;
	TAILQ_INSERT_TAIL(&(worker->listOfDispatches), item, items);
#endif
	
	pthread_mutex_unlock(&(worker->_dispatcherGuardian));
	return 0;
}

UIDispatchBlock *UIPopDispatchBlock(UIDispatchWorker *restrict worker) {
	pthread_mutex_lock(&(worker->_dispatcherGuardian));
	
	UIDispatchBlock *block = NULL;
	
#ifdef COBJ
	if ( _listOfDisptaches == NULL ) return errno = EFAULT, pthread_mutex_unlock(&_dispatcherGuardian), (UIDispatchBlock *)NULL;
	
	ValueRef blockValue = getObjectAtIndex(_listOfDisptaches, 0);
	if ( blockValue == NULL ) return pthread_mutex_unlock(&_dispatcherGuardian),  (UIDispatchBlock *)NULL;
	
	block = getValuePointer(blockValue);
	removeFirstObject(_listOfDisptaches);
#else
	 struct _DisptachItem *item = worker->listOfDispatches.tqh_first;
	if ( item == NULL ) return pthread_mutex_unlock(&(worker->_dispatcherGuardian)), (UIDispatchBlock *)NULL;
	block = (UIDispatchBlock *) item->item;
	TAILQ_REMOVE(&(worker->listOfDispatches), item, items);
	release(item);
#endif
	
	pthread_mutex_unlock(&(worker->_dispatcherGuardian));
	return block;
}

void UIPopAndPerform(UIDispatchWorker *restrict worker) {
	UIDispatchBlock *block = UIPopDispatchBlock(worker);
	UIPerformDispatchBlock(worker, block);
}

