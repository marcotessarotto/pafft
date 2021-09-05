#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>


#define AUDIO_BUF_SIZE 256

#define BUFSIZE (AUDIO_BUF_SIZE*4)

#define DEBUG 0

#define SAMPLING_FREQ 44100



int main(int argc, char*argv[]) {

    /* The sample type to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE, // Intel CPU architecture uses little endian memory representation
        .rate = SAMPLING_FREQ, // if SAMPLING_FREQ is 44100 Hz, then Nyquist frequency is 22050 Hz
        .channels = 2 // stereo
    };

    pa_simple *s_rec = NULL;
    pa_simple *s_play = NULL;

    int ret = 1;

    int error;

	/* Create the recording stream */
	if (!(s_rec = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish;
	}

    /* Create a new playback stream */
    if (!(s_play = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    uint8_t buf[BUFSIZE];

    for (;;) {

#if 1
        pa_usec_t latency;

        if ((latency = pa_simple_get_latency(s_rec, &error)) == (pa_usec_t) -1) {
            fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        fprintf(stderr, "%0.0f usec    \r", (float)latency);
#endif

        /* Record some data ... */
        if (pa_simple_read(s_rec, buf, sizeof(buf), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        /* ... and play it */
        if (pa_simple_write(s_play, buf, (size_t) sizeof(buf), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }

    }

    ret = 0;

    finish:

	if (s_rec)
		pa_simple_free(s_rec);

    if (s_play)
        pa_simple_free(s_play);


	return ret;
}
