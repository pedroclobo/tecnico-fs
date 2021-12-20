#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

/*
 * Copy tfs file contents to already existing filesystem file
 */
int main() {

	char *str = "AAA!";
	char *path = "/f1";
	char buffer[40];
	memset(buffer, 0, sizeof(buffer));

	/* Create and write str to new tfs file */
	int f;
	assert(tfs_init() != -1);
	assert((f = tfs_open(path, TFS_O_CREAT)) != -1);
	assert(tfs_write(f, str, strlen(str)) == strlen(str));
	assert(tfs_close(f) != -1);

	/* Create new file with random contents */
	FILE *fp = fopen("write.txt", "r");
	fprintf(fp, "Hello World!");

	/* Write from tfs file to filesystem file */
	assert(tfs_copy_to_external_fs(path, "write.txt") != -1);

	/* Check if file was created */
	assert(fp != NULL);

	/* Check if contents are identical */
	assert(fread(buffer, sizeof(char), strlen(str), fp) == strlen(str));
	assert(strcmp(buffer, str) == 0);

	fclose(fp);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
