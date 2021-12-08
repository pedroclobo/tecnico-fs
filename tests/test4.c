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

	/* Create and write str to new tfs file */
	tfs_init();
	int f = tfs_open(path, TFS_O_CREAT);
	tfs_write(f, str, strlen(str));
	tfs_close(f);

	/* Create new file with random contents */
	FILE *fp = fopen("write.txt", "r");
	fprintf(fp, "Hello World!");

	/* Write from tfs file to filesystem file */
	assert(tfs_copy_to_external_fs(path, "write.txt") != -1);

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
