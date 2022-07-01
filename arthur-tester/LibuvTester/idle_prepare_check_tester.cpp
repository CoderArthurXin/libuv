#include <assert.h>
#include "idle_prepare_check_tester.h"

static uv_idle_t idle_handle;
static uv_check_t check_handle;
static uv_timer_t timer_handle;
static uv_prepare_t prepare_handle;


static int idle_cb_called = 0;
static int check_cb_called = 0;
static int timer_cb_called = 0;
static int close_cb_called = 0;
static int prepare_cb_called = 0;


static void close_cb(uv_handle_t* handle) {
	close_cb_called++;
}


static void timer_cb(uv_timer_t* handle) {
	assert(handle == &timer_handle);

	uv_close((uv_handle_t*)&idle_handle, close_cb);
	uv_close((uv_handle_t*)&check_handle, close_cb);
	uv_close((uv_handle_t*)&timer_handle, close_cb);
	uv_close((uv_handle_t*)&prepare_handle, close_cb);

	timer_cb_called++;
	fprintf(stderr, "timer_cb %d\n", timer_cb_called);
	fflush(stderr);
}

static void prepare_cb(uv_prepare_t* handle) {
	assert(handle == &prepare_handle);

	prepare_cb_called++;
	fprintf(stderr, "prepare_cb %d\n", prepare_cb_called);
	fflush(stderr);
}

static void idle_cb(uv_idle_t* handle) {
	assert(handle == &idle_handle);

	idle_cb_called++;
	fprintf(stderr, "idle_cb %d\n", idle_cb_called);
	fflush(stderr);
}


static void check_cb(uv_check_t* handle) {
	assert(handle == &check_handle);

	check_cb_called++;
	fprintf(stderr, "check_cb %d\n", check_cb_called);
	fflush(stderr);
}


void idle_prepare_check_tester() {
	int r;

	r = uv_prepare_init(uv_default_loop(), &prepare_handle);
	assert(r == 0);
	r = uv_prepare_start(&prepare_handle, prepare_cb);

	r = uv_idle_init(uv_default_loop(), &idle_handle);
	assert(r == 0);
	r = uv_idle_start(&idle_handle, idle_cb);
	assert(r == 0);

	r = uv_check_init(uv_default_loop(), &check_handle);
	assert(r == 0);
	r = uv_check_start(&check_handle, check_cb);
	assert(r == 0);

	r = uv_timer_init(uv_default_loop(), &timer_handle);
	assert(r == 0);
	r = uv_timer_start(&timer_handle, timer_cb, 50, 0);
	assert(r == 0);

	r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	assert(r == 0);

	assert(idle_cb_called > 0);
	assert(timer_cb_called == 1);
	assert(close_cb_called == 4);
}