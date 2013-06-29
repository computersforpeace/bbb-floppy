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
	char str[80];
	int fd, ret;

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
}

int udelay(int sec, int us)
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

int reset_floppy_positions(void)
{
	int i;

	set_direction(0);

	for (i = 0; i < FLOPPY_MAX_STEP; i++) {
		write_value(step_fd, 0);
		write_value(step_fd, 1);
		udelay(0, 2000);
	}

	set_direction(1);
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
}

static double get_frequency(double delta_a4)
{
	return 440.0 * pow(2, (double)delta_a4 / 12.0);
}

static long int get_period_us(double delta_a4)
{
	return lround(1000000.0 / get_frequency(delta_a4));
}

#define MIN_DELTA_A4	-24
#define MAX_DELTA_A4	24

/* Period of tones, given in microseconds */
static long int tone_period_us[MIN_DELTA_A4 + MAX_DELTA_A4 + 1];

static void init_tones(void)
{
	int i;
	for (i = MIN_DELTA_A4; i <= MAX_DELTA_A4; i++)
		tone_period_us[i - MIN_DELTA_A4] = get_period_us(i);
}

int tone(int tone_us, int ms)
{
	int cycles = 1000 * ms / tone_us;
	int i;

	for (i = 0; i < cycles; i++) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		take_step();
		ts.tv_nsec += tone_us * 1000;
		ts.tv_sec += ts.tv_nsec / 1000000000;
		ts.tv_nsec %= 1000000000;
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
	}
	return 0;
}

#define get_us(delta)	(tone_period_us[(delta) - MIN_DELTA_A4])

#define G3	get_us(-14)
#define A3	get_us(-12)
#define B3	get_us(-10)
#define C4	get_us(-9)
#define D4	get_us(-7)
#define E4	get_us(-5)
#define F4	get_us(-4)
#define G4	get_us(-2)
#define A4	get_us(0)
#define B4	get_us(2)
#define C5	get_us(3)
#define D5	get_us(5)
#define E5	get_us(7)
#define F5	get_us(8)
#define G5	get_us(10)

void looney_tunes()
{
	int i;

	tone(E4, 300);
	tone(D4, 100);

	tone(C4, 200);
	tone(D4, 200);

	tone(E4, 200);
	tone(E4, 200);

	tone(E4, 200);
	tone(C4, 200);

	tone(D4, 200);
	tone(D4, 200);

	tone(D4, 200);
	tone(D4, 200 + 4 * 200);


	tone(D4, 300);
	tone(C4, 100);

	tone(B3, 200);
	tone(C4, 200);

	tone(D4, 200);
	tone(D4, 200);

	tone(D4, 200);
	tone(B3, 200);

	tone(C4, 200);
	tone(C4, 200);

	tone(C4, 200);
	tone(C4, 200 + 4 * 200);


	tone(G3, 400);

	tone(A3, 400);

	tone(G3, 400);

	tone(A3, 400);

	tone(G3, 200);
	tone(D4, 200);

	tone(D4, 200);
	tone(D4, 200 + 4 * 200);


	tone(G3, 400);

	tone(A3, 400);

	tone(G3, 400);

	tone(A3, 400);

	tone(G3, 200);
	tone(C4, 200);

	tone(C4, 200);
	tone(C4, 200 + 4 * 200);

	tone(G4, 200);
	tone(F4, 200);
	tone(E4, 200);
	tone(D4, 200);
	tone(C4, 50);

	for (i = 0; i < 8; i++) {
		tone(D4, 50);
		tone(C4, 50);
	}
	tone(C4, 1000);
}

struct note {
	int delta_a4;
	int duration;
};

#define SIXTEENTH	1
#define EIGHTH		2
#define QUARTER		4
#define HALF		8
#define WHOLE		16

#define DOT(note) ((note) * 3 / 2)

#define NOTE(_delta_a4, _duration) { \
	.delta_a4	= (_delta_a4), \
	.duration	= (_duration), \
}

struct note mario[] = {
	NOTE(4, SIXTEENTH),
	NOTE(4, EIGHTH),
	NOTE(4, EIGHTH),
	NOTE(0, SIXTEENTH),
	NOTE(4, EIGHTH),

	NOTE(7, QUARTER),
	NOTE(-5, WHOLE),

	NOTE(-5, SIXTEENTH),
	NOTE(-3, SIXTEENTH),
	NOTE(-1, SIXTEENTH),
	NOTE(-5, SIXTEENTH),
};

static void play_mario(void)
{
	int i;
	for (i = 0; i < sizeof(mario) / sizeof(*mario); i++) {
		struct note *n = &mario[i];
		long int period_us = get_us(n->delta_a4 - 17);
		int ms = n->duration * 50;
		printf("us: %ld, dur: %d\n", period_us, ms);
		tone(period_us, ms);
		ms *= 1500;
		udelay(ms / 1000000, ms % 1000000);
	}
}

int main(int argc, char **argv)
{
	int i;

	init_tones();

	if (init_gpios())
		return EXIT_FAILURE;

	reset_floppy_positions();

	printf("Done resetting...\n");
	udelay(1, 0);
	printf("GO!\n");

	play_mario();

	for (i = 0; i < 2; i++)
		looney_tunes();

	for (i = 0; i < 5; i++) {
		tone(A4, 100);
		tone(B4, 100);
		tone(C5, 100);
		tone(B4, 100);
	}
	tone(A4, 1000);
	return 0;
}
