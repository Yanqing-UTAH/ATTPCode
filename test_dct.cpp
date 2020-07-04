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
int n = (int) input_dim;
fftw_r2r_kind kind = FFTW_REDFT11; // dct type 4

double input_arr[input_dim];
double output_arr[input_dim];

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

void test_having_existing_wisdom() {
    fftw_plan plan;
    
    std::cout << "testing existing wisdom..." << std::endl;
    fftw_r2r_kind kind = FFTW_REDFT11; // dct type 4
    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    plan = fftw_plan_r2r_1d(
        input_dim,
        input_arr, // input
        output_arr, // output
        FFTW_REDFT11, // kind
        FFTW_WISDOM_ONLY | FFTW_PRESERVE_INPUT);
    if (!plan) {
        std::cout << "Error: plan failed with FFTW_WISDOM_ONLY" << std::endl;
    } else {
        if (!std::equal(input_arr, input_arr + input_dim, input)) {
            std::cout << "Error: input_arr is overwritten" << std::endl;
        } else {
            fftw_execute(plan);
            fftw_destroy_plan(plan);
            rescale_and_check_output(output_arr);
        }
    }

    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    plan = fftw_plan_many_r2r(
        1, // rank
        &n,
        1, // howmany
        input_arr, // in
        nullptr, // inembed
        1, // istride
        0, // idist (not used for howmany == 1)
        input_arr, // out
        nullptr, // onembed
        1, // ostride
        0, // odist (not used for howmnay == 1)
        &kind,
        FFTW_WISDOM_ONLY | FFTW_PRESERVE_INPUT);
    if (!plan) {
        std::cout << "Error: plan failed with FFTW_WISDOM_ONLY" << std::endl;
    } else {
        if (!std::equal(input_arr, input_arr + input_dim, input)) {
            std::cout << "Error: input_arr is overwritten" << std::endl;
        } else {
            fftw_execute(plan);
            fftw_destroy_plan(plan);
            rescale_and_check_output(input_arr);
        }
    }

    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    plan = fftw_plan_r2r_1d(
        input_dim,
        input_arr, // input
        output_arr, // output
        FFTW_REDFT10, // kind
        FFTW_WISDOM_ONLY);
    if (plan) {
        std::cout << "Error: not expecting a valid plan" << std::endl;
    } else {
        std::cout << "Ok" << std::endl;
    }

    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    plan = fftw_plan_r2r_1d(
        input_dim - 1,
        output_arr, // input
        output_arr, // output
        FFTW_REDFT11, // kind
        FFTW_WISDOM_ONLY);
    if (plan) {
        std::cout << "Error: not expecting a valid plan" << std::endl;
    } else {
        std::cout << "Ok" << std::endl;
    }
    std::cout << "testing existing wisdom done." << std::endl;
}

int main(int argc, char *argv[]) {
    std::cout << "importing wisdom..." << std::endl;
    if (!fftw_import_wisdom_from_filename("test_dct.fftw3")) {
        std::cout << "[WARN] error when importing wisdom" << std::endl;
    } else {
        std::cout << "test imported wisdom..." << std::endl;
        test_having_existing_wisdom();
        std::cout << "test imported wisdom done." << std::endl;
    }

    fftw_plan plan = fftw_plan_r2r_1d(
        input_dim,
        input_arr, // input
        input_arr, // output
        FFTW_REDFT11, // kind
        FFTW_PATIENT);
    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    rescale_and_check_output(input_arr);

    test_having_existing_wisdom();
    test_having_existing_wisdom();
    
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
        FFTW_PATIENT);
    n = 100000000;
    std::memcpy(input_arr, input, sizeof(double) * input_dim);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    rescale_and_check_output(output_arr);
    n = input_dim;
    
    test_having_existing_wisdom();
     
    std::cout << "exporting wisdom..." << std::endl;
    fftw_export_wisdom_to_filename("test_dct.fftw3");

    return 0;
}

