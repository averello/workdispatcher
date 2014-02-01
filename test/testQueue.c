//
//  testQueue.c
//  workdipatcher
//
//  Created by George Boumis on 11/12/13.
//  Copyright (c) 2013 George Boumis <developer.george.boumis@gmail.com>. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "operationQueue.h"
#include <memory_management/memory_management.h>

#define ITER 10

void opf(WDOperation *operation, void *arg);

int main () {
	char *string = "Hello world";
	WDOperationQueue *operationQueue = WDOperationQueueAllocate();
	WDOperationQueueSetName(operationQueue, "queue.name");
	
	for (unsigned int i=0; i<ITER; i++) {
		WDOperation *operation = WDOperationCreate(opf, string);
		WDOperationQueueAddOperation(operationQueue, operation);
		release(operation);
	}
	
	
	WDOperationQueueWaitAllOperations(operationQueue);
	
	WDOperationQueueSuspend(operationQueue, 1);
	WDOperation *fistoperation = NULL;
	char *string2 = "hohohohho";
	for (unsigned int i=0; i<ITER; i++) {
		WDOperation *operation = WDOperationCreate(opf, string2);
		WDOperationQueueAddOperation(operationQueue, operation);
		if (i==0) fistoperation = operation;
		release(operation);
	}
	
	WDOperationQueueSuspend(operationQueue, 0);
	WDOperationWaitUntilFinished(fistoperation);
	release(operationQueue);
//	memory_management_print_stats();
	return 0;
}

void opf(WDOperation *operation, void *arg) {
	static int i=0;
	char *string = arg;
	usleep(50000);
	puts(string);
	WDOperationQueue *queue = WDOperationCurrentOperationQueue(operation);
	puts(WDOperationQueueGetName(queue));
	if (i++<ITER) {
		WDOperation *operation = WDOperationCreate(opf, string);
		WDOperationQueueAddOperation(queue, operation);
		release(operation);
	}
	return;
}

