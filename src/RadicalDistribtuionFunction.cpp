//
// Created by xiamr on 6/14/19.
//

#include "RadicalDistribtuionFunction.hpp"
#include "frame.hpp"

void RadicalDistribtuionFunction::process(std::shared_ptr<Frame> &frame) {

    nframe++;
    xbox = frame->a_axis;
    ybox = frame->b_axis;
    zbox = frame->c_axis;

    for (auto &atom_j : group1) {
        double xj = atom_j->x;
        double yj = atom_j->y;
        double zj = atom_j->z;
        auto molj = atom_j->molecule.lock();

        for (auto &atom_k : group2) {
            if (atom_j != atom_k) {
                auto mol_k = atom_k->molecule.lock();
                if (intramol or (molj not_eq mol_k)) {
                    double dx = atom_k->x - xj;
                    double dy = atom_k->y - yj;
                    double dz = atom_k->z - zj;
                    frame->image(dx, dy, dz);
                    double rjk = sqrt(dx * dx + dy * dy + dz * dz);
                    int ibin = int(rjk / width) + 1;
                    if (ibin <= nbin) {
                        hist[ibin] += 1;
                    }
                }
            }
        }
    }
}

void RadicalDistribtuionFunction::readInfo() {

    Atom::select2group(ids1, ids2);
    rmax = choose(0.0, std::numeric_limits<double>::max(), "Enter Maximum Distance to Accumulate[10.0 Ang]:", true,
                  10.0);
    width = choose(0.0, std::numeric_limits<double>::max(), "Enter Width of Distance Bins [0.01 Ang]:", true, 0.01);
    auto inputline = input("Include Intramolecular Pairs in Distribution[N]:");
    boost::trim(inputline);
    if (!inputline.empty()) {
        if (boost::to_lower_copy(inputline) == "y") {
            intramol = true;
        }
    }
    nbin = int(rmax / width);
    for (int i = 0; i <= nbin; i++) {
        hist[i] = 0;
        gr[i] = gs[i] = 0.0;
    }

}

void RadicalDistribtuionFunction::print() {
    if (numj != 0 and numk != 0) {
        double factor = (4.0 / 3.0) * M_PI * nframe;
        int pairs = numj * numk;
        double volume = xbox * ybox * zbox;
        factor *= pairs / volume;
        for (int i = 1; i <= nbin; i++) {
            double rupper = i * width;
            double rlower = rupper - width;
            double expect = factor * (pow(rupper, 3) - pow(rlower, 3));
            gr[i] = hist[i] / expect;
            if (i == 1) {
                integral[i] = hist[i] / double(nframe);
            } else {
                integral[i] = hist[i] / double(nframe) + integral[i - 1];
            }
        }

    }

//     find the 5th degree polynomial smoothed distribution function

    if (nbin >= 5) {
        gs[1] = (69.0 * gr[1] + 4.0 * gr[2] - 6.0 * gr[3] + 4.0 * gr[4] - gr[5]) / 70.0;
        gs[2] = (2.0 * gr[1] + 27.0 * gr[2] + 12.0 * gr[3] - 8.0 * gr[4] + 2.0 * gr[5]) / 35.0;
        for (int i = 3; i <= nbin - 2; i++) {
            gs[i] = (-3.0 * gr[i - 2] + 12.0 * gr[i - 1] +
                     17.0 * gr[i] + 12.0 * gr[i + 1] - 3.0 * gr[i + 2]) / 35.0;
        }
        gs[nbin - 1] =
                (2.0 * gr[nbin - 4] - 8.0 * gr[nbin - 3] +
                 12.0 * gr[nbin - 2] + 27.0 * gr[nbin - 1] + 2.0 * gr[nbin]) / 35.0;
        gs[nbin] =
                (-gr[nbin - 4] + 4.0 * gr[nbin - 3] - 6.0 * gr[nbin - 2]
                 + 4.0 * gr[nbin - 1] + 69.0 * gr[nbin]) / 70.0;
        for (int i = 1; i <= nbin; i++) {
            gs[i] = std::max(0.0, gs[i]);
        }
    }

    outfile << "************************************************\n";
    outfile << "***** Pairwise Radial Distribution Function ****\n";

    outfile << "First Type : " << ids1 << " Second Type : " << ids2 << '\n';

    outfile << "************************************************\n";
    outfile << "Bin    Counts    Distance    Raw g(r)  Smooth g(r)   Integral\n";

    for (int i = 1; i <= nbin; i++) {
        outfile << boost::format("%d        %d      %.4f      %.4f     %.4f      %.4f\n") %
                   i % hist[i] % ((i - 0.5) * width) % gr[i] % gs[i] % integral[i];
    }

    outfile << "************************************************\n";
}

void RadicalDistribtuionFunction::processFirstFrame(std::shared_ptr<Frame> &frame) {
    std::for_each(frame->atom_list.begin(), frame->atom_list.end(),
                  [this](std::shared_ptr<Atom> &atom) {
                      if (Atom::is_match(atom, this->ids1)) this->group1.insert(atom);
                      if (Atom::is_match(atom, this->ids2)) this->group2.insert(atom);
                  });
    numj = group1.size();
    numk = group2.size();
}