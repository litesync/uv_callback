#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "../uv_callback.c"
#include <assert.h>

uv_thread_t  worker_thread;
uv_barrier_t barrier;
uv_async_t   stop_worker;

int progress_called = 0;
int static_call_counter = 0;
int dynamic_call_counter = 0;

char *msg1 = "Hello World!";
char *msg2 = "How you doing?";
char *msg3 = "I'm programming with libuv! :P";
char *msg4 = "The fourth message";
char *msg5 = "The fifth message";
char *msg6 = "The sixth message";

/* Common ********************************************************************/

void on_walk(uv_handle_t *handle, void *arg) {
   uv_close(handle, NULL);
}

/* Worker Thread *************************************************************/

struct numbers {
	int number1;
	int number2;
	int result;
};

uv_callback_t cb_progress;
uv_callback_t cb_static_pointer;
uv_callback_t cb_dynamic_pointer;
uv_callback_t cb_sum;
uv_callback_t cb_sum2;

void * on_progress(uv_callback_t *callback, void *data) {
   printf("progress: %d %%\n", (int)data);
   progress_called++;
}

void * on_static_pointer(uv_callback_t *callback, void *data) {
   printf("static pointer: (%p) %s\n", data, (char*)data);
   //assert(strcmp((char*)data, msg1) == 0);
   static_call_counter++;
}

void * on_dynamic_pointer(uv_callback_t *callback, void *data) {
   printf("dynamic pointer: (%p) %s\n", data, (char*)data);
   free(data);
   dynamic_call_counter++;
}

void * on_sum(uv_callback_t *callback, void *data) {
   struct numbers *request = (struct numbers *)data;
   struct numbers *response = malloc(sizeof(struct numbers));
   assert(response != 0);
   printf("sum (%p) number1: %d  number2: %d\n", data, request->number1, request->number2);
   response->number1 = request->number1;
   response->number2 = request->number2;
   response->result = request->number1 + request->number2;
   free(request);
   printf("sum result: %d\n", response->result);
   return response;
}

void * on_sum2(uv_callback_t *callback, void *data) {
   struct numbers *request = (struct numbers *)data;
   int result = request->number1 + request->number2;
   printf("sum (%p) number1: %d  number2: %d  result: %d\n", data, request->number1, request->number2, result);
   free(request);
   return (void*)result;
}

void stop_worker_cb(uv_async_t *handle) {
   puts("signal received to stop worker thread");
   uv_stop(handle->loop);
}

void worker_start(void *arg) {
   uv_loop_t loop;
   int rc;

   uv_loop_init(&loop);

   rc = uv_callback_init(&loop, &cb_progress, on_progress, UV_COALESCE);
   printf("uv_callback_init rc=%d\n", rc);
   assert(rc == 0);

   rc = uv_callback_init(&loop, &cb_static_pointer, on_static_pointer, UV_DEFAULT);
   printf("uv_callback_init rc=%d\n", rc);
   assert(rc == 0);

   rc = uv_callback_init(&loop, &cb_dynamic_pointer, on_dynamic_pointer, UV_DEFAULT);
   printf("uv_callback_init rc=%d\n", rc);
   assert(rc == 0);

   rc = uv_callback_init(&loop, &cb_sum, on_sum, UV_DEFAULT);
   printf("uv_callback_init rc=%d\n", rc);
   assert(rc == 0);

   rc = uv_callback_init(&loop, &cb_sum2, on_sum2, UV_DEFAULT);
   printf("uv_callback_init rc=%d\n", rc);
   assert(rc == 0);

   uv_async_init(&loop, &stop_worker, stop_worker_cb);

   /* signal to the main thread the the listening socket is ready */
   uv_barrier_wait(&barrier);

   /* run the event loop */
   uv_run(&loop, UV_RUN_DEFAULT);

   /* cleanup */
   puts("cleaning up worker thread");
   uv_walk(&loop, on_walk, NULL);
   uv_run(&loop, UV_RUN_DEFAULT);
   uv_loop_close(&loop);

}

/* Main Thread ***************************************************************/

uv_callback_t cb_result;

