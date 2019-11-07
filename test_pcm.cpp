#include <iostream>
#include <cstring>
#include "pcm.h"

using namespace std;

  const char *l_data[] = {
    "hello", "some", "one", "hello", "alice",
    "one", "lady", "let", "us", "lady",
    "alice", "in", "wonderland", "us", "lady",
    "lady", "some", "hello", "none", "pie"

  };

int count(int begin, int end, const char * str) {
    int cnt = 0;
    for (int i = begin; i <= end; ++i) {
        if (!strcmp(str, l_data[i])) {
            cnt += 1;
        }
    }
    return cnt;
}

void cm_test(CMSketch *cm, const char *str) {
    cout << str << ":\tEst: " << cm->estimate(str) << "\tTruth: " << count(0, 19, str) << endl; 
}

void pcm_test(PCMSketch *pcm, const char *str, int begin, int end) {
    cout << str << "[" << begin << "," << end << "]:\tEst: " << pcm->estimate_point_in_interval(str, begin, end) << "\tTruth: " << count(begin, end, str) << endl; 
}

int main(int argc, char **argv) {

  double epsilon = 0.01;
  double delta = 0.1;
  double Delta = 0.5;

  cout << "CM SKETCH" << endl;
  CMSketch cm(epsilon, delta);

  for (int i = 0; i < 20; i++) {
      cm.update(l_data[i]);
      //element, count = 1
  }
  cm_test(&cm, "lady");
  cm_test(&cm, "hello");
  cm_test(&cm, "none");
  cm_test(&cm, "sketch");

  cout << "PCM SKETCH" << endl;

  PCMSketch pcm(epsilon, delta, Delta);

  for (int i = 0; i < 20; i++) {
      pcm.update(i + 1, l_data[i]);
      //ts, element, count = 1
  }
  pcm_test(&pcm, "lady", 0, 7);
  pcm_test(&pcm, "lady", 0, 4);
  pcm_test(&pcm, "lady", 6, 16);

  return 0;
}
