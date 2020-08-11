#pragma once

// http://practicalcryptography.com/miscellaneous/machine-learning/implementing-dolph-chebyshev-window/

/***************************************************************************
 calculate a chebyshev window of size N, store coeffs in out as in Antoniou
-out should be array of size N
-atten is the required sidelobe attenuation (e.g. if you want -60dB atten, use '60')
***************************************************************************/
void cheby_win(float* out, int N, float atten);
