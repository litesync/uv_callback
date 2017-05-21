#include <stdlib.h>
#include "uv_callback.h"

// what is not covered: destroy of uv_callback handle does not release all the resources


/*****************************************************************************/
/* RECEIVER / CALLED THREAD **************************************************/
/*****************************************************************************/

#ifndef container_of
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

void uv_callback_idle_cb(uv_idle_t* handle);

/* Master Callback ***********************************************************/

void master_on_walk(uv_handle_t *handle, void *arg) {
   if (handle->type == UV_ASYNC && ((uv_callback_t*)handle)->usequeue) {
      *(uv_callback_t**)arg = (uv_callback_t *) handle;
   }
}

uv_callback_t * get_master_callback(uv_loop_t *loop) {
   uv_callback_t *callback=0;
   uv_walk(loop, master_on_walk, &callback);
   return callback;
}

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
      uv_call_t *call = dequeue_call(callback);
      if (call) {
         void *result = call->callback->function(call->callback, call->data);
         if (call->notify) uv_callback_fire(call->notify, result, NULL);
         free(call);
         /* don't check for new calls now to prevent the loop from blocking
         for i/o events. start an idle handle to call this function again */
         if (!callback->idle_active) {
            uv_idle_start(&callback->idle, uv_callback_idle_cb);
            callback->idle_active = 1;
         }
      } else {
         /* no more calls in the queue. stop the idle handle */
         uv_idle_stop(&callback->idle);
         callback->idle_active = 0;
      }
   } else {
      callback->function(callback, callback->arg);
   }

}

void uv_callback_idle_cb(uv_idle_t* handle) {
   uv_callback_t* callback = container_of(handle, uv_callback_t, idle);
   uv_callback_async_cb((uv_async_t*)callback);
}

/* Initialization ************************************************************/

int uv_callback_init(uv_loop_t* loop, uv_callback_t* callback, uv_callback_func function, int callback_type) {
   int rc;

   if (!loop || !callback || !function) return UV_EINVAL;

   memset(callback, 0, sizeof(uv_callback_t));
   callback->function = function;

   switch(callback_type) {
   case UV_DEFAULT:
      callback->usequeue = 1;
      callback->master = get_master_callback(loop);
      if (callback->master) {
         return 0;
      } else {
         uv_mutex_init(&callback->mutex);
         rc = uv_idle_init(loop, &callback->idle);
         if (rc) return rc;
      }
   case UV_COALESCE:
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
      call->callback = callback;
      /* if there is a master callback, use it */
      if (callback->master) callback = callback->master;
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

/* Synchronous Callback Firing ***********************************************/

struct call_result {
   int timed_out;
   int called;
   void *data;
};

void callback_on_walk(uv_handle_t *handle, void *arg) {
   uv_close(handle, NULL);
}

void * on_call_result(uv_callback_t *callback, void *data) {
   uv_loop_t *loop = ((uv_handle_t*)callback)->loop;
   struct call_result *result = loop->data;
   result->called = 1;
   result->data = data;
   uv_stop(loop);
}

void on_timer(uv_timer_t *timer) {
   uv_loop_t *loop = timer->loop;
   struct call_result *result = loop->data;
   result->timed_out = 1;
   uv_stop(loop);
}

int uv_callback_fire_sync(uv_callback_t* callback, void *data, void** presult, int timeout) {
   struct call_result result = {0};
   uv_loop_t loop;
   uv_callback_t notify;
   uv_timer_t timer;
   int rc=0;

   if (!callback || callback->usequeue==0) return UV_EINVAL;

   /* set the call result */
   uv_loop_init(&loop);
   uv_callback_init(&loop, &notify, on_call_result, UV_DEFAULT);
   loop.data = &result;

   /* fire the callback on the other thread */
   rc = uv_callback_fire(callback, data, &notify);
   if (rc) {
      uv_close((uv_handle_t *) &notify, NULL);
      goto loc_exit;
   }

   /* if a timeout is supplied, set a timer */
   if (timeout > 0) {
      uv_timer_init(&loop, &timer);
      uv_timer_start(&timer, on_timer, timeout, 0);
   }

   /* run the event loop */
   uv_run(&loop, UV_RUN_DEFAULT);

   /* exited the event loop */
   uv_walk(&loop, callback_on_walk, NULL);
   uv_run(&loop, UV_RUN_DEFAULT);
loc_exit:
   uv_loop_close(&loop);

   /* store the result */
   if (presult) *presult = result.data;
   if (rc==0 && result.timed_out) rc = UV_ETIMEDOUT;
   if (rc==0 && result.called==0) rc = UV_UNKNOWN;
   return rc;

}
