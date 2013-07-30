#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "move-ctrl.h"

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
static long int tone_period_us[MAX_DELTA_A4 - MIN_DELTA_A4 + 1];

static void init_tones(void)
{
	int i;
	for (i = MIN_DELTA_A4; i <= MAX_DELTA_A4; i++)
		tone_period_us[i - MIN_DELTA_A4] = get_period_us(i);
}

static void add_to_timespec(struct timespec *ts, int us)
{
	ts->tv_nsec += us * 1000;
	ts->tv_sec += ts->tv_nsec / 1000000000;
	ts->tv_nsec %= 1000000000;
}

int tone(int tone_us, int ms)
{
	int cycles = 1000 * ms / tone_us;
	int i, ret;
	struct timespec ts;

	ret = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (ret < 0) {
		perror("clock_gettime");
		return errno;
	}

	for (i = 0; i < cycles; i++) {
		take_step();
		add_to_timespec(&ts, tone_us);
		ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
		if (ret < 0) {
			perror("clock_nanosleep");
			return errno;
		}
	}
	return 0;
}

#define get_us(delta)	(tone_period_us[(delta) - MIN_DELTA_A4])

#define G3	-5
#define A3	-3
#define B3	-1
#define C4	0
#define D4	2
#define E4	4
#define F4	5
#define G4	7
#define A4	9
#define B4	11
#define C5	12
#define D5	14
#define E5	16
#define F5	17
#define G5	19

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

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

struct note mario1[] = {
	NOTE(4, SIXTEENTH),
	NOTE(4, EIGHTH),
	NOTE(4, EIGHTH),
	NOTE(0, SIXTEENTH),
	NOTE(4, EIGHTH),

	NOTE(7, QUARTER),
	NOTE(-5, QUARTER),
};

struct note mario2[] = {
	NOTE(0, DOT(EIGHTH)),
	NOTE(-5, DOT(EIGHTH)),

	NOTE(-8, QUARTER + SIXTEENTH),
	NOTE(-3, SIXTEENTH),
	NOTE(-1, EIGHTH),
	NOTE(-2, SIXTEENTH),
	NOTE(-3, EIGHTH),

	NOTE(-5, SIXTEENTH),
	NOTE(4, EIGHTH),
	NOTE(7, SIXTEENTH),
	NOTE(9, EIGHTH),
	NOTE(5, SIXTEENTH),
	NOTE(7, EIGHTH),
	NOTE(4, EIGHTH),
	NOTE(0, SIXTEENTH),
	NOTE(2, SIXTEENTH),
	NOTE(-1, DOT(EIGHTH)),
};

struct note mario3[] = {
	NOTE(0, EIGHTH),
	NOTE(7, SIXTEENTH),
	NOTE(6, SIXTEENTH),
	NOTE(5, SIXTEENTH),
	NOTE(2, SIXTEENTH),
	NOTE(3, SIXTEENTH),
	NOTE(4, EIGHTH),
	NOTE(-5, SIXTEENTH),
	NOTE(-3, SIXTEENTH),
	NOTE(0, EIGHTH),
	NOTE(-3, SIXTEENTH),
	NOTE(0, SIXTEENTH),
	NOTE(2, EIGHTH),

	NOTE(7, SIXTEENTH),
	NOTE(6, SIXTEENTH),
	NOTE(5, SIXTEENTH),
	NOTE(2, SIXTEENTH),
	NOTE(3, SIXTEENTH),
	NOTE(4, EIGHTH),
	NOTE(12, EIGHTH),
	NOTE(12, SIXTEENTH),
	NOTE(12, EIGHTH),
	NOTE(12, EIGHTH),
};

struct note looney_tunes[] = {
	NOTE(E4, DOT(EIGHTH)),
	NOTE(D4, SIXTEENTH),

	NOTE(C4, EIGHTH),
	NOTE(D4, EIGHTH),

	NOTE(E4, EIGHTH),
	NOTE(E4, EIGHTH),

	NOTE(E4, EIGHTH),
	NOTE(C4, EIGHTH),

	NOTE(D4, EIGHTH),
	NOTE(D4, EIGHTH),

	NOTE(D4, EIGHTH),
	NOTE(D4, EIGHTH + 4 * EIGHTH),


	NOTE(D4, DOT(EIGHTH)),
	NOTE(C4, SIXTEENTH),

	NOTE(B3, EIGHTH),
	NOTE(C4, EIGHTH),

	NOTE(D4, EIGHTH),
	NOTE(D4, EIGHTH),

	NOTE(D4, EIGHTH),
	NOTE(B3, EIGHTH),

	NOTE(C4, EIGHTH),
	NOTE(C4, EIGHTH),

	NOTE(C4, EIGHTH),
	NOTE(C4, EIGHTH + 4 * EIGHTH),


	NOTE(G3, QUARTER),

	NOTE(A3, QUARTER),

	NOTE(G3, QUARTER),

	NOTE(A3, QUARTER),

	NOTE(G3, EIGHTH),
	NOTE(D4, EIGHTH),

	NOTE(D4, EIGHTH),
	NOTE(D4, EIGHTH + 4 * EIGHTH),


	NOTE(G3, QUARTER),

	NOTE(A3, QUARTER),

	NOTE(G3, QUARTER),

	NOTE(A3, QUARTER),

	NOTE(G3, EIGHTH),
	NOTE(C4, EIGHTH),

	NOTE(C4, EIGHTH),
	NOTE(C4, EIGHTH + 4 * EIGHTH),

	NOTE(G4, EIGHTH),
	NOTE(F4, EIGHTH),
	NOTE(E4, EIGHTH),
	NOTE(D4, EIGHTH),
	NOTE(C4, SIXTEENTH),

	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),
	NOTE(D4, SIXTEENTH),
	NOTE(C4, SIXTEENTH),

	NOTE(C4, WHOLE),
};

enum {
	SPEED_NORMAL = 4,
	SPEED_FAST = 2,
};

static void play_score(struct note *score, int len, int speed)
{
	int i;
	for (i = 0; i < len; i++) {
		struct note *n = &score[i];
		long int period_us = get_us(n->delta_a4 - 12);
		int ms = n->duration * 12 * speed;
		tone(period_us, ms);
		ms *= 1500;
		udelay(ms / 1000000, ms % 1000000);
	}
}

static void play_mario(void)
{
	int speed = SPEED_NORMAL;

	play_score(mario1, ARRAY_SIZE(mario1), speed);
	play_score(mario2, ARRAY_SIZE(mario2), speed);
	play_score(mario2, ARRAY_SIZE(mario2), speed);
	play_score(mario3, ARRAY_SIZE(mario3), speed);
	play_score(mario3, ARRAY_SIZE(mario3), speed);
}

static void play_looney_tunes(void)
{
	play_score(looney_tunes, ARRAY_SIZE(looney_tunes), SPEED_FAST);
}

int main(int argc, char **argv)
{
	init_tones();

	if (init_drives())
		return EXIT_FAILURE;

	printf("Done resetting...\n");
	udelay(1, 0);
	printf("GO!\n");

	play_looney_tunes();

	play_mario();

	return 0;
}
