
#ifndef TINKER_ANGLEWAT_HPP
#define TINKER_ANGLEWAT_HPP

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"
#include "utils/std.hpp"

class Frame;

class AngleWat : public AbstractAnalysis {
public:
    AngleWat();

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "Angle (Ow-Hw) Distribution with cutoff"; }

protected:
    AmberMask mask1, mask2, mask3;

    std::unordered_set<std::shared_ptr<Atom>> group1, group2, group3;

    std::list<std::tuple<std::shared_ptr<Atom>, std::shared_ptr<Atom>>> pairs;

    double angle_width{};

    int angle_bins{};

    std::unordered_map<int, std::size_t> hist;

    double cutoff1{}, cutoff2{};

    void printData(std::ostream &os) const;
};

#endif  // TINKER_ANGLEWAT_HPP
