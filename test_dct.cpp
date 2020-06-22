#include <iostream>
#include <fstream>
#include <fftw3.h>
#include <cmath>
#include <cstring>

const double input[] = {
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

void rescale_and_check_output(double *output) {
    double scale_factor = std::sqrt(2 * input_dim);
    bool ok = true;
    for (size_t i = 0; i < input_dim; ++i) {
        output[i] /= scale_factor;
        if (std::abs(output[i] - expected_output[i]) > 1e-8) {
            ok = false;
            break;
        }
    }

    if (ok) {
        std::cout << "Ok" << std::endl;
    } else {
        std::cout << "fftw_output\texpected_output" << std::endl;
        for (size_t i = 0; i < input_dim; ++i) {
            std::cout << output[i] << '\t' << expected_output[i] << std::endl;
        }
    }

}

int main(int argc, char *argv[]) {
    FILE *wisdom_file = fopen("test_dct.fftw", "r");
    if (wisdom_file) {
        fclose(wisdom_file);
        std::cout << "importing widsom..." << std::endl;
        if (!fftw_import_wisdom_from_filename("test_dct.fftw")) {
            std::cout << "[WARN] error when importing wisdom" << std::endl;
        }
    }

    double *input_arr = new double[input_dim];

    fftw_plan plan = fftw_plan_r2r_1d(
        input_dim,
        input_arr, // input
        input_arr, // output
        FFTW_REDFT11, // kind
        FFTW_MEASURE);
    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    rescale_and_check_output(input_arr);
    
    double *output_arr = new double[input_dim];
    int n = input_dim;
    fftw_r2r_kind kind = FFTW_REDFT11; // dct type 4
    plan = fftw_plan_many_r2r(
        1, // rank
        &n,
        1, // howmany
        input_arr, // in
        nullptr, // inembed
        1, // istride
        0, // idist (not used for howmany == 1)
        output_arr, // out
        nullptr, // onembed
        1, // ostride
        0, // odist (not used for howmnay == 1)
        &kind,
        FFTW_MEASURE);
    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    rescale_and_check_output(output_arr);
     
    std::cout << "exporting wisdom..." << std::endl;
    fftw_export_wisdom_to_filename("test_dct.fftw");

    return 0;
}

