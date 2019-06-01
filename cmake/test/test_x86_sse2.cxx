#include <emmintrin.h>
#include <stdlib.h>
#include <signal.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	__m128i x = _mm_setzero_si128();
	x=_mm_add_epi64(x,x);
	return 0;
}
