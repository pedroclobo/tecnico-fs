#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

/*
 * Open two tfs files, concurrently.
 * They must have distinct file handles.
 */

char *path = "/f1";

void *open_from_tfs(void *arg) {
	int *f = (int*) malloc(sizeof(int));

	*f = tfs_open(path, TFS_O_CREAT);
	assert(*f != -1);

	return f;
}

int main() {
	assert(tfs_init() != -1);

	void *p1;
	void *p2;

	pthread_t tid[2];
	assert(pthread_create(&tid[0], NULL, open_from_tfs, NULL) == 0);
	assert(pthread_create(&tid[1], NULL, open_from_tfs, NULL) == 0);
	pthread_join(tid[0], &p1);
	pthread_join(tid[1], &p2);

	int *f1 = (int*) p1;
	int *f2 = (int*) p2;

	tfs_close(*f1);
	tfs_close(*f2);

	assert(*f1 != *f2);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	free(f1);
	free(f2);

	return 0;
}
