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
    struct PartialCkpt {
        TIMESTAMP       ts;
    
        double          *row;
    };

    struct FullCkpt {
        TIMESTAMP       ts;
        
        FD              *fd;

        uint32_t        next_partial_ckpt;
    };

    uint32_t l;
    uint32_t d;
    FD *C;
    double AF2;
    std::vector<PartialCkpt> partial_ckpt;
    std::vector<FullCkpt> full_ckpt;
    uint32_t ckpt_cnt;

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
        const double *dvec) override;

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
