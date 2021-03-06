

#ifndef TINKER_CONDITIONALTIMECORRELATIONFUNCTION_HPP
#define TINKER_CONDITIONALTIMECORRELATIONFUNCTION_HPP

#include <boost/circular_buffer.hpp>
#include <boost/multi_array.hpp>

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"
#include "utils/std.hpp"

class Frame;

class VectorSelector;

class ConditionalTimeCorrelationFunction : public AbstractAnalysis {
public:
    ConditionalTimeCorrelationFunction();

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() {
        return "Conditional Time Correlation Function (JCTC 2019, 15, 803−812)";
    }

    void setParameters(const AmberMask &M, const AmberMask &L, const std::shared_ptr<VectorSelector> &vector, int P,
                       double width, double max_r, double time_increment_ps, double max_time_gap_ps,
                       const std::string &out);

protected:
    void normalize();

    template <typename Function>
    void calculateFrame(Function f);

    const std::unordered_map<int, std::function<void()>> func_mapping;

    AmberMask reference_atom_mask;
    AmberMask water_Ow_atoms_mask;

    std::shared_ptr<Atom> reference_atom;
    std::vector<std::shared_ptr<Atom>> water_Ow_atoms;

    std::shared_ptr<VectorSelector> vectorSelector;

    std::vector<boost::circular_buffer<std::tuple<double, double, double>>> cb_vector;

    boost::circular_buffer<std::vector<double>> cache_x;

    double time_increment_ps;
    // Number of time points of time correlation functions
    double max_time_gap_ps;

    int max_time_gap_frame;

    double max_distance;
    double distance_width;
    int distance_bins;

    int LegendrePolynomial;

    boost::multi_array<double, 2> ctcf;
};

#endif  // TINKER_CONDITIONALTIMECORRELATIONFUNCTION_HPP
