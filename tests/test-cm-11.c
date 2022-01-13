#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/*
 * Append concurrently to one tfs file, with the same file handle.
 * Expected output is one possible concatenation of the two strings.
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

void *append_to_tfs(void *arg) {
	reference *r = (reference*) arg;

	int f;
	assert((f = tfs_open(path, TFS_O_APPEND)) != -1);

	assert(tfs_write(f, r->str, strlen(r->str)) == strlen(r->str));

	assert(tfs_close(f) == 0);

	return NULL;
}

int main() {
	size_t size1 = 150000;
	size_t size2 = 122384;

	char str1[size1];
	random_string(str1, size1);

	char str2[size2];
	random_string(str2, size2);

	char buffer[size1+size2];
	memset(buffer, 0, sizeof(buffer));

	int f;
	assert(tfs_init() != -1);
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);
	assert(tfs_write(f, "A", 1) == strlen("A"));
	assert(tfs_close(f) != -1);

	reference r1 = { -1, str1, size1 };
	reference r2 = { -1, str2, size2 };

	pthread_t tid[2];
	assert(pthread_create(&tid[0], NULL, append_to_tfs, &r1) == 0);
	assert(pthread_create(&tid[1], NULL, append_to_tfs, &r2) == 0);
	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);

	assert(tfs_copy_to_external_fs(path, "write.txt") == 0);

	FILE *fp = fopen("write.txt", "r");
	assert(fp != NULL);

	/* Two concatenations possible */
	char opt1[1+size1+size2];
	strcpy(opt1, "A");
	strcat(opt1, r1.str);
	strcat(opt1, r2.str);

	char opt2[1+size1+size2];
	strcpy(opt2, "A");
	strcat(opt2, r2.str);
	strcat(opt2, r1.str);

	/* Check if contents are identical */
	assert(fread(buffer, sizeof(char), strlen("A") + strlen(r1.str) + strlen(r2.str), fp) == strlen("A") + strlen(r1.str) + strlen(r2.str));
	assert(strcmp(buffer, opt1) == 0 || strcmp(buffer, opt2) == 0);

	fclose(fp);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
