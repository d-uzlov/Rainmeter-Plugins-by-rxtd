// http://practicalcryptography.com/miscellaneous/machine-learning/implementing-dolph-chebyshev-window/

#include "cheby_win.h"

static const auto pi = std::acos(-1.0);

/**************************************************************************
This function computes the chebyshev polyomial T_n(x)
***************************************************************************/
double cheby_poly(int n, double x) {
	double res;
	if (fabs(x) <= 1) res = cos(n*acos(x));
	else              res = cosh(n*acosh(x));
	return res;
}

/***************************************************************************
 calculate a chebyshev window of size N, store coeffs in out as in Antoniou
-out should be array of size N
-atten is the required sidelobe attenuation (e.g. if you want -60dB atten, use '60')
***************************************************************************/
void cheby_win(float *out, int N, float atten) {
	int nn, i;
	double M, n, sum = 0, max = 0;
	double tg = pow(10, atten / 20);  /* 1/r term [2], 10^gamma [2] */
	double x0 = cosh((1.0 / (N - 1))*acosh(tg));
	M = (N - 1) / 2;
	if (N % 2 == 0) M = M + 0.5; /* handle even length windows */
	for (nn = 0; nn < (N / 2 + 1); nn++) {
		n = nn - M;
		sum = 0;
		for (i = 1; i <= M; i++) {
			sum += cheby_poly(N - 1, x0*cos(pi*i / N))*cos(2.0*n*pi*i / N);
		}
		out[nn] = tg + 2 * sum;
		out[N - nn - 1] = out[nn];
		if (out[nn] > max)max = out[nn];
	}
	for (nn = 0; nn < N; nn++) out[nn] /= max; /* normalise everything */
	return;
}
