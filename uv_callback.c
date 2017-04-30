#include <stdlib.h>
#include "uv_callback.h"

// what is not covered: destroy of uv_callback handle does not release all the resources


/*****************************************************************************/
/* RECEIVER / CALLED THREAD **************************************************/
/*****************************************************************************/

/* Dequeue *******************************************************************/

void * dequeue_call(uv_callback_t* callback) {
   uv_call_t *current, *prev = NULL;

   uv_mutex_lock(&callback->mutex);

   current = callback->queue;
   while (current && current->next != NULL) {
      prev = current;
      current = current->next;
   }

   if (prev)
      prev->next = NULL;
   else
      callback->queue = NULL;

   uv_mutex_unlock(&callback->mutex);

   return current;
}

/* Callback Function Call ****************************************************/

void uv_callback_async_cb(uv_async_t* handle) {
   uv_callback_t* callback = (uv_callback_t*) handle;

   if (callback->usequeue) {
      uv_call_t *call;
      while (call = dequeue_call(callback)) {
         void *result = callback->function(callback, call->data);
         if (call->notify) uv_callback_fire(call->notify, result, NULL);
         free(call);
      }
   } else {
      callback->function(callback, callback->arg);
   }

}

/* Initialization ************************************************************/

int uv_callback_init(uv_loop_t* loop, uv_callback_t* callback, uv_callback_func function, int callback_type) {
   int rc;

   if (!loop || !callback || !function) return UV_EINVAL;

   callback->function = function;
   callback->queue = NULL;

   switch(callback_type) {
   case UV_DEFAULT:
      callback->usequeue = 1;
      uv_mutex_init(&callback->mutex);
      break;
   case UV_COALESCE:
      callback->usequeue = 0;
      break;
   default:
      return UV_EINVAL;
   }

   return uv_async_init(loop, (uv_async_t*) callback, uv_callback_async_cb);
}

/*****************************************************************************/
/* SENDER / CALLER THREAD ****************************************************/
/*****************************************************************************/

/* Asynchronous Callback Firing **********************************************/

int uv_callback_fire(uv_callback_t* callback, void *data, uv_callback_t* notify) {

   if (!callback) return UV_EINVAL;

   /* if there is a notification callback set, then the call must use a queue */
   if (notify!=NULL && callback->usequeue==0) return UV_EINVAL;

   if (callback->usequeue) {
      /* allocate a new call info */
      uv_call_t *call = malloc(sizeof(uv_call_t));
      if (!call) return UV_ENOMEM;
      /* save the call info */
      call->data = data;
      call->notify = notify;
      /* add the call to the queue */
      uv_mutex_lock(&callback->mutex);
      call->next = callback->queue;
      callback->queue = call;
      uv_mutex_unlock(&callback->mutex);
   } else {
      callback->arg = data;
   }

   /* call uv_async_send */
   return uv_async_send((uv_async_t*)callback);
}
