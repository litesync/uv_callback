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
called on the other loop returns.

The main difference from the previous example is the use of UV_SYNCHRONOUS.

This can be used when the worker thread does not have a loop.

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
uv_callback_fire(&send_data, data, UV_SYNCHRONOUS);
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

uv_callback_init(loop, &send_data, (uv_callback_cb)on_data, UV_DEFAULT);
```

### In the calling thread

```C
uv_callback_t data_sent;

void * on_data_sent(uv_callback_t *handle, void *result) {
  printf("The result is %d\n", (int)result);
}

uv_callback_init(loop, &data_sent, on_data_sent, UV_DEFAULT);

uv_callback_fire(&send_data, data, &data_sent);
```

# License

MIT

# Contact

contact AT litereplica DOT io
