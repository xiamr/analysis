//
// Created by xiamr on 9/24/19.
//

#include "LocalStructureIndexForLiquid.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/numeric.hpp>

#include "LocalStructureIndex.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/Histogram.hpp"
#include "utils/Histogram2D.hpp"
#include "utils/common.hpp"

LocalStructureIndexForLiquid::LocalStructureIndexForLiquid() {
    enable_outfile = true;
    enable_forcefield = true;
}

void LocalStructureIndexForLiquid::process(std::shared_ptr<Frame> &frame) {
    std::vector<double> distance_square_vector;
    std::vector<double> distance_within_cutoff_range;
    for (auto &mol1 : frame->molecule_list) {
        distance_square_vector.clear();
        distance_within_cutoff_range.clear();
        auto mol1_coord = mol1->calc_weigh_center(frame);
        for (auto &mol2 : frame->molecule_list) {
            if (mol1 != mol2) {
                auto mol2_coord = mol2->calc_weigh_center(frame);
                auto v = mol2_coord - mol1_coord;
                frame->image(v);
                auto r2 = vector_norm2(v);
                distance_square_vector.push_back(r2);
                if (r2 < cutoff2) {
                    distance_within_cutoff_range.push_back(std::sqrt(r2));
                }
            }
        }
        auto lsi = LocalStructureIndex::calculateLSI(distance_within_cutoff_range);
        if (distance_square_vector.size() >= r_index) {
            std::nth_element(std::begin(distance_square_vector), std::begin(distance_square_vector) + r_index - 1,
                             std::end(distance_square_vector));
            localStructureIndices.emplace_back(std::sqrt(distance_square_vector[r_index - 1]), lsi);
        } else {
            std::cerr << "WARNING !! no r(" << r_index << ") for r distance vector \n";
        }
    }
}

void LocalStructureIndexForLiquid::print(std::ostream &os) {
    os << std::string(50, '#') << '\n';
    os << "# " << title() << " # \n";
    os << "# distance cutoff(Ang) = " << std::sqrt(cutoff2) << '\n';
    os << "# index for ri = " << r_index << '\n';
    os << std::string(50, '#') << '\n';

    auto [Ri_min_iter, Ri_max_iter] =
        std::minmax_element(std::begin(localStructureIndices), std::end(localStructureIndices),
                            [](auto &lhs, auto &rhs) { return std::get<0>(lhs) < std::get<0>(rhs); });
    auto [LSI_min_iter, LSI_max_iter] =
        std::minmax_element(std::begin(localStructureIndices), std::end(localStructureIndices),
                            [](auto &lhs, auto &rhs) { return std::get<1>(lhs) < std::get<1>(rhs); });

    Histogram2D histogram2D({LSI_min_iter->second, LSI_max_iter->second},
                            (LSI_max_iter->second - LSI_min_iter->second) / 100,
                            {Ri_min_iter->first, Ri_max_iter->first}, (Ri_max_iter->first - Ri_min_iter->first) / 100);

    Histogram histogram({Ri_min_iter->first, Ri_max_iter->first}, (Ri_max_iter->first - Ri_min_iter->first) / 100);

    for (auto [ri, lsi] : localStructureIndices) {
        histogram2D.update(lsi, ri);
        histogram.update(ri);
    }

    os << boost::format("%15s %15s %15s\n") % "LSI(Ang^2)" % ("R" + std::to_string(r_index) + "(Ang)") %
              "Normalized Probability Density";

    auto distribution = histogram2D.getDistribution();
    auto max_population = std::get<2>(
        *boost::max_element(distribution, [](auto &lhs, auto &rhs) { return std::get<2>(lhs) < std::get<2>(rhs); }));

    for (auto [ri, lsi, population] : distribution) {
        os << boost::format("%15.6f %15.6f %15.6f\n") % ri % lsi % (population / max_population);
    }

    os << std::string(50, '#') << '\n';

    auto ri_distribution = histogram.getDistribution();

    auto ri_max_population = std::get<1>(
        *boost::max_element(ri_distribution, [](auto &lhs, auto &rhs) { return std::get<1>(lhs) < std::get<1>(rhs); }));

    os << boost::format("%15s %15s\n") % ("R" + std::to_string(r_index) + "(Ang)") % "Normalized Probability Density";
    for (auto [ri, population] : ri_distribution) {
        os << boost::format("%15.6f %15.6f\n") % ri % (population / ri_max_population);
    }
}

void LocalStructureIndexForLiquid::readInfo() {
    auto cutoff = choose(0.0, "Enter cutoff for local distance(Ang) [3.7] > ", Default(3.7));
    cutoff2 = cutoff * cutoff;
    r_index = choose(1, "Enter i for ri [5] > ", Default(5));
}
