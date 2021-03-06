

#include "EquatorialAngle.hpp"

#include "data_structure/frame.hpp"
#include "utils/ThrowAssert.hpp"
#include "utils/common.hpp"

using namespace std;

EquatorialAngle::EquatorialAngle() { enable_outfile = true; }

void EquatorialAngle::processFirstFrame(std::shared_ptr<Frame> &frame) {
    std::for_each(frame->atom_list.begin(), frame->atom_list.end(), [this](shared_ptr<Atom> &atom) {
        if (is_match(atom, this->ids1)) this->group1.insert(atom);
        if (is_match(atom, this->ids2)) this->group2.insert(atom);
        if (is_match(atom, this->ids3)) this->group3.insert(atom);
    });
}

void EquatorialAngle::process(std::shared_ptr<Frame> &frame) {
    for (auto &vec1_atom1 : group1) {
        for (auto &vec1_atom2 : group2) {
            if (vec1_atom1->molecule.lock() == vec1_atom2->molecule.lock()) {
                double xr = vec1_atom2->x - vec1_atom1->x;
                double yr = vec1_atom2->y - vec1_atom1->y;
                double zr = vec1_atom2->z - vec1_atom1->z;

                frame->image(xr, yr, zr);

                double leng1 = sqrt(xr * xr + yr * yr + zr * zr);

                for (auto &atom3 : group3) {
                    double xr2 = atom3->x - vec1_atom1->x;
                    double yr2 = atom3->y - vec1_atom1->y;
                    double zr2 = atom3->z - vec1_atom1->z;

                    frame->image(xr2, yr2, zr2);

                    double distance = sqrt(xr2 * xr2 + yr2 * yr2 + zr2 * zr2);

                    if (cutoff1 <= distance and distance < cutoff2) {
                        double _cos = (xr * xr2 + yr * yr2 + zr * zr2) / (distance * leng1);

                        double angle = acos(_cos) * radian;

                        hist.update(angle);
                    }
                }
            }
        }
    }
}

void EquatorialAngle::print(std::ostream &os) {
    os << string(50, '#') << '\n';
    os << "# " << EquatorialAngle::title() << '\n';
    os << "# Group1 > " << ids1 << '\n';
    os << "# Group2 > " << ids2 << '\n';
    os << "# Group3 > " << ids3 << '\n';
    os << "# angle_width(degree) > " << hist.getWidth() << '\n';
    os << "# Cutoff1(Ang) > " << cutoff1 << '\n';
    os << "# Cutoff2(Ang) > " << cutoff2 << '\n';
    os << string(50, '#') << '\n';
    os << format("#%15s %15s\n", "Angle(degree)", "Probability Density(% degree-1)");

    printData(os);

    os << string(50, '#') << '\n';
}

void EquatorialAngle::readInfo() {
    select1group(ids1, "Enter mask for atom1 : ");
    select1group(ids2, "Enter mask for atom2(in same mol with atom1) : ");
    select1group(ids3, "Enter mask for atom3 : ");

    auto angle_max = choose(0.0, 180.0, "Enter Maximum Angle to Accumulate[180.0 degree]:", Default(180.0));
    auto angle_width = choose(0.0, 180.0, "Enter Width of Angle Bins [0.5 degree]:", Default(0.5));

    cutoff1 = choose(0.0, 100.0, "Cutoff1 [Angstrom]:");
    cutoff2 = choose(0.0, 100.0, "Cutoff2 [Angstrom]:");

    throw_assert(cutoff1 < cutoff2, "Cutoff1 must less than Cutoff2");

    hist.initialize(angle_max, angle_width);
}

void EquatorialAngle::printData(std::ostream &os) const {
    for (auto [grid, value] : hist.getDistribution()) {
        os << format("%15.3f %15.3f\n", grid, 100 * value);
    }
}
