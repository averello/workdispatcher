//
//  testMainRunLoop.c
//  workdipatcher
//
//  Created by George Boumis on 1/2/14.
//  Copyright (c) 2014 George Boumis <developer.george.boumis@gmail.com>. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "operationQueue.h"
#include <memory_management/memory_management.h>

void opf(WDOperation *operation, void *arg);
void opmain(WDOperation *operation, void *arg);

int main(int argc, char *argv[]) {
	char *backgroundOperationArgument = "backgroundOperationArgument";
	WDOperationQueue *operationQueue = WDOperationQueueAllocate();
	WDOperation *backgroundOperation = WDOperationCreate(opf, backgroundOperationArgument);
	WDOperationQueueAddOperation(operationQueue, backgroundOperation);
	
	WDOperationQueueAddOperation(WDOperationQueueMainQueue(), backgroundOperation);
	WDOperationQueueMainQueueLoop();
	release(operationQueue);
	return 0;
}


void opf(WDOperation *operation, void *arg) {
	char *argument = (char *)arg;
	WDOperationQueue *queue = WDOperationCurrentOperationQueue(operation);
	printf("The background operation is executing in \"%s\" with argument \"%s\"\n", WDOperationQueueGetName(queue), argument);
	sleep(1);
	char *mainString = MEMORY_MANAGEMENT_ALLOC(strlen("main argument")+1);
	sprintf(mainString, "%s", "main argument");
	WDOperation *mainOperation = WDOperationCreate(opmain, mainString);
	WDOperationQueue *mainQueue = WDOperationQueueMainQueue();
	WDOperationQueueAddOperation(mainQueue, mainOperation);
	release(mainOperation);
}


void opmain(WDOperation *operation, void *arg) {
	char *argument = arg;
	WDOperationQueue *queue = WDOperationCurrentOperationQueue(operation);
	printf("The main operation is executing in \"%s\" with argument \"%s\"\n", WDOperationQueueGetName(queue), argument);
	release(argument);
}
