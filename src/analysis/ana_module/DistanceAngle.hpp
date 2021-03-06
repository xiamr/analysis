
#ifndef TINKER_DISTANCEANGLE_HPP
#define TINKER_DISTANCEANGLE_HPP

#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"

class DistanceAngle : public AbstractAnalysis {
public:
    DistanceAngle();

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "Gibbs Free Energy of Distance Angle for AnO2"; }

protected:
    AmberMask mask1, mask2, mask3;

    std::list<std::pair<std::shared_ptr<Atom>, std::shared_ptr<Atom>>> pairs;

    std::unordered_set<std::shared_ptr<Atom>> group3;

    double distance_width;
    double angle_width;

    int distance_bins;
    int angle_bins;

    double temperature;  // unit: K

    std::map<std::pair<int, int>, size_t> hist;

    void printData(std::ostream &os) const;
};

#endif  // TINKER_DISTANCEANGLE_HPP
