#include "pla.h"
#include <iostream>
using namespace std;

void test() {
    vector<PLA::point> stream {
        {1, 2},
            {2, 4},
            {3, 2},
            {4, 4},
            {5, 5},
            {6, 4},
            {7, 5},
            {8, 5},
            {9, 6},
            {10, 4},
            {11, 4},
            {12, 6},
            {13, 6},
            {14, 6},
            {15, 7},
            {16, 7},
            {17, 9},
            {18, 10},
            {19, 11},
            {20, 13},
            {9999, 13}};

    vector<PLA::segment> result {
        {1, 2, 2, 0},
        {2, 3, -2, 8},
        {3, 7, 1, -1},
        {7, 11, -0.416667, 8.91667},
        {11, 19, 0.720833, -3.59583},
        {19, 20, 2.9, -45}};
    PLA pla(1.0);
    for (auto it = stream.begin(); it != stream.end(); it++) {
        pla.feed(*it);
    }
    auto pass = true;
    for (auto it = pla.result.begin(), it2 = result.begin(); 
            it != pla.result.end() && it2!= result.end();
            it++, it2++) {
        auto j = *it; 
        auto k = *it2;
        cout << "In [" << j.start << "," << j.end << "]: y=" << j.slope << "*x+(" << j.intercept << ")" << endl;
        if (j.start != k.start || j.end != k.end || abs(j.slope-k.slope) > 0.01 || abs(j.intercept-k.intercept) > 0.01) {
            cout << "Failed!" << endl;
            pass = false;
        }
    }
    if (pass) {
        cout << "Pass!" << endl;
    }
}

int main() {
    test();
}
