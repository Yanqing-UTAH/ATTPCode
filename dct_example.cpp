#include <iostream>
#include <fstream>

double input[] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10
};
constexpr size_t input_dim = sizeof(input) / sizeof(input[0]);

// In python3:
// scipy.fftpack.dct(input, 4, norm='ortho')
double expected_output[] = {
    11.81857236,
    -12.0523535 ,
    5.42995573, 
    -4.84272707,
    3.41361626,
    -3.21323163,
    2.67329439, 
    -2.59144377,
    2.38698227,
    -2.36395779
};

int main(int argc, char *argv[]) {
    

}

