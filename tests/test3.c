#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*
 * Copy very large tfs file to new filesystem file
 */
void random_string(char *buffer, size_t size) {
	int i;
	srand(time(0));

	for (i = 0; i < size - 1; i++) {
		buffer[i] = 65 + (rand() % 57);
	}

	buffer[i] = '\0';
}

int main() {

	char *path = "/f1";

	/* Create random large string */
	size_t size = 1024;
	char str[size];
	random_string(str, size);

	char buffer[size];

	/* Create and write str to new tfs file */
	tfs_init();
	int f = tfs_open(path, TFS_O_CREAT);
	tfs_write(f, str, strlen(str));
	tfs_close(f);

	/* Write from tfs file to filesystem file */
	assert(tfs_copy_to_external_fs(path, "write.txt") == 0);

	FILE *fp = fopen("write.txt", "r");

	/* Check if file was created */
	assert(fp != NULL);

	/* Check if contents are identical */
	fread(buffer, sizeof(char), strlen(str), fp);
	assert(strcmp(buffer, str) == 0);

	fclose(fp);
	tfs_close(f);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
