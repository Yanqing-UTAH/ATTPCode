#ifndef FD_H
#define FD_H

#include "sketch.h"
#include <vector>
#include <string>
#include <utility>

// we want keep class FD private to FD_ATTP impl.
class FD;

class FD_ATTP:
    public IPersistentMatrixSketch
{
    int l;
    int d;
    FD *C;
    double AF2;
    std::vector<std::pair<TIMESTAMP, std::vector<double>>> partial_ckpt;
    std::vector<std::pair<TIMESTAMP, FD*>> full_ckpt;
    int ckpt_cnt;

public:

    FD_ATTP(int _l, int _d);
    
    virtual
    ~FD_ATTP();

    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;

    void
    update(
        TIMESTAMP ts,
        double *dvec) override;

    void
    get_covariance_matrix(
        TIMESTAMP ts_e,
        double *A) const override;

    static FD_ATTP*
    get_test_instance();

    static FD_ATTP*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();
};

#endif // FD_H
