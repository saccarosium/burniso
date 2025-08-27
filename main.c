#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#define MB(x) (x * 1024 * 1024)

#define FAILED_WRITE 134
#define DISK_TOO_SMALL 135

static bool processing = true;

int32_t validate_image(int32_t *image_fd, const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return errno;

	*image_fd = fd;
	return 0;
}

int32_t validate_disk(int32_t *disk_fd, const char *path)
{
	int fd = open(path, O_WRONLY);
	if (fd == -1)
		return errno;

	*disk_fd = fd;
	return 0;
}

void display_burn_error(int32_t error, const char *image_path, const char *disk_path)
{
	switch (error) {
	case FAILED_WRITE:
		fprintf(stderr, "Error while writing to %s. Try to rerun the command\n", disk_path);
		break;
	case DISK_TOO_SMALL:
		fprintf(stderr, "%s is too big for %s\n", image_path, disk_path);
		break;
	default:
		fprintf(stderr, "Error: %s\n", strerror(error));
		break;
	}
}

int32_t burn_to_disk(int32_t image_fd, int32_t disk_fd)
{
	int32_t err = 0;

	struct stat image_stats;
	err = fstat(image_fd, &image_stats);
	if (err == -1)
		return errno;

	uint64_t disk_size = 0;
	err = ioctl(disk_fd, BLKGETSIZE64, &disk_size);
	if (err == -1)
		return errno;

	if (disk_size < image_stats.st_size)
		return DISK_TOO_SMALL;

	ssize_t nbytes = sendfile(disk_fd, image_fd, NULL, image_stats.st_size);
	if (nbytes == -1)
		return errno;

	fdatasync(disk_fd);

	if (nbytes != image_stats.st_size)
		return FAILED_WRITE;

	return 0;
}

void *progress_report(void* args)
{
	int32_t seconds = 0;

	while (processing) {
		int32_t mins = seconds / 60;
		int32_t secs = seconds % 60;

		printf("\rBurning ISO... %d:%02d", mins, secs);
		fflush(stdout);
		sleep(1);

		++seconds;
	}

	printf("\n");

	return NULL;
}

int main(int argc, char **argv)
{
	// TODO(sacca): add flag parsing
	if (argc < 3) {
		fprintf(stderr, "Not enogh arguments were passed\n");
		exit(EXIT_SUCCESS);
		return 1;
	}

	const char *image_path = argv[1];
	const char *disk_path = argv[2];

	int32_t err = 0;

	int32_t image_fd = 0;
	err = validate_image(&image_fd, image_path);
	if (err != 0) {
		fprintf(stderr, "Error image (%s): %s\n", image_path, strerror(err));
		return err;
	}

	int32_t disk_fd = 0;
	err = validate_disk(&disk_fd, disk_path);
	if (err != 0) {
		fprintf(stderr, "Error disk (%s): %s\n", disk_path, strerror(err));
		close(image_fd);
		return err;
	}

	pthread_t td = 0;
	err = pthread_create(&td, NULL, progress_report, NULL);
	if (err != 0)
		printf("Burning ISO...\n");

	printf("The syscall won't stop until it finishes. You can't use ctrl+c or ctrl+\\\n");
	int32_t code = burn_to_disk(image_fd, disk_fd);
	if (code != 0)
		display_burn_error(code, image_path, disk_path);

	close(image_fd);
	close(disk_fd);

	processing = false;
	pthread_join(td, NULL);

	return code;
}