void * on_result(uv_callback_t *callback, void *data) {
   struct numbers *response = (struct numbers *)data;
   printf("sum result (%p)  number1: %d  number2: %d  result=%d\n", data, response->number1, response->number2, response->result);
   assert(response->number1 == 123);
   assert(response->number2 == 456);
   assert(response->result == 579);
   free(response);
   uv_stop(((uv_handle_t*)callback)->loop);
   return NULL;
}

int main() {
   uv_loop_t *loop = uv_default_loop();
   struct numbers *req, *resp;
   int rc, result;

   uv_barrier_init(&barrier, 2);

   uv_thread_create(&worker_thread, worker_start, NULL);

   /* wait until the worker thread is ready */
   uv_barrier_wait(&barrier);

   /* fire the callbacks */

   /* this calls can coalesce */
   uv_callback_fire(&cb_progress, (void*)10, NULL);
   uv_callback_fire(&cb_progress, (void*)20, NULL);
   uv_callback_fire(&cb_progress, (void*)30, NULL);
   uv_callback_fire(&cb_progress, (void*)40, NULL);
   uv_callback_fire(&cb_progress, (void*)50, NULL);
   uv_callback_fire(&cb_progress, (void*)60, NULL);
   uv_callback_fire(&cb_progress, (void*)70, NULL);
   uv_callback_fire(&cb_progress, (void*)80, NULL);
   uv_callback_fire(&cb_progress, (void*)90, NULL);
   uv_callback_fire(&cb_progress, (void*)99, NULL);

   /* this calls should not coalesce, and the memory should not be released */
   uv_callback_fire(&cb_static_pointer, msg1, NULL);
   uv_callback_fire(&cb_static_pointer, msg2, NULL);
   uv_callback_fire(&cb_static_pointer, msg3, NULL);

   /* this calls should not coalesce, and the memory must be released */
   uv_callback_fire(&cb_dynamic_pointer, strdup(msg4), NULL);
   uv_callback_fire(&cb_dynamic_pointer, strdup(msg5), NULL);
   uv_callback_fire(&cb_dynamic_pointer, strdup(msg6), NULL);

   /* make a call and receive the response asynchronously */

   /* set the result callback */
   rc = uv_callback_init(loop, &cb_result, on_result, UV_DEFAULT);
   printf("uv_callback_init rc=%d\n", rc);
   assert(rc == 0);

   /* allocate memory fo the arguments */
   req = malloc(sizeof(struct numbers));
   assert(req != 0);
   req->number1 = 123;
   req->number2 = 456;
   req->result = 0;

   /* call the function in the other thread */
   uv_callback_fire(&cb_sum, req, &cb_result);

   /* run the event loop */
   puts("running the event loop in the main thread");
   uv_run(loop, UV_RUN_DEFAULT);

   /* close the handles from this loop */
   puts("cleaning up the main thread");
   uv_walk(loop, on_walk, NULL);
   uv_run(loop, UV_RUN_DEFAULT);
   uv_loop_close(loop);

   /* check the values */
   assert(progress_called > 0);
   assert(static_call_counter == 3);
   assert(dynamic_call_counter == 3);


   /* test asynchronous calls */

   /* allocate memory fo the arguments */
   req = malloc(sizeof(struct numbers));
   assert(req != 0);
   req->number1 = 111;
   req->number2 = 222;
   req->result = 0;

   /* call the function in the other thread */
   rc = uv_callback_fire_sync(&cb_sum, req, (void**)&resp, 10000);
   printf("uv_callback_fire_sync rc=%d\n", rc);
   assert(rc == 0);
   printf("response=%p\n", resp);
   assert(resp != 0);
   assert(resp->result == 333);
   free(resp);

   /* allocate memory fo the arguments */
   req = malloc(sizeof(struct numbers));
   assert(req != 0);
   req->number1 = 111;
   req->number2 = 222;
   req->result = 0;

   /* call the function in the other thread - this one returns an integer */
   rc = uv_callback_fire_sync(&cb_sum2, req, (void**)&result, 10000);
   printf("uv_callback_fire_sync rc=%d\n", rc);
   assert(rc == 0);
   printf("result=%d\n", result);
   assert(result == 333);

   /* send a signal to the worker thread to exit */
   uv_async_send(&stop_worker);

   /* wait the worker thread to exit */
   uv_thread_join(&worker_thread);

   puts("All tests pass!");
   return 0;
}
