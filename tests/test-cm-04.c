#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

/*
 * Copy from invalid tfs file
 */
int main() {

	assert(tfs_init() != -1);

	/* Write from invalid tfs file */
	assert(tfs_copy_to_external_fs("f1", "write.txt") == -1);

	assert(tfs_destroy() != -1);

	printf("\033[0;32mSuccessful test.\n\033[0m");

	return 0;
}
