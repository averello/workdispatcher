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
	
	//for (unsigned int i=0; i<ITER; i++) {
		WDOperation *operation = WDOperationAllocate(opf, string);
		WDOperationQueueAddOperation(operationQueue, operation);
		release(operation);
	//}
	
	
	WDOperationQueueWaitAllOperations(operationQueue);
	release(operationQueue);
	return 0;
}

void opf(WDOperation *operation, void *arg) {
	static int i=0;
	char *string = arg;
	sleep(1);
	puts(string);
	WDOperationQueue *queue = WDOperationCurrentOperationQueue(operation);
	puts(WDOperationQueueGetName(queue));
	if (i++<ITER) {
		WDOperation *operation = WDOperationAllocate(opf, string);
		WDOperationQueueAddOperation(queue, operation);
		release(operation);
	}
	return;
}

