#include <iostream>
#include "conf.h"
#include "misra_gries.h"
#include "sketch.h"
#include "query.h"

//using namespace std;

/* old impl. moved to old_driver.cpp */
int old_main(int argc, char *argv[]);

/* new impl. */

template<class Q>
int run_query()
{
    Q query;
    int ret;

    if ((ret = query.setup())) return ret;
    if ((ret = query.run())) return ret;
    if ((ret = query.print_stats())) return ret;

    return 0;
}

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
        if (query_type == "heavy_hitter")
        {
            return run_query<QueryHeavyHitter>();
            //return run_new_heavy_hitter();
        }
        else if (query_type == "heavy_hitter_bitp")
        {
            return run_query<QueryHeavyHitterBITP>();
        }
        else if (query_type == "matrix_sketch")
        {
            return run_query<QueryMatrixSketch>();
        }
        else
        {
            std::cerr << "[ERROR] Invalid query type " << query_type << std::endl;
            print_new_help(nullptr);
            return 1;
        }
    }
    else if (!strcmp(command, "help"))
    {
        const char *query_type = argv[argi++];
        if (!strcmp(query_type, "heavy_hitter"))
        {
            std::cerr << "Query heavy_hitter" << std::endl; 
        }
        else if (!strcmp(query_type, "heavy_hitter_bitp"))
        {
            std::cerr << "Query heavy_hitter_bitp" << std::endl;
        }
        else if (!strcmp(query_type, "matrix_sketch"))
        {
            std::cerr << "Query matrix_sketch" << std::endl;
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

