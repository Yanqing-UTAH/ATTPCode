#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        std::cout << argv[0] << " <infile> <outfile> <nlines> <num_reports>" << std::endl;
        return 1;
    }
    
    std::ifstream fin(argv[1]);
    std::ofstream fout(argv[2]);
    auto nlines = strtoull(argv[3], nullptr, 0);
    auto num_reports = strtoull(argv[4], nullptr, 0);
    
    auto report_interval = nlines / num_reports;
    unsigned long long counter = 0;    

    std::string line;
    while (std::getline(fin, line)) {
        fout << line << std::endl;
        if (++counter == report_interval) {
            fout << "+" << std::endl;
            counter = 0;
        }
    }

    return 0;
}
