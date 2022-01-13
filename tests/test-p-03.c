#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/*
 * Read concurrently to one tfs file, from the same file handle.
 */

typedef struct {
	int f;
	char *str;
	size_t size;
} reference;

int random_seed = 1;

char *path = "/f1";

void random_string(char *buffer, size_t size) {
	int i;
	srand(random_seed++);

	for (i = 0; i < size - 1; i++) {
		buffer[i] = 97 + (rand() % 25);
	}

	buffer[i] = '\0';
}

void *read_from_tfs(void *arg) {
	reference *r = (reference*) arg;

	char buffer[r->size];
	memset(buffer, 0, sizeof(buffer));

	ssize_t read = tfs_read(r->f, buffer, strlen(r->str));
	assert(read != -1);

	assert(read == strlen(r->str) || read == 0);

	if (read == strlen(r->str)) {
		assert(strcmp(buffer, r->str) == 0);
	} else if (read == 0) {
		assert(strcmp(buffer, "") == 0);
	}

	return NULL;
}

int main() {
	size_t size = 272385;
	char str[size];
	random_string(str, size);

	int f;
	assert(tfs_init() != -1);
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);
	assert(tfs_write(f, str, strlen(str)) == strlen(str));
	assert(tfs_close(f) == 0);

	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);

	reference r = { f, str, size };

	pthread_t tid[2];
	assert(pthread_create(&tid[0], NULL, read_from_tfs, &r) == 0);
	assert(pthread_create(&tid[1], NULL, read_from_tfs, &r) == 0);

	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);

	assert(tfs_close(f) == 0);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
