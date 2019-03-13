/*!
 *  @file operationQueue.h
 *  @brief Work Dispatch Module.
 *  @details This module used to create operations and operations queues.
 *
 *  Created by @author George Boumis
 *  @date 2013/12/11.
 *	@version 1.1
 *  @copyright Copyright (c) 2013 George Boumis <developer.george.boumis@gmail.com>. All rights reserved.
 *
 *  @defgroup wd Work Dispatch Module
 *
 *	@mainpage Work Dispath Documentation
 *	@section intro_sec Introduction
 *	A little library in `C` for managing task with operations via an operation queue interface. See @ref wd for documentation of this library.
 *	Please refer to [workdispatcher](https://github.com/averello/workdispatcher)
 */

#ifndef workdipatcher_dispatch_h
#define workdipatcher_dispatch_h

#ifdef _cplusplus
extern "C" {
#endif
    
#include <stdbool.h>
    
#define WD_NONNULL __attribute__ ((nonnull (1)))

/*!
 *  @typedef struct _wd_operation_t WDOperation
 *  @brief An operation structure.
 *  @ingroup wd
 *
 *
 *	Overview
 *	========
 *  `WDOperation` is an opaque structure used to encapsulate the code and data associated with a single task. An operation object is a run-once object that is, it executes its task once and cannot be used to execute it again. You typically execute operations by adding them to an operation queue (@ref WDOperationQueue). An operation queue executes its operations directly on **its proper thread**.
 *
 *	Responding to cancel {#respondingToCancel}
 *	====================
 *	@par
 *	Once you add an operation to a queue, the operation is out of your hands. The queue takes over and handles the scheduling of that task. However, if you decide later that you do not want to execute the operation after all, you can cancel the operation to prevent it from consuming CPU time needlessly. You do this by calling the @ref WDOperationCancel function with the operation object itself or by calling the @ref WDOperationQueueCancelAllOperations function with the @ref WDOperationQueue. 
 *
 *	@par
 *	Canceling an operation does not immediately force it to stop what it is doing. Although respecting the value returned by the @ref WDOperationGetFlags function is expected of all operations, your code must explicitly check the value returned by this function and abort as needed. The default implementation of @ref WDOperationQueue does include checks for cancellation. For example, if you cancel an operation before is started, the operation is never executed.
 *	@par
 *	You should always support cancellation semantics in any custom code you write. In particular, your main task code should periodically check the value of the @ref WDOperationGetFlags function. If the flags ever returns contains true for `canceled`, your operation object should clean up and exit as quickly as possible.
 */
typedef struct _wd_operation_t WDOperation;

/*!
 *  @typedef typedef struct _wd_operation_queue_t WDOperationQueue
 *  @brief An operation queue structure.
 *  @ingroup wd
 *
 *
 *	Overview
 *	========
 *	@par
 *  The `WDOperationQueue` class regulates the execution of a set of @ref WDOperation objects. After being added to a queue, an operation remains in that queue until it is explicitly canceled or finishes executing its task. An application may create multiple operation queues and submit operations to any of them.
 *	@par
 *	You cannot directly remove an operation from a queue after it has been added. An operation remains in its queue until it reports that it is finished with its task. Finishing its task does not necessarily mean that the operation performed that task to completion. An operation can also be canceled. Canceling an operation object leaves the object in the queue but notifies the object that it should abort its task as quickly as possible. For currently executing operations, this means that the operation object’s work code must check the cancellation state, stop what it is doing. For operations that are queued but not yet executing, the queue does not start the operation mark it as finished. */
typedef struct _wd_operation_queue_t WDOperationQueue;

/*!
 *  @typedef struct _wd_operation_flags_t
 *  @brief An operation's flags.
 *  @ingroup wd
 */
typedef struct _wd_operation_flags_t {
	bool canceled:1; /*!< The `canceled` flag lets clients know that the cancellation of an operation was requested. Support for cancellation is voluntary but encouraged and your own code. See also @ref [“Responding to the Cancel Command.”](@ref respondingToCancel) */
	bool finished:1; /*!< The `finished` flag lets clients know that an operation finished its task successfully or was cancelled and is exiting. */
	bool executing:1; /*!< The `executing` flag lets clients know whether the operation is actively working on its assigned task. */
} wd_operation_flags_t;


/*!
 *  @typedef typedef void (*wd_operation_f) (WDOperation *const , void *const)
 *  @brief The prororype of an operation's function.
 *  @ingroup wd
 *
 *	@param[in] operation the operation that executes this task
 *	@param[in,out] argument the argument that was used to create this operation. You can use this parameter as you please.
 */
typedef void (*wd_operation_f) (WDOperation *const operation, void *const argument);

/*!
 *  @fn WDOperation *WDOperationCreate(const wd_operation_f function, void *const argument)
 *  @brief Creates an operation.
 *  @ingroup wd
 *	@details The argument of the operation is retained (see [libmemorymanagement](https://github.com/averello/memorymanagement)). If the argument is not allocated (managed) by the [libmemorymanagement](https://github.com/averello/memorymanagement) then *you* have to make *sure* that this pointer does not gets freed before the execution of the operation by the operation queue.
 *	@param[in] function the function that the operation will execute
 *	@param[in,out] argument the argument to pass when executing the operation's function
 *	@returns an initialized @ref WDOperation object.
 */
WDOperation *WDOperationCreate(const wd_operation_f function, void *const argument) WD_NONNULL;

/*!
 *  @fn WDOperation *WDOperationRetain(WDOperation *const operation)
 *  @brief Increments the retain count of a WDOperation.
 *  @ingroup wd
 *	@param[in] operation the operation to retain.
 *	@returns the same you passed in as the @a operation parameter.
 */
WD_NONNULL WDOperation *WDOperationRetain(WDOperation *const operation) WD_NONNULL;
	
/*!
 *  @fn void WDOperationRelease(WDOperation *const operation)
 *  @brief Decrements the retain count of a WDOperation.
 *  @ingroup wd
 *	@param[in] operation the operation to release.
 *	@warning You should never release the operation from within its executing function. Doing so results in unexpected behavior and will probably crash the application.
 */
void WDOperationRelease(WDOperation *const operation) WD_NONNULL;

/*!
 *  @fn WDOperationQueue *WDOperationQueueRetain(WDOperationQueue *const queue)
 *  @brief Increments the retain count of a WDOperationQueue.
 *  @ingroup wd
 *	@param[in] queue the operation to retain.
 *	@returns the same you passed in as the @a queue parameter.
 */
WD_NONNULL WDOperationQueue *WDOperationQueueRetain(WDOperationQueue *const queue) WD_NONNULL;
	
/*!
 *  @fn void WDOperationQueueRelease(WDOperationQueue *const queue)
 *  @brief Decrements the retain count of a WDOperationQueue.
 *  @ingroup wd
 *	@param[in] queue the queue to release.
 *	@warning You should never release the operation queue that executes an operation from within the operation. Doing so results in unexpected behavior and will probably crash the application.
 */
void WDOperationQueueRelease(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn void WDOperationCancel(WDOperation *const operation)
 *  @brief Advises the operation object that it should stop executing its task.
 *  @ingroup wd
 *	@details This function does not force your operation code to stop. Instead, it updates the object’s internal flags to reflect the change in state. If the operation has already finished executing, this function has no effect. Canceling an operation that is currently in an operation queue, but not yet executing, makes it possible to remove the operation from the queue sooner than usual.
 *
 *	For more information on what you must do in your operation objects to support cancellation, see [“Responding to the Cancel Command.”](@ref respondingToCancel)
 *	@param[in] operation the operation to mark as canceled
 */
void WDOperationCancel(WDOperation *const operation) WD_NONNULL;

/*!
 *  @fn wd_operation_flags_t WDOperationGetFlags(WDOperation *const operation)
 *  @brief Returns a structure of flags that indicate the state of this operation.
 *  @ingroup wd
 *	@details See @ref wd_operation_flags_t for explication of different fields of this structure.
 *	@param[in] operation the operation whose flags are demanded
 *	@returns the flag structure
 */
wd_operation_flags_t WDOperationGetFlags(WDOperation *const operation) WD_NONNULL;

/*!
 *  @fn WDOperationQueue *WDOperationCurrentOperationQueue(WDOperation *const operation)
 *  @brief Returns the operation queue that launched the current operation.
 *  @ingroup wd
 *	@details You can use this function from within a running operation function to get a reference to the operation queue that started it. Calling this function from outside the context of a running operation typically results in `NULL` being returned.
 *	@param[in] operation the operation that is running in the context of a queue
 *	@returns The operation queue that started the operation or `NULL` if the queue could not be determined
 */
WDOperationQueue *WDOperationCurrentOperationQueue(WDOperation *const operation) WD_NONNULL;

/*!
 *  @fn void WDOperationWaitUntilFinished(WDOperation *const operation)
 *  @brief Blocks execution of the current thread until the receiver finishes.
 *  @ingroup wd
 *	@details
 *	This function should never be called in the context of a running operation with the same operation as argument and should avoid calling it passing any operations submitted to the same operation queue as itself. Doing so can cause the operation to deadlock. It is generally safe to call this function with an operation that is in a different operation queue, although it is still possible to create deadlocks if each operation waits on the other.
 *
 *	A typical use for this function would be to call it from the code that created the operation in the first place. After submitting the operation to a queue, you would call this function to wait until that operation finished executing.
 *	@param[in] operation the operation that is running in the context of a queue
 */
void WDOperationWaitUntilFinished(WDOperation *const operation) WD_NONNULL;



/*******************/
/* Operation queue */
/*******************/

/*!
 *  @fn WDOperationQueue *WDOperationQueueAllocate(void)
 *  @brief Creates an operation queue.
 *  @ingroup wd
 *	@returns an initialized @ref WDOperationQueue object.
 */
WDOperationQueue *WDOperationQueueAllocate(void);

/*!
 *  @fn int WDOperationQueueAddOperation(WDOperationQueue *const queue, WDOperation *const operation)
 *  @brief Adds the specified operation object to the queue.
 *  @ingroup wd
 *	@details This function can be called from a currently running operation. This function is thread-safe.
 *	@param[in] queue the operation queue
 *	@param[in] operation The operation object to be added to the queue. In memory-managed applications, this object is retained by the operation queue (see [libmemorymanagement](https://github.com/averello/memorymanagement)).
 *	@returns a boolean indicating whether the operation was correctly submitted to the operation queue
 */
bool WDOperationQueueAddOperation(WDOperationQueue *const queue, WDOperation *const operation) __attribute__ ((nonnull (1,2)));

/*!
 *  @fn void WDOperationQueueSuspend(WDOperationQueue *const queue)
 *  @brief Modifies the execution of pending operations
 *  @details The queue stops scheduling queued operations for execution.
 *  @ingroup wd
 *	@param[in] queue the operation queue
 */
void WDOperationQueueSuspend(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn void WDOperationQueueResume(WDOperationQueue *const queue)
 *  @brief Modifies the execution of pending operations
 *  @details The queue resumes scheduling queued operations for execution.
 *  @ingroup wd
 *    @param[in] queue the operation queue
 */
void WDOperationQueueResume(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn bool WDOperationQueueIsSuspended(WDOperationQueue *const queue)
 *  @brief Returns whether the queue is suspended or not.
 *  @ingroup wd
 *	@param[in] queue the operation queue
 *	@returns Returns a Boolean value indicating whether the receiver is scheduling queued operations for execution.
 */
bool WDOperationQueueIsSuspended(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn void WDOperationQueueCancelAllOperations(WDOperationQueue *const queue)
 *  @brief Cancels all queued and executing operations.
 *  @ingroup wd
 *	@details This function sends a cancel message to all operations currently in the queue. Queued operations are cancelled before they begin executing. If an operation is already executing, it is up to that operation to recognize the cancellation and stop what it is doing.
 *	@param[in] queue the operation queue
 */
void WDOperationQueueCancelAllOperations(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn void WDOperationQueueWaitAllOperations(WDOperationQueue *const queue)
 *  @brief Blocks the current thread until all of the receiver’s queued and executing operations finish executing.
 *  @ingroup wd
 *	@details When called, this function blocks the current thread and waits for the receiver’s current and queued operations to finish executing. While the current thread is blocked, the receiver continues to launch already queued operations and monitor those that are executing. During this time, the current thread cannot add operations to the queue, *neither* other threads may. Once all of the pending operations are finished, this method returns.
 *
 *	If there are no operations in the queue, this method returns immediately.
 *	@param[in] queue the operation queue
 */
void WDOperationQueueWaitAllOperations(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn void WDOperationQueueSetName(WDOperationQueue *const queue, const char *const name)
 *  @brief Assigns the specified name to the opeartion queue.
 *  @ingroup wd
 *	@details Names provide a way for you to identify your operation queues at run time. Tools may also use this name to provide additional context during debugging or analysis of your code.
 *	@param[in] queue the operation queue
 *	@param[in] name the new name to associate with the operation queue.
 */
void WDOperationQueueSetName(WDOperationQueue *const queue, const char *const name) __attribute__ ((nonnull (1,2)));

/*!
 *  @fn const char * WDOperationQueueGetName(WDOperationQueue *const queue)
 *  @brief Returns the name of the operation queue.
 *  @ingroup wd
 *	@details The default value of this string is “WDOperationQueue <id>”, where <id> is the memory address of the operation queue.
 *	@param[in] queue the operation queue
 *	@returns The name of the operation queue.
 */
const char *WDOperationQueueGetName(WDOperationQueue *const queue) WD_NONNULL;

/*!
 *  @fn WDOperationQueue *WDOperationQueueMainQueue()
 *  @brief Returns the operation queue associated with the main thread.
 *  @ingroup wd
 *	@details The returned queue executes operations on the main thread.
 *	@returns The default operation queue bound to the main thread.
 *	@warning You should never call @ref WDOperationQueueRelease with the main queue, doing so will cause the application to crash.
 */
WDOperationQueue *WDOperationQueueMainQueue(void);

/*!
 *  @fn bool WDOperationQueueMainQueueLoop()
 *  @brief Start the main queues loop.
 *  @ingroup wd
 *	@details This function normally never returns. If you want to dispatch operations back to the main queue then you should probably do something like this
 *	~~~~~~~~~~
 *	int main(int argc, char *argv[]) {
 *		...
 *		return WDOperationQueueMainQueueLoop();
 *	}
 *	~~~~~~~~~~
 *	@warning You should never use @ref WDOperationQueueMainQueue to dispatch operations back to the main queue if this function was not never called.
 */
bool WDOperationQueueMainQueueLoop(void);
	
#ifdef _cplusplus
}
#endif


#endif
