#define _GNU_SOURCE 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/limits.h>

#define WALK_FD_LIMIT			16
#define CGROUP_MOUNT_PATH		"/sys/fs/cgroup"
#define CGROUP_WORK_DIR			"/cpu/"
#define format_cgroup_path(buf, path) \
	snprintf(buf, sizeof(buf), "%s%s%s", CGROUP_MOUNT_PATH, \
		 CGROUP_WORK_DIR, path)

unsigned long long get_cgroup_id_user(const char *path)
{
	int dirfd, err, flags, mount_id, fhsize;
	union {
		unsigned long long cgid;
		unsigned char raw_bytes[8];
	} id;
	char cgroup_workdir[PATH_MAX + 1];
	struct file_handle *fhp, *fhp2;
	unsigned long long ret = 0;

	format_cgroup_path(cgroup_workdir, path);

	dirfd = AT_FDCWD;
	flags = 0;
	fhsize = sizeof(*fhp);
	fhp = calloc(1, fhsize);
	if (!fhp) {
		printf("calloc\n");
		return 0;
	}
	err = name_to_handle_at(dirfd, cgroup_workdir, fhp, &mount_id, flags);
	if (err >= 0 || fhp->handle_bytes != 8) {
		printf("name_to_handle_at\n");
		goto free_mem;
	}

	fhsize = sizeof(struct file_handle) + fhp->handle_bytes;
	fhp2 = realloc(fhp, fhsize);
	if (!fhp2) {
		printf("realloc\n");
		goto free_mem;
	}
	err = name_to_handle_at(dirfd, cgroup_workdir, fhp2, &mount_id, flags);
	fhp = fhp2;
	if (err < 0) {
		printf("name_to_handle_at\n");
		goto free_mem;
	}

	memcpy(id.raw_bytes, fhp->f_handle, 8);
	ret = id.cgid;

free_mem:
	free(fhp);
	return ret;
}

/*
 * example: ./get_cgroup_id_user "docker.slice"
 * */
#if 0
int main(int argc, char *argv[])
{
	unsigned long long cgrpid;

	cgrpid = get_cgroup_id_user(argv[1]);
	printf("cgrpid=%d\n", cgrpid);
}
#endif
