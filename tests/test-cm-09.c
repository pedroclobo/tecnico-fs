#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 * Write twice to tfs file.
 * First, with large string and then with a smaller string.
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
	size_t size1 = 5000;
	char str1[size1];
	random_string(str1, size1);

	/* Create another random smaller string */
	size_t size2 = 2300;
	char str2[size2];
	random_string(str2, size2);

	char buffer[size1];
	memset(buffer, 0, sizeof(buffer));

	/* Create and write str1 to new tfs file */
	int f;
	assert(tfs_init() != -1);
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);
	assert(tfs_write(f, str1, strlen(str1)) == strlen(str1));
	assert(tfs_close(f) != -1);

	/* Open file again and write smaller str2 */
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);
	assert(tfs_write(f, str2, strlen(str2)) == strlen(str2));
	assert(tfs_close(f) != -1);

	/* Write from tfs file to filesystem file */
	assert(tfs_copy_to_external_fs(path, "write.txt") == 0);

	FILE *fp = fopen("write.txt", "r");

	/* Check if contents are identical */
	assert(fread(buffer, sizeof(char), strlen(str2), fp) == strlen(str2));
	assert(strcmp(buffer, str2) == 0);

	fclose(fp);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
