#include "utils.h"
#include <inttypes.h>

const char* duv_type_to_string(duv_type_t type) {
  switch (type) {
    case DUV_TIMER: return "uv_timer_t";
    case DUV_TCP: return "uv_tcp_t";
    case DUV_PIPE: return "uv_pipe_t";
  }
  return "unknown";
}

const char* duv_mask_to_string(duv_type_mask_t mask) {
  switch (mask) {
    case DUV_HANDLE_MASK: return "uv_handle_t";
    case DUV_TIMER_MASK: return "uv_timer_t";
    case DUV_STREAM_MASK: return "uv_stream_t";
    case DUV_TCP_MASK: return "uv_tcp_t";
    case DUV_PIPE_MASK: return "uv_pipe_t";
  }
  return "unknown";
}

uv_loop_t* duv_loop(duk_context *ctx) {
  duk_memory_functions funcs;
  duk_get_memory_functions(ctx, &funcs);
  return funcs.udata;
}

void duv_error(duk_context *ctx, int status) {
  duk_error(ctx, DUK_ERR_ERROR, "%s: %s", uv_err_name(status), uv_strerror(status));
}

void duv_check(duk_context *ctx, int status) {
  if (status < 0) duv_error(ctx, status);
}

#define KEYLEN sizeof(void*)*2+1
static char key[KEYLEN];

// Assumes buffer is at top of stack.
// replaces with this on top of stack.
void duv_setup_handle(duk_context *ctx, uv_handle_t *handle, duv_type_t type) {
  // Insert this before buffer
  duk_push_this(ctx);

  // Set buffer as uv-data internal property.
  duk_insert(ctx, -2);
  duk_put_prop_string(ctx, -2, "\xff""uv-data");

  // Set uv-type from provided parameter.
  duk_push_int(ctx, type);
  duk_put_prop_string(ctx, -2, "\xff""uv-type");

  // Store this object in the heap stack keyed by the handle's pointer address.
  // This will prevent it from being garbage collected and allow us to find
  // it with nothing more than the handle's address.
  duk_push_heap_stash(ctx);
  duk_dup(ctx, -2);
  snprintf(key, KEYLEN, "%"PRIXPTR, (uintptr_t)handle);
  duk_put_prop_string(ctx, -2, key);
  duk_pop(ctx);

  // Store the context in the handle so it can use duktape APIs.
  handle->data = ctx;
}

void duv_push_handle(duk_context *ctx, void *handle) {
  duk_push_heap_stash(ctx);
  snprintf(key, KEYLEN, "%"PRIXPTR, (uintptr_t)handle);
  duk_get_prop_string(ctx, -1, key);
  duk_remove(ctx, -2);
}

void* duv_get_handle(duk_context *ctx, int index) {
  duk_get_prop_string(ctx, index, "\xff""uv-data");
  void* handle = duk_get_buffer(ctx, -1, 0);
  duk_pop(ctx);
  return handle;
}

duk_bool_t duv_is_handle_of(duk_context *ctx, int index, duv_type_mask_t mask) {
  if (!duk_is_object(ctx, index)) return 0;
  duk_get_prop_string(ctx, index, "\xff""uv-type");
  int type = duk_get_int(ctx, -1);
  duk_bool_t is = (1 << type) & mask;
  duk_pop(ctx);
  return is;
}

void* duv_require_this_handle(duk_context *ctx, duv_type_mask_t mask) {
  duk_push_this(ctx);
  if (!duv_is_handle_of(ctx, -1, mask)) {
    duk_pop(ctx);
    duk_error(ctx, DUK_ERR_TYPE_ERROR, "this must be %s", duv_mask_to_string(mask));
  }
  void *handle = duv_get_handle(ctx, -1);
  duk_insert(ctx, 0);
  return handle;
}
