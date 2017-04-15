#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <stdio.h>

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

struct note a_tone[] = {
	NOTE(C4, SIXTEENTH),
	{0, 0},
};

enum {
	SPEED_NORMAL = 4,
	SPEED_FAST = 2,
};

static void play_note(struct note *n)
{
	long int period_us = get_us(n->delta_a4 - 12);
	int ms = n->duration * 12;
	tone(period_us, ms);
}

static void play_score(struct note *score, int speed)
{
	struct note *n = score;
	while (n->duration) {
		long int period_us = get_us(n->delta_a4 - 12);
		int ms = n->duration * 12 * speed;
		tone(period_us, ms);
		ms *= 1500;
		if (0) udelay(ms / 1000000, ms % 1000000);
		n++;
	}
}

static int on;
static int saved_note;

static void play_tone(void)
{
	while (1) {
		if (on) {
			struct note n = NOTE(saved_note, SIXTEENTH);

			if (0) play_note(a_tone);
			play_note(&n);
		} else
			udelay(0, 10000);
	};
}

int start_tone()
{
	init_tones();

	if (init_drives())
		return EXIT_FAILURE;

	printf("Done resetting...\n");
	udelay(1, 0);
	printf("GO!\n");

	play_tone();

	return 0;
}


/* this function is run by the second thread */
void *tone_thread(void *arg)
{
	(void)arg;

	start_tone();

	/* the function must return something - NULL will do */
	return NULL;
}

#define MIDI_C4 60

static void log_note(int note)
{
	saved_note = note + (C4 - MIDI_C4);
}

static void handle_buf(const char *in, int len)
{
	if (in[0] & 0xf0 == 0xf0)
		return;

	if (len == 3) {
		if (in[2] == 0)
			on--;
		else {
			on++;
			log_note(in[1]);
		}
		printf("on = %d, note = %d\n", on, in[1]);
	}
}

static int process_midi(const char *dev)
{
	int i;
	char inpacket[4];

	// first open the sequencer device for reading.
	int seqfd = open(dev, O_RDONLY);
	if (seqfd == -1) {
		printf("Error: cannot open %s\n", dev);
		exit(1);
	} 

	// now just wait around for MIDI bytes to arrive and print them to screen.
	while (1) {
		int len = read(seqfd, &inpacket, sizeof(inpacket));
		if (len == -1) {
			perror("read");
			continue;
		}

		if (1)
			handle_buf(inpacket, len);
		else {
			printf("received MIDI bytes:");
			for (i = 0; i < len; i++)
				printf(" %02x", inpacket[i]);
			printf("\n");
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	pthread_t thread;
	const char *dev;

	if (argc < 2) {
		printf("Error: missing arg\n");
		return 1;
	}

	dev = argv[1];

	if (pthread_create(&thread, NULL, tone_thread, /*arg*/NULL)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	process_midi(dev);

	/* wait for the second thread to finish */
	if (pthread_join(thread, NULL)) {
		fprintf(stderr, "Error joining thread\n");
		return 1;
	}

	return 0;
}
