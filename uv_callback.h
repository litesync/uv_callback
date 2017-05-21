#ifndef UV_CALLBACK_H
#define UV_CALLBACK_H
#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>


/* Typedefs */

typedef struct uv_callback_s   uv_callback_t;
typedef struct uv_call_s       uv_call_t;


/* Callback Functions */

typedef void* (*uv_callback_func)(uv_callback_t* handle, void *data);


/* Functions */

int uv_callback_init(uv_loop_t* loop, uv_callback_t* callback, uv_callback_func function, int callback_type);

int uv_callback_fire(uv_callback_t* callback, void *data, uv_callback_t* notify);

int uv_callback_fire_sync(uv_callback_t* callback, void *data, void** presult, int timeout);


/* Constants */

#define UV_DEFAULT      0
#define UV_COALESCE     1


/* Structures */

struct uv_callback_s {
   union {
      uv_async_t async;
      void *data;
   };
   int usequeue;
   uv_call_t *queue;          /* queue of calls to this callback */
   uv_mutex_t mutex;          /* mutex used to access the queue */
   uv_callback_func function; /* the function to be called */
   void *arg;                 /* data argument for coalescing calls (when not using queue) */
   uv_idle_t idle;            /* idle handle used to drain the queue if new async request was sent while an old one was being processed */
   int idle_active;           /* flags if the idle handle is active */
   uv_callback_t *master;     /* master callback handle */
};

struct uv_call_s {
   uv_call_t *next;           /* pointer to the next call in the queue */
   uv_callback_t *callback;   /* callback linked to this call */
   void *data;                /* data argument for this call */
   uv_callback_t *notify;     /* callback to be fired with the result of this one */
};


#ifdef __cplusplus
}
#endif
#endif  // UV_CALLBACK_H
