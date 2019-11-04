#include <iostream>
#include <cstring>
#include "pams.h"

using namespace std;

  const char *l_data[] = {
    "hello", "some", "one", "hello", "alice",
    "one", "lady", "let", "us", "lady",
    "alice", "in", "wonderland", "us", "lady",
    "lady", "some", "hello", "none", "pie"

  };

int count(int begin, int end, const char * str) {
    int cnt = 0;
    for (int i = begin; i < end; ++i) {
        if (!strcmp(str, l_data[i])) {
            cnt += 1;
        }
    }
    return cnt;
}

void pams_test(PAMSketch *pams, const char *str, int begin, int end) {
    cout << str << "[" << begin << "," << end << "]:\tEst: " << pams->estimate_point_in_interval(str, begin, end) << "\tTruth: " << count(begin, end, str) << endl; 
}

int main(int argc, char **argv) {

  double epsilon = 0.01;
  double delta = 0.1;
  double Delta = 0.5;

  cout << "pams SKETCH" << endl;
  PAMSketch pams(epsilon, delta, Delta);

  for (int i = 0; i < 20; i++) {
      pams.update(i + 1, l_data[i]);
      //ts, element, count = 1
  }
  pams_test(&pams, "lady", 0, 7);
  pams_test(&pams, "lady", 0, 4);
  pams_test(&pams, "lady", 6, 16);

  return 0;
}
