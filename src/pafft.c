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


#define BUFSIZE (1024*4)

#define DEBUG 0

#define SAMPLING_FREQ 44100



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

	printf("%5d#  MAX_FREQ=%5d  VALUE=%.0f\n", counter++, max_freq, max);

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

int main(int argc, char*argv[]) {
	/**
	 * very simple example which iterates these steps:
	 * from audio input, records BUFSIZE samples at sampling frequency SAMPLING_FREQ,
	 * then calculates fft of recorded samples,
	 * then looks up the frequency at which the spectrum has the absolute maximum (uses only the real component of spectrum).
	 *
	 */


    /* The sample type to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
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

	cx_in = malloc(sizeof(kiss_fft_cpx) * BUFSIZE);
	cx_out = malloc(sizeof(kiss_fft_cpx) * BUFSIZE);

	if (DEBUG) {

	} else {
		/* Create the recording stream */
		if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
			fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
			goto finish;
		}
	}

    uint16_t buf[BUFSIZE];

    printf("buf size: %ld bytes\n",sizeof(buf));

    for (;;) {

    	if (DEBUG) {
    		populate_audio_buf(buf, BUFSIZE, 440, SAMPLING_FREQ);
    	}
    	else {
			/* Record some data ... */
			if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
				fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
				goto finish;
			}
    	}

    	for (int i = 0; i < BUFSIZE; i++) {
    		cx_in[i].r = buf[i];
    		cx_in[i].i = 0.f;
    	}

    	/* calculate fft */
    	kiss_fft( cfg , cx_in , cx_out );

    	find_max(cx_out, BUFSIZE, SAMPLING_FREQ);


        if (DEBUG)
        	break;

    }

    ret = 0;
finish:

	free(cx_out);
	free(cx_in);

	kiss_fft_free(cfg);

	if (s)
        pa_simple_free(s);

    return ret;
}
