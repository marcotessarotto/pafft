/***
  this program works on Linux Debian 9.7 with PulseAudio
  and is derived from:
  https://freedesktop.org/software/pulseaudio/doxygen/parec-simple_8c-example.html

  original header from parec-simple.c:

  This file is part of PulseAudio.
  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.
  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include <math.h>

#include "kiss_fft.h"

/*
 * on Linux Debian 9.7:
 * sudo apt install libpulse-dev
 *
 * useful:
 * https://gavv.github.io/articles/pulseaudio-under-the-hood/
 * https://askubuntu.com/questions/22031/what-are-my-audio-devices
 */




#define AUDIO_BUF_SIZE (1024*2)

#define BUFSIZE (AUDIO_BUF_SIZE*8)

#define DEBUG 0

#define SAMPLING_FREQ 44100

#define E2	82.41
#define A2	110.00
#define D3	146.83
#define E3	164.81

#define A3	220.00

#define E4	329.63
#define G4	392.00
#define A4	440.00
#define B4	493.88
#define D5	587.33

static float frequencies [] = {E2, A2, D3, E3, A3, A4, -1};


double get_freq(int freq) {
	float * f = frequencies;

	double dist;
	if (*(f+1) != -1)
		dist = (*(f+1) - *f) / 2 * 0.8;
	if (dist > 20)
		dist = 20;

	while (*f != -1 && !(*f-dist <= freq && freq <= *f+dist)) {
		f++;
	}

	return *f;
}

int get_freq_len() {
	float * f = frequencies;
	int n = 0;
	while (*f++ != -1)
		n++;
	return n;
}

static void find_max_v2(kiss_fft_cpx * cx_out, int size, int sampling_freq) {


	float max = 0;
	int max_freq = 0;

	static int counter;
	double freq_segment = -1;
	double new_freq_segment = -1;

	int n = get_freq_len();

	//printf("%u\n", n);


	// skip frequency zero
	for (int i = 1; i < size / 2; i++) {

		int freq = sampling_freq * i / size;
		double val = fabs(cx_out[i].r);

		if (val >= max) {
			max = val;
			max_freq = freq;
		}

		new_freq_segment = get_freq(freq);

		if (freq_segment != -1 && new_freq_segment != -1 ) {
			max = val;
			max_freq = freq;
		} else if (freq_segment == -1 && new_freq_segment != -1 ) {
			max = val;
			max_freq = freq;
		} else if (freq_segment != -1 && new_freq_segment == -1 ) {
			//printf("%5d#  note=%3.0f  MAX_FREQ=%5d  VALUE=%.0f\n", counter, freq_segment, max_freq, log(max));

		} else {
		}
		freq_segment = new_freq_segment;


	}

	//printf("\033[%uA", get_freq_len()); // move cursor up n lines


	counter++;

}

static float find_max(kiss_fft_cpx * cx_out, int size, int sampling_freq) {

	float max = 0;
	int max_freq = 0;

	static int counter;

	// skip frequency zero
	for (int i = 1; i < size / 2; i++) {

		int freq = sampling_freq * i / size;

		if (DEBUG) printf("%4d  %5d Hz  %10.0f\n", i, freq, cx_out[i].r);

		if (cx_out[i].r >= max) {
			max = cx_out[i].r;
			max_freq = freq;
		}

	}

	// ANSI/VT100 Terminal Control Escape Sequences
	// http://www.termsys.demon.co.uk/vtansi.htm
	//printf("%5d#  \033[32m  MAX_FREQ=%5d \033[30m  VALUE=%.0f\r", counter++, max_freq, max);
	printf("%5d#    MAX_FREQ=%5d VALUE=%.0f\r", counter++, max_freq, max);

	return max_freq;
}


void populate_audio_buf(uint16_t * buf, int size, int freq, int sampling_frequency) {

	float w = 2 * M_PI  * freq;

//	printf("angular speed: %f\n", w);

	for (int i = 0; i < size; i++) {

		float radiant = w * i / sampling_frequency;

		float val = sinf(radiant);

		buf[i] = val * 100;

//		printf("%d  %f  %hd   \n", i, radiant,  buf[i]);

	}


}

int stop_execution = 0;

static void sigHandler(int sig) {

	printf("signal %d\n", sig);

	stop_execution = 1;

//
//	exit(EXIT_SUCCESS);

}


int main(int argc, char*argv[]) {
	/**
	 * very simple example which iterates these steps:
	 * from audio input, records BUFSIZE samples at sampling frequency SAMPLING_FREQ,
	 * then calculates fft of recorded samples,
	 * then looks up the frequency at which the spectrum has the absolute maximum (uses only the real component of spectrum).
	 *
	 */

	if (signal(SIGINT, sigHandler) == SIG_ERR) {
		perror("signal");
	}

    /* The sample type to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE, // Intel CPU architecture uses little endian memory representation
        .rate = SAMPLING_FREQ, // if SAMPLING_FREQ is 44100 Hz, then Nyquist frequency is 22050 Hz
        .channels = 1 // mono
    };

    pa_simple *s = NULL;

    int ret = 1;

    int error;


    kiss_fft_cfg cfg;
    kiss_fft_cpx * cx_in;
    kiss_fft_cpx * cx_out;

    /* init fft data structure */
	cfg = kiss_fft_alloc( BUFSIZE ,/*is_inverse_fft*/ 0 ,0,0 );

	cx_in = calloc(BUFSIZE, sizeof(kiss_fft_cpx));
	cx_out = calloc(BUFSIZE, sizeof(kiss_fft_cpx));

	if (DEBUG) {

	} else {
		/* Create the recording stream */
		if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
			fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
			goto finish;
		}
	}

    uint16_t buf[AUDIO_BUF_SIZE];

    printf("buf size: %ld bytes\n",sizeof(buf));

    while (!stop_execution) {

    	if (DEBUG) {
    		populate_audio_buf(buf, sizeof(buf) / sizeof(uint16_t), 440, SAMPLING_FREQ);
    	}
    	else {
			/* Record some data ... */
			if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
				fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
				goto finish;
			}
    	}

    	memcpy(cx_in, &cx_in[AUDIO_BUF_SIZE], (BUFSIZE - AUDIO_BUF_SIZE) * sizeof(kiss_fft_cpx));

    	// running average
    	// m(n) = m(n-1) + (a(n) - m(n-1)) / n
    	float incremental_average = 0;

    	for (int i = 0; i < AUDIO_BUF_SIZE; i++) {

    		int val = (int16_t)buf[i];
//


    		cx_in[i + BUFSIZE - AUDIO_BUF_SIZE].r = val;
    		//cx_in[i].i = 0.f;

    		incremental_average = incremental_average + (cx_in[i + BUFSIZE - AUDIO_BUF_SIZE].r - incremental_average) / (i+1);
    	}

    	printf("incremental_average = %f\n", incremental_average);

    	/* calculate fft */
    	kiss_fft( cfg , cx_in , cx_out );

    	find_max_v2(cx_out, BUFSIZE, SAMPLING_FREQ );


        if (DEBUG)
        	break;

    }

    ret = 0;
finish:

	printf("terminating...\n");

	free(cx_out);
	free(cx_in);

	kiss_fft_free(cfg);

	if (s)
        pa_simple_free(s);

    return ret;
}
