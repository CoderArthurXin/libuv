#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include "queue_work_tester.h"


uv_loop_t *loop;
uv_async_t async;

double percentage;

void fake_download(uv_work_t *req) {
	int size = *((int*)req->data);
	int downloaded = 0;
	while (downloaded < size) {
		percentage = downloaded * 100.0 / size;
		async.data = (void*)&percentage;
		uv_async_send(&async);

		std::this_thread::sleep_for(std::chrono::seconds(1));
		//sleep(1);
		downloaded += 300 % 1000; // can only download max 1000bytes/sec,
										   // but at least a 200;
	}
}

void after(uv_work_t *req, int status) {
	fprintf(stderr, "Download complete\n");
	uv_close((uv_handle_t*)&async, NULL);
}

void print_progress(uv_async_t *handle) {
	double percentage = *((double*)handle->data);
	fprintf(stderr, "Downloaded %.2f%%\n", percentage);
}

int queue_work_tester() {
	loop = uv_default_loop();

	uv_work_t req;
	int size = 10240;
	req.data = (void*)&size;

	uv_async_init(loop, &async, print_progress);
	uv_queue_work(loop, &req, fake_download, after);

	return uv_run(loop, UV_RUN_DEFAULT);
}
