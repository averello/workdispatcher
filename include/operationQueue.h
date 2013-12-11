//
//  operationQueue.h
//  workdipatcher
//
//  Created by George Boumis on 11/12/13.
//  Copyright (c) 2013 George Boumis <developer.george.boumis@gmail.com>. All rights reserved.
//

#ifndef workdipatcher_dispatch_h
#define workdipatcher_dispatch_h

#ifdef _cplusplus
//extern "C" {
#endif
	
	
typedef struct _wd_operation_t WDOperation;
typedef struct _wd_operation_queue_t WDOperationQueue;

typedef struct _wd_operation_flags_t {
	unsigned int canceled:1;
	unsigned int finished:1;
	unsigned int executing:1;
} wd_operation_flags_t;


typedef void (*wd_operation_f) (WDOperation *, void *);

WDOperation *WDOperationAllocate(const wd_operation_f function, void *restrict argument);

void WDOperationCancel(WDOperation *operation);

wd_operation_flags_t WDOperationGetFlags(WDOperation *operation);

WDOperationQueue *WDOperationCurrentOperationQueue(WDOperation *operation);

void WDOperationWaitUntilFinished(WDOperation *operation);

/*******************/
/* Operation queue */

WDOperationQueue *WDOperationQueueAllocate(void);

int WDOperationQueueAddOperation(WDOperationQueue *restrict queue, WDOperation *restrict block);

void WDOperationQueueSuspend(WDOperationQueue *restrict queue, int choice);
int WDOperationQueueIsSuspended(WDOperationQueue *restrict queue);

void WDOperationQueueCancelAllOperations(WDOperationQueue *queue);

void WDOperationQueueWaitAllOperations(WDOperationQueue *queue);

void WDOperationQueueSetName(WDOperationQueue *queue, const char *name);
const char * WDOperationQueueGetName(WDOperationQueue *queue);
	
#ifdef _cplusplus
//}
#endif


#endif
