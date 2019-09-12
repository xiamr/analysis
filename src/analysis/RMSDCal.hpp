//
// Created by xiamr on 6/14/19.
//

#ifndef TINKER_RMSDCAL_HPP
#define TINKER_RMSDCAL_HPP

#include <map>
#include <unordered_set>

#include "common.hpp"
#include "BasicAnalysis.hpp"
#include "atom.hpp"

class RMSDCal : public BasicAnalysis {

    int steps = 0; // current frame number
    std::map<int, double> rmsd_map;
    bool first_frame = true;

    double x1[ATOM_MAX], y1[ATOM_MAX], z1[ATOM_MAX];
    double x2[ATOM_MAX], y2[ATOM_MAX], z2[ATOM_MAX];

    double rmsvalue(std::shared_ptr<Frame> &frame);

    AmberMask mask_for_superpose;
    AmberMask mask_for_rmscalc;

    std::unordered_set<std::shared_ptr<Atom>> atoms_for_superpose;
    std::unordered_set<std::shared_ptr<Atom>> atoms_for_rmscalc;

public:
    RMSDCal() {
        enable_outfile = true;
    }

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    static const std::string title() {
        return "RMSD Calculator";
    }

    static double rmsfit(double x1[], double y1[], double z1[],
                         double x2[], double y2[], double z2[], int n_rms_calc);

    static void jacobi(int n, double a[4][4], double d[], double v[4][4]);

    static void quatfit(int n1, double x1[], double y1[], double z1[],
                        int n_rms_calc, double x2[], double y2[], double z2[], int nfit);

    static void center(int n1, double x1[], double y1[], double z1[],
                       int n2, double x2[], double y2[], double z2[],
                       double mid[], int nfit);

    void save_frame_coord(double x[], double y[], double z[], const std::shared_ptr<Frame> &frame);
};

#endif //TINKER_RMSDCAL_HPP
