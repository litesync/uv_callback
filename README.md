# uv_callback

[![Build Status](https://travis-ci.org/litesync/uv_callback.svg?branch=master)](https://travis-ci.org/litesync/uv_callback)

Module to call functions on other libuv threads.

It is an alternative to uv_async with some differences:

 * It supports coalescing and non coalescing calls
 * It supports synchronous and asynchronous calls
 * It supports the transfer of an argument to the called function
 * It supports result notification callback


# Usage Examples


## Sending progress to the main thread

In this case the calls can and must coalesce to avoid flooding the event loop if the
work is running too fast.

The call coalescing is enabled using the UV_COALESCE constant.

### In the receiver thread

```C
uv_callback_t progress;

void * on_progress(uv_callback_t *handle, void *value) {
   printf("progress: %d\n", (int)value);
}

uv_callback_init(loop, &progress, on_progress, UV_COALESCE);
```

### In the sender thread

```C
uv_callback_fire(&progress, (void*)value, NULL);
```

⚠️ Do **NOT** send pointers with UV_COALESCE, only scalar values


## Sending allocated data that must be released

In this case the calls cannot coalesce because it would cause data loss and memory leaks.

So instead of UV_COALESCE it uses UV_DEFAULT.

### In the receiver thread

```C
uv_callback_t send_data;

void * on_data(uv_callback_t *handle, void *data) {
  do_something(data);
  free(data);
}

uv_callback_init(loop, &send_data, on_data, UV_DEFAULT);
```

### In the sender thread

```C
uv_callback_fire(&send_data, data, NULL);
```


## Firing the callback synchronously

In this case the thread firing the callback will wait until the function
called on the other thread returns.

It means it will not use the current thread event loop and it does not
even need to have one (it can be used in caller threads with no event loop
but the called thread needs to have one).

The last argument is the timeout in milliseconds.

If the called thread does not respond within the specified timeout time
the function returns `UV_ETIMEDOUT`.


### In the called thread

```C
uv_callback_t send_data;

void * on_data(uv_callback_t *handle, void *args) {
  int result = do_something(args);
  free(args);
  return (void*)result;
}

uv_callback_init(loop, &send_data, on_data, UV_DEFAULT);
```

### In the caller thread

```C
int result;
uv_callback_fire_sync(&send_data, args, (void**)&result, 1000);
```


## Firing the callback and getting the result asynchronously

In this case the thread firing the callback will receive the result in its
own callback when the function called on the other thread loop returns.

Note that there are 2 callback definitions here, one for each thread.

### In the called thread

```C
uv_callback_t send_data;

void * on_data(uv_callback_t *handle, void *data) {
  int result = do_something(data);
  free(data);
  return (void*)result;
}

uv_callback_init(loop, &send_data, on_data, UV_DEFAULT);
```

### In the calling thread

```C
uv_callback_t result_cb;

void * on_result(uv_callback_t *handle, void *result) {
  printf("The result is %d\n", (int)result);
}

uv_callback_init(loop, &result_cb, on_result, UV_DEFAULT);

uv_callback_fire(&send_data, data, &result_cb);
```


# Non-static objects

If the `uv_callback_t` object is allocated on memory then you can inform which function should be used to release it using the `uv_callback_init_ex` function:

```C
uv_callback_t *cb = malloc(sizeof(uv_callback_t));
if (!cb) ...
uv_callback_init_ex(loop, &cb, get_values, UV_DEFAULT, free, NULL);
```

You can discard it on the same thread it was created using the `uv_callback_stop` function just before closing the loop handles and then `uv_callback_release` on the callback of the `uv_close`. The object will be released when the reference counter reaches 0.

```C
void on_close(uv_handle_t *handle) {
   if (uv_is_callback(handle)) {
      uv_callback_release((uv_callback_t*) handle);
   }
}

void on_walk(uv_handle_t *handle, void *arg) {
   uv_close(handle, on_close);
}

...

   /* run the event loop */
   uv_run(&loop, UV_RUN_DEFAULT);

   /* the event loop stopped */
   uv_callback_stop_all(&loop);
   uv_walk(&loop, on_walk, NULL);
   uv_run(&loop, UV_RUN_DEFAULT);
   uv_loop_close(&loop);
```

You can also inform in the last argument which function should be used to release the result from the callback, if it is not used.

```C
void * get_data(uv_callback_t *handle, void *arg) {
  struct my_data *result = malloc(sizeof(struct my_data))
  result->data1 = calc1(arg);
  result->data2 = calc2(arg);
  free(arg);
  return result;
}

void thread_init() {
  uv_callback_t *cb = malloc(sizeof(uv_callback_t));
  if (!cb) ...
  uv_callback_init_ex(loop, &cb, get_data, UV_DEFAULT, free, free);
  ...
}
```

Check the [test](test/test.c) for more usage examples.


# Requirement

⚠️ For `uv_callback` to work you must **NOT** use `uv_async` handles in the same loops that use `uv_callback`!

This requirement applies to the [this commit](https://github.com/litesync/uv_callback/commit/f9e54ca561e40cb61398534c3b069c800c537a41) that ensures that the order of calls will be preserved.

If you want to use uv_async handles with uv_callback, use the [previous commit](https://github.com/litesync/uv_callback/commit/f19aba8b9c21f860f9e00fd5654baa6aeff81a76). It preserves the 
order of calls only within a single callback handle, not globally.


# License

MIT

# Contact

contact AT litereplica DOT io
