
#include <boost/range/algorithm.hpp>

#include "AngleWat.hpp"

#include "data_structure/frame.hpp"
#include "utils/ThrowAssert.hpp"
#include "utils/common.hpp"

AngleWat::AngleWat() { enable_outfile = true; }

void AngleWat::processFirstFrame(std::shared_ptr<Frame> &frame) {
    boost::for_each(frame->atom_list, [this](std::shared_ptr<Atom> &atom) {
        if (is_match(atom, mask1)) group1.insert(atom);
        if (is_match(atom, mask2)) group2.insert(atom);
        if (is_match(atom, mask3)) group3.insert(atom);
    });
    for (auto &vec1_atom1 : group2) {
        for (auto &vec1_atom2 : group3) {
            if (vec1_atom1->molecule.lock() == vec1_atom2->molecule.lock()) {
                pairs.emplace_back(vec1_atom1, vec1_atom2);
            }
        }
    }
}

void AngleWat::process(std::shared_ptr<Frame> &frame) {
    for (auto &[vec1_atom1, vec1_atom2] : pairs) {
        double xr = vec1_atom2->x - vec1_atom1->x;
        double yr = vec1_atom2->y - vec1_atom1->y;
        double zr = vec1_atom2->z - vec1_atom1->z;

        frame->image(xr, yr, zr);

        double leng1 = sqrt(xr * xr + yr * yr + zr * zr);

        for (auto &atom3 : group1) {
            double xr2 = vec1_atom1->x - atom3->x;
            double yr2 = vec1_atom1->y - atom3->y;
            double zr2 = vec1_atom1->z - atom3->z;

            frame->image(xr2, yr2, zr2);

            double distance = sqrt(xr2 * xr2 + yr2 * yr2 + zr2 * zr2);

            if (cutoff1 <= distance and distance < cutoff2) {
                double _cos = (xr * xr2 + yr * yr2 + zr * zr2) / (distance * leng1);

                double angle = acos(_cos) * radian;

                int i_angle_bin = int(angle / angle_width) + 1;

                if (i_angle_bin <= angle_bins) {
                    hist[i_angle_bin] += 1;
                }
            }
        }
    }
}

void AngleWat::print(std::ostream &os) {
    os << std::string(50, '#') << '\n';
    os << "# " << AngleWat::title() << '\n';
    os << "# Group1 > " << mask1 << '\n';
    os << "# Group2 > " << mask2 << '\n';
    os << "# Group3 > " << mask3 << '\n';
    os << "# angle_width(degree) > " << angle_width << '\n';
    os << "# Cutoff1(Ang) > " << cutoff1 << '\n';
    os << "# Cutoff2(Ang) > " << cutoff2 << '\n';
    os << std::string(50, '#') << '\n';
    os << format("#%15s %15s\n", "Angle(degree)", "Probability Density(% degree-1)");

    printData(os);

    os << std::string(50, '#') << '\n';
}

void AngleWat::readInfo() {
    select1group(mask1, "Enter mask for atom1(Metal) : ");
    select1group(mask2, "Enter mask for atom2(Ow) : ");
    select1group(mask3, "Enter mask for atom3(Oh) : ");

    double angle_max = choose(0.0, 180.0, "Enter Maximum Angle to Accumulate[180.0 degree]:", Default(180.0));
    angle_width = choose(0.0, 180.0, "Enter Width of Angle Bins [0.5 degree]:", Default(0.5));

    cutoff1 = choose(0.0, 100.0, "Cutoff1 [Angstrom]:");
    cutoff2 = choose(0.0, 100.0, "Cutoff2 [Angstrom]:");

    throw_assert(cutoff1 < cutoff2, "Cutoff1 must less than Cutoff2");

    angle_bins = int(angle_max / angle_width);

    for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
        hist[i_angle] = 0;
    }
}

void AngleWat::printData(std::ostream &os) const {
    double total = 0.0;

    for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
        total += hist.at(i_angle);
    }

    for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
        os << format("%15.3f %15.3f\n", (i_angle - 0.5) * angle_width, 100 * (hist.at(i_angle) / total) / angle_width);
    }
}
