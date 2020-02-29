
#ifndef TINKER_ANGLE_HPP
#define TINKER_ANGLE_HPP

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "utils/std.hpp"
#include <Eigen/Eigen>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

class Frame;

class Angle : public AbstractAnalysis {
public:
    Angle();

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "Angle between two groups (mass-weighted)"; }

protected:

    enum class AxisType {
        MIN, MAX
    };

    Eigen::Matrix3d calculate_inertia(
            std::shared_ptr<Frame> &frame, const std::vector<std::shared_ptr<Atom>> &atom_group);

    [[nodiscard]] std::tuple<double, double, double>
    calculate_axis(AxisType type, const Eigen::Matrix3d &inertia) const;

    AmberMask mask1, mask2;

    AxisType type1, type2;

    std::vector<std::shared_ptr<Atom>> atom_group1, atom_group2;

    std::deque<double> deque;
};

#endif // TINKER_ANGLE_HPP
