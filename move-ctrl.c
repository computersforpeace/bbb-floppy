#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <glob.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "move-ctrl.h"

#define ARRAY_SIZE(x) ((int)(sizeof((x)) / sizeof((x)[0])))

//#define GPIO_IDX_STEP	60
#define GPIO_IDX_STEP	51
//#define GPIO_IDX_DIR	50
#define GPIO_IDX_DIR	48

struct floppy {
	int step_gpio;
	int dir_gpio;

	int step_fd;
	int dir_fd;

	int position;
	int direction;
};

struct floppy floppies[] = {
	{
		.step_gpio = 60,
		.dir_gpio = 50,
	},
	{
		.step_gpio = 51,
		.dir_gpio = 48,
	},
};

#define FLOPPY_MAX_STEP	80

#define GPIO_SYSFS	"/sys/class/gpio"
#define GPIO_EXPORT	GPIO_SYSFS "/export"
#define GPIO_PATH	GPIO_SYSFS "/gpio%d"
#define GPIO_VALUE	GPIO_PATH "/value"
#define GPIO_DIRECTION	GPIO_PATH "/direction"

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
		} else if ((size_t)ret < strlen(str)) {
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
	} else if ((size_t)ret < strlen("out")) {
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

static int init_gpio(struct floppy *f)
{
	int ret;

	ret = export_gpio(f->step_gpio);
	if (ret)
		return ret;

	ret = export_gpio(f->dir_gpio);
	if (ret)
		return ret;

	f->step_fd = open_gpio(f->step_gpio);
	if (f->step_fd < 0)
		return f->step_fd;

	f->dir_fd = open_gpio(f->dir_gpio);
	if (f->dir_fd < 0)
		return f->dir_fd;

	return 0;
}

static int init_gpios(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(floppies); i++) {
		ret = init_gpio(&floppies[i]);
		if (ret)
			return ret;
	}

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

static int set_direction(struct floppy *f, int dir)
{
	write_value(f->dir_fd, dir);
	f->direction = dir;

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

static int reset_floppy_position(struct floppy *f)
{
	int i;

	set_direction(f, 0);

	for (i = 0; i < FLOPPY_MAX_STEP; i++) {
		write_value(f->step_fd, 0);
		write_value(f->step_fd, 1);
		udelay(0, 2000);
	}

	set_direction(f, 1);

	return 0;
}

static int reset_floppy_positions(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(floppies); i++) {
		ret = reset_floppy_position(&floppies[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int _take_step(struct floppy *f)
{
	write_value(f->step_fd, 0);
	write_value(f->step_fd, 1);
	f->position += f->direction * 2 - 1;
	if (f->direction == 0 && f->position == 0)
		set_direction(f, 1);
	else if (f->direction == 1 && f->position == FLOPPY_MAX_STEP - 1)
		set_direction(f, 0);

	return 0;
}

int take_step(int idx)
{
	if (idx < 0 || idx > ARRAY_SIZE(floppies)) {
		fprintf(stderr, "Invalid floppy: %d\n", idx);
		return -1;
	}

	return _take_step(&floppies[idx]);
}

int get_num_floppies(void)
{
	return ARRAY_SIZE(floppies);
}

int init_drives(void)
{
	int ret;

	ret = init_gpios();
	if (ret)
		return ret;

	return reset_floppy_positions();
}
