#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <glob.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

#define GPIO_IDX_STEP	60
#define GPIO_IDX_DIR	50

#define FLOPPY_MAX_STEP	80

#define GPIO_SYSFS	"/sys/class/gpio"
#define GPIO_EXPORT	GPIO_SYSFS "/export"
#define GPIO_PATH	GPIO_SYSFS "/gpio%d"
#define GPIO_VALUE	GPIO_PATH "/value"
#define GPIO_DIRECTION	GPIO_PATH "/direction"

static int floppy_position;
static int floppy_direction;

static int dir_fd;
static int step_fd;

static int export_gpio(int idx)
{
	char str[80];
	int fd;
	ssize_t ret;
	struct stat sb;

	/* Already exported? */
	sprintf(str, GPIO_PATH, idx);
	if (!stat(str, &sb) == 0 || !S_ISDIR(sb.st_mode)) {
		/* Export the GPIO */
		fd = open(GPIO_EXPORT, O_WRONLY);
		if (fd < 0) {
			perror("open");
			return errno;
		}

		sprintf(str, "%d", idx);

		ret = write(fd, str, strlen(str));
		if (ret < 0) {
			perror("write to export");
			return errno;
		} else if (ret < strlen(str)) {
			printf("error: didn't write full length\n");
			return -1;
		}
		close(fd);
	}

	/* Set the GPIO direction */
	sprintf(str, GPIO_DIRECTION, idx);

	fd = open(str, O_WRONLY);
	if (fd < 0) {
		perror("open");
		return errno;
	}

	ret = write(fd, "out", strlen("out"));
	if (ret < 0) {
		perror("write");
		return errno;
	} else if (ret < strlen("out")) {
		printf("error: didn't write full length\n");
		return -1;
	}

	close(fd);

	return 0;
}

static int open_gpio(int idx)
{
	char str[80];
	int fd;

	sprintf(str, GPIO_VALUE, idx);
	fd = open(str, O_WRONLY);
	if (fd < 0) {
		perror("open");
		return errno;
	}
	return fd;
}

static int init_gpios(void)
{
	int ret;

	ret = export_gpio(GPIO_IDX_STEP);
	if (ret)
		return ret;

	ret = export_gpio(GPIO_IDX_DIR);
	if (ret)
		return ret;

	step_fd = open_gpio(GPIO_IDX_STEP);
	if (step_fd < 0)
		return step_fd;

	dir_fd = open_gpio(GPIO_IDX_DIR);
	if (dir_fd < 0)
		return dir_fd;

	return 0;
}

static int write_value(int fd, int val)
{
	char str[20];
	ssize_t ret;
	off_t off;

	off = lseek(fd, 0, SEEK_SET);
	if (off < 0) {
		perror("lseek");
		return off;
	}

	sprintf(str, "%d", val);
	ret = write(fd, str, strlen(str));
	if (ret < 0) {
		perror("write");
		return errno;
	}
	return 0;
}

static int set_direction(int dir)
{
	write_value(dir_fd, dir);
	floppy_direction = dir;

	return 0;
}

static int udelay(int sec, int us)
{
	struct timespec ts;
	int ret;

	ts.tv_sec = sec;
	ts.tv_nsec = 1000 * us;

	ret = nanosleep(&ts, NULL);
	if (ret < 0) {
		perror("nanosleep");
		return errno;
	}
	return 0;
}

static int reset_floppy_positions(void)
{
	int i;

	set_direction(0);

	for (i = 0; i < FLOPPY_MAX_STEP; i++) {
		write_value(step_fd, 0);
		write_value(step_fd, 1);
		udelay(0, 2000);
	}

	set_direction(1);

	return 0;
}

int take_step(void)
{
	write_value(step_fd, 0);
	write_value(step_fd, 1);
	floppy_position += floppy_direction * 2 - 1;
	if (floppy_direction == 0 && floppy_position == 0)
		set_direction(1);
	else if (floppy_direction == 1 && floppy_position == FLOPPY_MAX_STEP - 1)
		set_direction(0);

	return 0;
}

int init_drives(void)
{
	int ret;

	ret = init_gpios();
	if (ret)
		return ret;

	return reset_floppy_positions();
}
