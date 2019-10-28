#include <iostream>
#include <cstring>
#include "pcm.h"

using namespace std;

  const char *data[] = {
    "hello", "some", "one", "hello", "alice",
    "one", "lady", "let", "us", "lady",
    "alice", "in", "wonderland", "us", "lady",
    "lady", "some", "hello", "none", "pie"

  };

int count(int begin, int end, const char * str) {
    int cnt = 0;
    for (int i = begin; i <= end; ++i) {
        if (!strcmp(str, data[i])) {
            cnt += 1;
        }
    }
    return cnt;
}

void pcm_test(PCMSketch *pcm, const char *str, int begin, int end) {
    cout << str << "[" << begin << "," << end << "]:\tEst: " << pcm->estimate_point_in_interval(str, begin, end) << "\tTruth: " << count(begin, end, str) << endl; 
}

int main(int argc, char **argv) {

  double epsilon = 0.01;
  double delta = 0.1;
  double Delta = 0.5;

  PCMSketch pcm(epsilon, delta, Delta);

  for (int i = 0; i < 20; i++) {
      pcm.update(i, data[i]);
  }
  //pcm_test(&pcm, "lady", 0, 7);
  //pcm_test(&pcm, "lady", 0, 4);
  //pcm_test(&pcm, "lady", 6, 15);

  return 0;
}
