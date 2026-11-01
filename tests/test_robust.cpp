#include "../Source/FormantCore.h"
#include <cstdio>
#include <random>
using namespace formantflex;
int main(){
  for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
    FormantShifter s; s.prepare(sr, FormantShifter::chooseFftSize(sr)); s.reset();
    std::mt19937 r(2); std::normal_distribution<float> n(0, 0.2f);
    double peak=0; int len=(int)sr*3;
    for (int i=0;i<len;++i){
      if (i % 4000 == 0) { s.setFormantSemitones((float)((i/4000)%25 - 12)); s.setDetail(((i/4000)%5)/4.0f); }
      float x = (i > len/3 && i < len/2) ? 0.0f : n(r);   // includes a silent gap
      if (i == len/4) x = 1.0f;                            // spike
      float d, y = s.processSample(x, d);
      if (!std::isfinite(y)) { printf("NaN at %d sr=%g\n", i, sr); return 1; }
      peak = std::max(peak, (double)std::abs(y));
    }
    printf("sr=%6.0f fft=%d latency=%d samples (%.1f ms) peak=%.2f OK\n", sr, s.getFftSize(), s.getLatency(), 1000.0*s.getLatency()/sr, peak);
  }
}
