extern "C"
{
#include <lapacke.h>
}
#include "lapack_wrapper.h"
#include "query.h"

////////////////////////////////////////
// Matrix sketch query implementation //
////////////////////////////////////////
QueryMatrixSketchImpl::QueryMatrixSketchImpl():
    m_exact_enabled(false),
    m_n(0),
    m_dvec(nullptr),
    m_last_answer(nullptr),
    m_exact_covariance_matrix(nullptr),
    m_work(nullptr),
    m_singular_values(nullptr),
    m_exact_fnorm(0)
{}

QueryMatrixSketchImpl::~QueryMatrixSketchImpl()
{
    delete []m_dvec;
    delete []m_last_answer;
    delete []m_exact_covariance_matrix;
    delete []m_work;
    delete []m_singular_values;
}

int
QueryMatrixSketchImpl::early_setup()
{
    if (g_config->is_assigned("MS.dimension"))
    {
        m_n = g_config->get_u32("MS.dimension").value();
    }
    else
    {
        // infer m_n from the input
        std::string infile = g_config->get("infile").value();
        std::ifstream fin(infile);
        if (!fin) return 1;

        std::string line;
        m_n = 0;
        while (std::getline(fin, line))
        {
            if (line.empty()) continue;
            if (line[0] == '?' || line[0] == '#') continue;

            m_n = 0;
            const char *s = line.c_str();
            while (*s && !std::isspace(*s)) ++s;
            for (;*s;)
            {
                while (std::isspace(*s)) ++s;
                if (*s)
                {
                    ++m_n;
                    while (*s && !std::isspace(*s)) ++s;
                }
            }
            break;
        }

        if (m_n == 0)
        {
            std::cerr << "[ERROR] Unable to determine matrix dimension" << std::endl;
            return 1;
        }

        g_config->set_u32("MS.dimension", m_n);
    }
    
    m_dvec = new double[m_n];
    m_last_answer = new double[matrix_size()];
    m_exact_covariance_matrix = new double[matrix_size()];
    m_work = new double[m_n * m_n];
    m_singular_values = new double[m_n];

    return 0;
}

int
QueryMatrixSketchImpl::additional_setup()
{

    if (g_config->get_boolean("EXACT_MS.enabled").value())
    {
        m_exact_enabled = true;
        if (m_sketches.size() == 0 ||
            m_sketches[0].get()->get_short_description() != "EXACT_MS")
        {
            std::cerr << "[ERROR] exact query should come first" << std::endl;
            return 1;
        }
    }
    else
    {
        m_exact_enabled = false;
    }

    return 0;
}

int
QueryMatrixSketchImpl::parse_query_arg(
    TIMESTAMP ts,
    const char *str)
{
    m_out << "MS(" << ts << "):" << std::endl;
    return 0;
}

void
QueryMatrixSketchImpl::query(
    IPersistentMatrixSketch *sketch,
    TIMESTAMP ts)
{
    sketch->get_covariance_matrix(ts, m_last_answer);
}

void
QueryMatrixSketchImpl::print_query_summary(
    IPersistentMatrixSketch *sketch)
{
    if (m_exact_enabled && sketch == m_sketches[0].get())
    {
        std::swap(m_last_answer, m_exact_covariance_matrix);
        m_exact_fnorm = lapack_wrapper_dlansp('f', 'u', m_n, m_exact_covariance_matrix);
        m_out << "\tF-norm = " << m_exact_fnorm << std::endl;
    }
    else
    {
        // expand the packed form to a general matrix
        int k = 0;
        for (int j = 0; j < m_n; ++j)
        {
            for (int i = 0; i < j; ++i)
            {
                m_work[i * m_n + j] = 
                m_work[j * m_n + i] = m_last_answer[k] - m_exact_covariance_matrix[k];
                ++k;
            }
            m_work[j * m_n + j] = m_last_answer[k] - m_exact_covariance_matrix[k];
            ++k;
        }

        // do svd
        (void) LAPACKE_dgesdd(
            LAPACK_COL_MAJOR,
            'N',
            m_n,
            m_n,
            m_work,
            m_n,
            m_singular_values,
            nullptr,
            1,
            nullptr,
            1);

        m_out << '\t'
            << sketch->get_short_description()
            << ": "
            << "||ATA-BTB||_2 / ||A||_F^2 = "
            << m_singular_values[0] / (m_exact_fnorm * m_exact_fnorm)
            << std::endl;
    }
}

void
QueryMatrixSketchImpl::dump_query_result(
    IPersistentMatrixSketch *sketch,
    std::ostream &out,
    TIMESTAMP ts,
    uint64_t out_limit)
{
    // TODO do something
}

int
QueryMatrixSketchImpl::parse_update_arg(
    const char *str)
{
    const char *s = str;
    for (int i = 0; i < m_n; ++i)
    {
        char *s2;
        m_dvec[i] = strtod(s, &s2);
        if (s2 == s || (m_dvec[i] == HUGE_VAL)) return 1;
        s = s2;
    }
    
    return 0;
}

void
QueryMatrixSketchImpl::update(
    IPersistentMatrixSketch *sketch,
    TIMESTAMP ts)
{
    sketch->update(ts, m_dvec);
}

