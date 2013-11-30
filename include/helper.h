/*!
 *  @file helper.h
 *  @brief ui
 *
 *  Created by @author George Boumis 
 *  @date 13/4/13.
 *  @copyright   Copyright (c) 2013 George Boumis. All rights reserved.
 */

#ifndef ui_helper_h
#define ui_helper_h

#ifdef _cplusplus
extern "C" {
#endif


typedef struct _dispatchWorker UIDispatchWorker;
typedef struct _dispatchBlock UIDispatchBlock;

typedef void (*voidf) (void *);


UIDispatchWorker *UIAllocateDispatchWorker(void);
UIDispatchBlock *UIAllocateDispatchBlock(const voidf function, const void *restrict argument);


int UIAddDispatchBlock(UIDispatchWorker *restrict worker, UIDispatchBlock *restrict block);
UIDispatchBlock *UIPopDispatchBlock(UIDispatchWorker *restrict worker);
void UIPerformDispatchBlock(UIDispatchWorker *restrict worker, UIDispatchBlock *restrict block);
void UIPopAndPerform(UIDispatchWorker *restrict worker);
	
#ifdef _cplusplus
}
#endif
#endif
