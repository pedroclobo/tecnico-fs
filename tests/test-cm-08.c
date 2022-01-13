#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 * Test file truncation.
 */

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

int main() {

	/* Create random large string */
	size_t size = 7500;
	char str[size];
	random_string(str, size);

	char buffer[size];
	memset(buffer, 0, sizeof(buffer));

	/* Create and write str to new tfs file */
	int f;
	assert(tfs_init() != -1);
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);
	assert(tfs_write(f, str, strlen(str)) == strlen(str));
	assert(tfs_close(f) != -1);

	assert((f = tfs_open(path, TFS_O_TRUNC)) != -1);
	assert(tfs_close(f) != -1);

	/* Write from tfs file to filesystem file */
	int res = tfs_copy_to_external_fs(path, "write.txt");
	assert(res == 0);

	FILE *fp = fopen("write.txt", "r");

	/* Check if file was created */
	assert(fp != NULL);

	/* Check if file was truncated */
	assert(strcmp(buffer, "") == 0);

	fclose(fp);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
