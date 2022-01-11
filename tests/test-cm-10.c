#include "../fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
	int fhandle;
	char *str;
	size_t size;
} write_struct;

/*
 * Write concurrently to one tfs file.
 * Expected output is the concatenation of the two strings.
 */
void random_string(char *buffer, size_t size) {
	int i;
	srand(time(0));

	for (i = 0; i < size - 1; i++) {
		buffer[i] = 97 + (rand() % 25);
	}

	buffer[i] = '\0';
}

void *write_to_tfs(void *w_struct) {
	write_struct *w = (write_struct*) w_struct;

	assert(tfs_write(w->fhandle, w->str, strlen(w->str)) == strlen(w->str));

	return NULL;
}

int main() {

	char *path = "/f1";

	/* Create random large string */
	size_t size1 = 150000;
	char str1[size1];
	random_string(str1, size1);

	/* Create another random large string */
	size_t size2 = 122000;
	char str2[size2];
	random_string(str2, size2);

	char buffer[size1+size2];
	memset(buffer, 0, sizeof(buffer));

	/* Create and write str1 to new tfs file */
	int f;
	assert(tfs_init() != -1);
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);

	write_struct w1 = { f, str1, size1 };
	write_struct w2 = { f, str2, size2 };

	pthread_t tid[2];
	assert(pthread_create(&tid[0], NULL, write_to_tfs, (void*)&w1) == 0);
	assert(pthread_create(&tid[1], NULL, write_to_tfs, (void*)&w2) == 0);
	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);

	assert(tfs_close(f) != -1);

	/* Write from tfs file to filesystem file */
	assert(tfs_copy_to_external_fs(path, "write.txt") == 0);

	FILE *fp = fopen("write.txt", "r");

	/* Check if file was created */
	assert(fp != NULL);

	/* Two concatenations possible */
	char opt1[size1+size2];
	strcpy(opt1, str1);
	strcat(opt1, str2);
	char opt2[size1+size2];
	strcpy(opt2, str2);
	strcat(opt2, str1);

	/* Check if contents are identical */
	assert(fread(buffer, sizeof(char), strlen(str1) + strlen(str2), fp) == strlen(str1) + strlen(str2));
	assert(strcmp(buffer, opt1) == 0 || strcmp(buffer, opt2) == 0);

	fclose(fp);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
