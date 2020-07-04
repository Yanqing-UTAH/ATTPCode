#include "sketch.h"
#include <unordered_map>

class ExactHeavyHitters:
    public IPersistentHeavyHitterSketch,
    public IPersistentHeavyHitterSketchBITP,
    public IPersistentFrequencyEstimationSketch,
    public IPersistentFrequencyEstimationSketchBITP
{
private:
    struct Item
    {
        TIMESTAMP       m_ts;

        uint64_t        m_cnt;
    };

public:
    ExactHeavyHitters();
    
    virtual ~ExactHeavyHitters();

    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;

    void
    update(TIMESTAMP ts, uint32_t value, int c = 1) override;

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

    std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
    estimate_heavy_hitters_bitp(
        TIMESTAMP ts_s,
        double frac_threshold) const override;

    uint64_t
    estimate_frequency(
        TIMESTAMP ts_e,
        uint32_t key) const override;

    uint64_t
    estimate_frequency_bitp(
        TIMESTAMP ts_s,
        uint32_t key) const override;

private:
    std::unordered_map<uint32_t, std::vector<Item>> m_items;

    const std::unordered_map<uint32_t, std::vector<Item>>::size_type
                                                    m_initial_bucket_count;

public:

    static ExactHeavyHitters *create(int &argi, int argc, char *argv[], const char **help_str);

    static ExactHeavyHitters *get_test_instance();
    
    static ExactHeavyHitters *create_from_config(int idx = -1);
};

class ExactMatrix:
    public IPersistentMatrixSketch
{
public:
    ExactMatrix(int n);

    virtual
    ~ExactMatrix();

    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;

    void
    update(
        TIMESTAMP       ts,
        const double    *dvec) override;
    
    void
    get_covariance_matrix(
        TIMESTAMP       ts_e,
        double          *A) const override;

private:

    inline size_t
    matrix_size() const
    {
        return m_n * (m_n + 1) / 2;
    }

    int                                 m_n;

    TIMESTAMP                           m_last_ts;

    double                              *m_cur_matrix;

    std::vector<std::pair<TIMESTAMP, double*>>
                                        m_matrices;

public:
    static ExactMatrix *get_test_instance();

    static ExactMatrix *create_from_config(int idx = -1);
};

