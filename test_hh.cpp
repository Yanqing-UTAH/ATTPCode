#include <iostream>
#include <cstring>
#include <vector>
#include "heavyhitters.h"

using namespace std;

int main(int argc, char **argv) {

  HeavyHitters hh(3);

  for (int i = 0; i < 8; i++) {
	  hh.update(1, i, i * 100);
  }
  int threshold = 400;
  auto res = hh.query_hh(1, threshold);
  cout << "Threshold: " << threshold << endl;
  cout << res.size() << " items found" << endl;
  for (int x : res) {
	  cout << x << endl;
  }

  return 0;
}
