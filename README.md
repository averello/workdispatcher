workdispatcher
==============

A little library in C for managing task with operations via an operation queue interface.

Documentation
-------------
This project uses [doxygen](http://www.stack.nl/~dimitri/doxygen/index.html) (http://www.stack.nl/~dimitri/doxygen/index.html) for the code documentation.
Just point doxygen to the doc/Doxyfile and the html documentation will be generated.

Command line exemple :
```bash
cd doc
doxygen Doxyfile
```

Dependencies
------------

This library uses [libmemorymanagement](https://github.com/averello/memorymanagement)(https://github.com/averello/memorymanagement) internally.


Usage
-----

An example of code:
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "operationQueue.h"
#include <memory_management/memory_management.h>

#define ITER 10

void opf(WDOperation *operation, void *arg);

int main () {
	char *string = "Hello world";
	
	// Create the operation queue
	WDOperationQueue *operationQueue = WDOperationQueueAllocate();
	// Set a name (for debugging)
	WDOperationQueueSetName(operationQueue, "queue.name");
	
	// Add ITER operations to the queue that execute the function opf with string as argument
	for (unsigned int i=0; i<ITER; i++) {
		WDOperation *operation = WDOperationAllocate(opf, string);
		WDOperationQueueAddOperation(operationQueue, operation);
		// related to libmemorymanagement
		release(operation);
	}
	
	// Block until all operations are finished
	WDOperationQueueWaitAllOperations(operationQueue);
	
	// memory management
	release(operationQueue);
	return 0;
}

// Operation's function
void opf(WDOperation *operation, void *arg) {
	static int i=0;
	char *string = arg;
	sleep(1);
	puts(string);
	
	// print the operation's queue name
	WDOperationQueue *queue = WDOperationCurrentOperationQueue(operation);
	puts(WDOperationQueueGetName(queue));
	
	// Add some more operations to the queue, why not?
	if (i++<ITER) {
		WDOperation *operation = WDOperationAllocate(opf, string);
		WDOperationQueueAddOperation(queue, operation);
		release(operation);
	}
	return;
}

```
