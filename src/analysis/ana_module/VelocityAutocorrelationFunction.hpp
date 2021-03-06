//
// Created by xiamr on 8/13/19.
//

#ifndef TINKER_VELOCITYAUTOCORRELATIONFUNCTION_HPP
#define TINKER_VELOCITYAUTOCORRELATIONFUNCTION_HPP

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "utils/std.hpp"

class Frame;

class VelocityAutocorrelationFunction : public AbstractAnalysis {
public:
    VelocityAutocorrelationFunction();

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "Velocity Autocorrelation Function"; }

    [[nodiscard]] static std::vector<double> calculateAcf(
        const std::vector<std::deque<std::tuple<double, double, double>>> &velocities, int max_time_gap_frame);

    void setParameters(double time_increment_ps, double max_time_gap_ps, const std::string &outfilename);

protected:
    double time_increment_ps;
    double max_time_gap_ps;

    std::vector<std::deque<std::tuple<double, double, double>>> velocities;

    std::vector<double> acf;
};

#endif  // TINKER_VELOCITYAUTOCORRELATIONFUNCTION_HPP
