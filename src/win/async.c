/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>

#include "uv.h"
#include "internal.h"
#include "atomicops-inl.h"
#include "handle-inl.h"
#include "req-inl.h"

// 1.在调用 uv_async_init 初始化的时候，会给handle设置 async_cb，这个 async_cb 会在 req 完成后调用
// 


void uv__async_endgame(uv_loop_t* loop, uv_async_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING &&
      !handle->async_sent) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    uv__handle_close(handle);
  }
}


int uv_async_init(uv_loop_t* loop, uv_async_t* handle, uv_async_cb async_cb) {
  uv_req_t* req;

  // 初始化 handle
  uv__handle_init(loop, (uv_handle_t*) handle, UV_ASYNC);
  handle->async_sent = 0;
  // 设置回调
  handle->async_cb = async_cb;

  req = &handle->async_req;
  UV_REQ_INIT(req, UV_WAKEUP);

  // req 指向 handle->async_req,也就是让自己member的member指向自己
  req->data = handle;

  // handle flag set UV_HANDLE_ACTIVE
  // loop->active_handles++
  uv__handle_start(handle);

  return 0;
}


void uv__async_close(uv_loop_t* loop, uv_async_t* handle) {
  // 如果 async_sent 为0，就会跑到if里面去，此时说明回调已执行完，可以去endgame了
  if (!((uv_async_t*)handle)->async_sent) {
    uv__want_endgame(loop, (uv_handle_t*) handle);
  }

  // 执行到这里的时候，async_sent 也可能为1, 它为1说明处于pending状态，也就是说，pengding状态下可以close handle
  // 难怪在 uv__process_async_wakeup_req 里有判断 flags 的 UV_HANDLE_CLOSING 是否已置起，如果已置起，
  // 就去 uv__want_endgame 了，就不会执行 uv__work_done 回调函数了。

  uv__handle_closing(handle);
}

// 从代码来看，整体作用就是通知IOCP,应该是通知主线程
int uv_async_send(uv_async_t* handle) {
  uv_loop_t* loop = handle->loop;

  if (handle->type != UV_ASYNC) {
    /* Can't set errno because that's not thread-safe. */
    return -1;
  }

  // user 不应该对 closing or closed handle 进行 uv_async_send
  /* The user should make sure never to call uv_async_send to a closing or
   * closed handle. */
  assert(!(handle->flags & UV_HANDLE_CLOSING));

  // 给 handle->async_sent 置1并返回之前的值
  // 这里的意思是给它置1，并且之前的值要是0
  if (!uv__atomic_exchange_set(&handle->async_sent)) {

    // 通知IOCP，是走 req 发出去的，不是 handle。所以在处理 pending 的地方找不到 UV_ASYNC
    // 这里要注意下，通知是走 async_req 发出去的，上面的 uv_async_init 有对 async_req 初始化，设置的 type 是 UV_WAKEUP
    POST_COMPLETION_FOR_REQ(loop, &handle->async_req);
  }

  return 0;
}


void uv__process_async_wakeup_req(uv_loop_t* loop, uv_async_t* handle,
    uv_req_t* req) {
  
  // handle 必须是 UV_ASYNC
  // req 必须是 UV_WAKEUP
  assert(handle->type == UV_ASYNC);
  assert(req->type == UV_WAKEUP);

  // 在IOCP通知之前，有将它置1，也就是上面的 uv_async_send
  // 全局搜了下 async_sent，它的作用应该是标志 handle 处于 pending 状态
  // 处于 pending 状态下的 handle 不准 close
  handle->async_sent = 0;

  // 如果 UV_HANDLE_CLOSING 已置起，说明在任务处理完前就执行了 uv_close(handle)
  if (handle->flags & UV_HANDLE_CLOSING) {
    // handle->flags: UV_HANDLE_CLOSING -> UV_HANDLE_ENDGAME_QUEUED
    // 在设置 UV_HANDLE_ENDGAME_QUEUED 的时候，会将handle放入 loop->endgame_handles
    // 在大循环的最后会处理 loop->endgame_handles，也就是 core.c 里的 uv__process_endgames
    uv__want_endgame(loop, (uv_handle_t*)handle);
  } else if (handle->async_cb != NULL) {
    // 如果没有closing，就执行 async_cb，这个 async_cb 是？ 这个 async_cb 是 线程池里的 uv__work_done
    // 它是在 uv_async_init 里赋值的，uv_loop_init 里有调用 uv_async_init，但这个init属于 loop->wq_async
    // user 也可以给自己的 async handle 设置 async_cb，也就是 user 自己调用 uv_async_init。
    handle->async_cb(handle);
  }
}
