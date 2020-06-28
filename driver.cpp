#include <iostream>
#include <cstring>
#include <vector>
#include "conf.h"
#include "misra_gries.h"
#include "sketch_lib.h"
#include "query.h"

//using namespace std;

/* old impl. moved to old_driver.cpp */
int old_main(int argc, char *argv[]);

/* new impl. */

void print_new_help(const char *progname)
{
    if (progname)
    {
        std::cerr<< "usage: " << progname << " run <ConfigFile>" << std::endl;
        std::cerr<< "usage: " << progname << " help <QueryType>" << std::endl;
    }
    std::cerr << "Available query types:" << std::endl;
    std::cerr << "\theavy_hitter" << std::endl;
    std::cerr << "\theavy_hitter_bitp" << std::endl;
    std::cerr << "\tmatrix_sketch" << std::endl;
    std::cerr << "\tfrequency_estimation" << std::endl;
    std::cerr << "\tfrequency_estimation_bitp" << std::endl;
}

int
main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (!strcmp(argv[1], "--run-old"))
        {
            argv[1] = argv[0];
            return old_main(argc - 1, argv + 1);
        }
        else if (!strcmp(argv[1], "--test-misra-gries"))
        {
            argv[1] = argv[0];
            return MisraGries::unit_test(argc - 1, argv + 1);
        }
    }

    setup_sketch_lib();
    setup_config();

    int argi = 0;
    const char *progname = argv[argi++];
    if (argc < 3)
    {
        print_new_help(progname);
        return 1;
    }
    
    const char *command = argv[argi++];
    if (!strcmp(command, "run"))
    {
        const char *config_file = argv[argi++];
        const char *help_str;
        if (!g_config->parse_file(config_file, &help_str))
        {
            std::cerr << help_str; 
            return 2;
        }
        
        std::string query_type = g_config->get("test_name").value();
        if (!is_supported_query_type(query_type))
        {
            std::cerr << "[ERROR] Invalid query type " << query_type << std::endl;
            print_new_help(nullptr);
            return 1;
        }

        return run_query(query_type);
    }
    else if (!strcmp(command, "help"))
    {
        const char *query_type = argv[argi++];
        if (is_supported_query_type(std::string(query_type)))
        {
            std::cerr << "Query " << query_type << std::endl;
        }
        else
        {
            std::cerr << "[ERROR] Invalid query type " << query_type << std::endl;
            print_new_help(nullptr);
            return 1;
        }

        std::cerr << "Supported sketches:" << std::endl;
        std::vector<SKETCH_TYPE> supported_sketch_types =
            check_query_type(query_type, nullptr);
        for (SKETCH_TYPE st: supported_sketch_types)
        {
            std::cerr << '\t' << sketch_type_to_sketch_name(st) << " ("
                << sketch_type_to_altname(st) << ')' << std::endl;
        }
    }
    else
    {
        print_new_help(progname);
        return 1;
    }

    return 0;
}

