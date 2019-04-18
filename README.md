# pafft
example: Pulse Audio recording and FFT elaboration

very simple example which iterates these steps:

from audio input, records BUFSIZE samples at sampling frequency SAMPLING_FREQ,

then calculates FFT of recorded samples,

then looks up the frequency at which the spectrum has the absolute maximum (uses only the real component of spectrum).

