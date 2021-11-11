#include <iostream>
#include <fstream>
#include <cstdint>
#include <cctype>
#include <string>
#include <cstdlib>
using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "usage: " << argv[0] << " <datafile> <key_set_file>" << std::endl;
        return 1;
    }

    std::string datafile = argv[1];
    std::string outfile = datafile + "-fe";
    std::string key_set_file = argv[2];

    std::ifstream data_in(datafile);
    std::ofstream data_out(outfile);
    std::ifstream key_in(key_set_file);
    
    std::string line;
    while (std::getline(data_in, line))
    {
        if (line.length() < 3 || line[0] != '?')
        {
            data_out << line << std::endl; 
            continue;
        }
    
        uint64_t ts1 = strtoul(line.c_str() + 2, nullptr, 0);
        
        // construct the key set
        if (!std::getline(key_in, line) || !std::getline(key_in, line))
        {
            std::cout << "premature eof in " << key_set_file << std::endl;
            return 1;
        }
        
        auto p = line.find('|');
        if (p == std::string::npos)
        {
            std::cout << "malformatted key set file " << key_set_file << std::endl;
            std::cout << line << std::endl;
            return 1;
        }
        auto p2 = line.find(')',p);
        if (p2 == std::string::npos)
        {
            std::cout << "malformatted key set file " << key_set_file << std::endl;
            std::cout << line << std::endl;
            return 1;
        }
        line[p2] = '\0';
        uint64_t ts2 = strtoul(line.c_str() + p + 1, nullptr, 0);

        if (ts1 != ts2)
        {
            std::cout << "key set file should be generated from the data file" << std::endl;
            return 1;
        }

        data_out << "? " << ts1;
        
        while (std::getline(key_in, line))
        {
            if (line.empty())
            {
                std::cout << "malformatted key set file " << key_set_file << std::endl;
                std::cout << line << std::endl;
                return 1;
            }
            if (line[0] == '}')
            {
                break;
            }
            
            size_t p = 0;
            while (p < line.length() && std::isspace(line[p])) ++p;
            if (p == line.length())
            {
                std::cout << "malformatted key set file " << key_set_file << std::endl;
                std::cout << line << std::endl;
                return 1;
            }
            size_t p2 = p;
            while (p2 < line.length() && std::isdigit(line[p2])) ++p2;
            if (p2 < line.length())
            {
                line[p2] = '\0';
            }
            data_out << ' ' << line.c_str() + p;
        }

        data_out << std::endl;
    }

    return 0;
}
