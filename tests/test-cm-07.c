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
		buffer[i] = 97 + (rand() % 25);
	}

	buffer[i] = '\0';
}

int main() {

	char *path = "/f1";

	/* Create random large string */
	size_t size1 = 6500;
	char str1[size1];
	random_string(str1, 5000);

	/* Create another random large string */
	size_t size2 = 1050;
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

	/* Append str2 to tfs file */
	assert((f = tfs_open(path, TFS_O_APPEND)) != -1);
	assert(tfs_write(f, str2, strlen(str2)) == strlen(str2));
	assert(tfs_close(f) != -1);

	/* Write from tfs file to filesystem file */
	int res = tfs_copy_to_external_fs(path, "write.txt");
	assert(res == 0);

	FILE *fp = fopen("write.txt", "r");

	/* Check if file was created */
	assert(fp != NULL);

	/* Concatenate strings */
	strcat(str1, str2);

	/* Check if contents are identical */
	assert(fread(buffer, sizeof(char), strlen(str1), fp) == strlen(str1));
	assert(strcmp(buffer, str1) == 0);

	fclose(fp);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
