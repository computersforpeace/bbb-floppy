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
#include <stdint.h>

#include "move-ctrl.h"

#include <list>

static int debug = 0;

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

#define MIN_DELTA_A4	-72
#define MAX_DELTA_A4	72

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

int tone(int floppy, int tone_us, int ms)
{
	int cycles = 1000 * ms / tone_us;
	int i, ret;
	struct timespec ts;

	if (cycles == 0)
		cycles = 1;

	if (debug)
		printf("cycles: %d\n", cycles);

	ret = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (ret < 0) {
		perror("clock_gettime");
		return errno;
	}

	for (i = 0; i < cycles; i++) {
		take_step(floppy);
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

enum {
	SPEED_NORMAL = 4,
	SPEED_FAST = 2,
};

static void play_note(int floppy, struct note *n)
{
	long int period_us = get_us(n->delta_a4 - 12);
	int ms = n->duration * 12;

	if (debug)
		printf("period us: %ld\n", period_us);

	tone(floppy, period_us, ms);
}

#define MAX_DRIVES 2
static int max_drives = MAX_DRIVES;

static int last;
static int on[MAX_DRIVES];
static int saved_note[MAX_DRIVES];

#define MIDI_C4 (60 + 12)

static int midi_to_note(int midi)
{
	return midi + (C4 - MIDI_C4);
}

static void play_tone(int idx)
{
	while (1) {
		if (on[idx]) {
			struct note n = NOTE(midi_to_note(saved_note[idx]), SIXTEENTH);

			play_note(idx, &n);
		} else
			udelay(0, 10000);
	};
}

static int drive_init(void)
{
	init_tones();

	if (init_drives())
		return EXIT_FAILURE;

	printf("Done resetting...\n");
	udelay(1, 0);
	printf("GO!\n");

	return 0;
}

static int start_tone(int idx)
{
	play_tone(idx);

	return 0;
}


/* this function is run by the second thread */
void *tone_thread(void *arg)
{
	int idx = (int)(uintptr_t)arg;

	start_tone(idx);

	/* the function must return something - NULL will do */
	return NULL;
}

static int handle_buf(std::list<char> &q)
{
	char c = q.front();
	char d, e;
	int i;

	q.pop_front();

	if ((c & 0xf0) == 0xf0)
		return 1;

	if (c == 0x90) {
		if (q.size() < 2) {
			printf("Warning: not enough bytes: %d\n", q.size());
			q.push_front(c);
			return 0;
		}
		d = q.front();
		q.pop_front();
		e = q.front();
		q.pop_front();
		if (e == 0) {
			for (i = 0; i < max_drives; i++) {
				if (on[i] && saved_note[i] == d) {
					on[i] = 0;
					//last = !i;
					break;
				}
			}
		} else {
			last = !last;
			if (d > 55) {
				for (i = max_drives - 1; i >= 0; i--) {
					if (!on[i] || i == 0) {
						on[i] = 1;
						saved_note[i] = d;
						break;
					}
				}
			} else {
				for (i = 0; i < max_drives; i++) {
					if (!on[i] || i == 1) {
						on[i] = 1;
						saved_note[i] = d;
						break;
					}
				}
			}
			/*
			for (i = max_drives - 1; i >= 0; i--) {
				if (!on[i] || i == 0 || d < 50) {
					on[i] = 1;
					saved_note[i] = d;
					break;
				}
			}
			*/
			//on[last] = 1;
			//saved_note[last] = d;
		}
		for (i = 0; i < max_drives; i++) {
			printf("on[%d]: %d, note[%d]: %d ", i, on[i], i, saved_note[i]);
		}
		printf("\n");
		//printf("on = %d, note = %d, floppy = %d\n", on, d, the_floppy);
		return 3;
	}

	/* Unknown; skip */
	printf("Warning: skipping unknown: %#x\n", c);
	return 1;
}

static int process_midi(const char *dev)
{
	int i;
	char inpacket[4];
	int ret;
	std::list<char> q;

	// first open the sequencer device for reading.
	int seqfd = open(dev, O_RDONLY);
	if (seqfd == -1) {
		perror("open");
		fprintf(stderr, "Error: cannot open %s\n", dev);
		return 1;
	} 

	// now just wait around for MIDI bytes to arrive
	while (1) {
		int len = read(seqfd, &inpacket, sizeof(inpacket));
		if (len == -1) {
			perror("read");
			continue;
		}
		for (i = 0; i < len; i++)
			q.push_back(inpacket[i]);

		if (debug) {
			printf("received MIDI bytes:");
			for (i = 0; i < len; i++)
				printf(" %02x", inpacket[i]);
			printf("\n");
		}

		do {
			ret = handle_buf(q);
		} while (ret > 0 && !q.empty());
	}

	return 0;
}

int main(int argc, char **argv)
{
	pthread_t thread[2];
	const char *dev;
	int i;

	if (argc < 2) {
		printf("Error: missing arg\n");
		return 1;
	}

	dev = argv[1];

	drive_init();

	for (i = 0; i < 2; i++) {
		if (pthread_create(&thread[i], NULL, tone_thread, (void *)i)) {
			fprintf(stderr, "Error creating thread %d\n", i);
			return 1;
		}
	}

	if (process_midi(dev)) {
		fprintf(stderr, "Exiting...\n");
		return 1;
	}

	/* wait for the threads to finish */
	for (i = 0; i < 2; i++) {
		if (pthread_join(thread[i], NULL)) {
			fprintf(stderr, "Error joining thread\n");
			return 1;
		}
	}

	return 0;
}
