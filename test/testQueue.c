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

void opf(WDOperation *operation, void *arg);

int main () {
	char *string = "Hello world";
	WDOperationQueue *operationQueue = WDOperationQueueAllocate();
	WDOperationQueueSetName(operationQueue, "queue.name");
	
	WDOperation *operation = WDOperationAllocate(opf, string);
	WDOperationQueueAddOperation(operationQueue, operation);
	
	
	WDOperationQueueWaitAllOperations(operationQueue);
	release(operation);
	release(operationQueue);
	return 0;
}

void opf(WDOperation *operation, void *arg) {
	char *string = arg;
	sleep(2);
	puts(string);
	puts(WDOperationQueueGetName(WDOperationCurrentOperationQueue(operation)));
	return;
}

