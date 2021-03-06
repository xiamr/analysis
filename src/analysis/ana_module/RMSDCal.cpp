//
// Created by xiamr on 6/14/19.
//

#include "RMSDCal.hpp"

#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>

#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "nlohmann/json.hpp"
#include "utils/PBCUtils.hpp"

RMSDCal::RMSDCal() { enable_outfile = true; }

void RMSDCal::process(std::shared_ptr<Frame> &frame) { rmsds.push_back(rmsvalue(frame)); }

void RMSDCal::print(std::ostream &os) {
    if (writer) writer->close();
    os << std::string(50, '#') << '\n';
    os << "# " << title() << " # \n";
    os << "# AmberMask for superpose : " << mask_for_superpose << '\n';
    os << "# AmberMask for rms calc  : " << mask_for_rmscalc << '\n';
    os << std::string(50, '#') << '\n';
    os << boost::format("#%15s %15s\n") % "Frame" % "RMSD(Ang)";
    for (const auto &element : rmsds | boost::adaptors::indexed(1)) {
        os << boost::format(" %15d %15.8f\n") % element.index() % element.value();
    }
    os << std::string(50, '#') << '\n';
    os << ">>>JSON<<<\n";
    saveJson(os);
    os << "<<<JSON>>>\n";
}

void RMSDCal::saveJson(std::ostream &os) const {
    nlohmann::json json;
    json["title"] = title();
    json["AmberMask for superpose"] = to_string(mask_for_superpose);
    json["AmberMask for rms calc"] = to_string(mask_for_rmscalc);
    json["RMSD"] = {{"unit", "Ang"}, {"values", rmsds}};
    os << json;
}

void RMSDCal::readInfo() {
    select1group(mask_for_superpose, "Please enter atoms for superpose > ");
    select1group(mask_for_rmscalc, "Please enter atoms for rms calc  > ");
    if (choose_bool("Output suprposed structure [N] >", Default(false))) {
        writer = std::make_unique<XTCWriter>();
        writer->open(choose_file("Enter xtc filename for output superposed structures > ").extension("xtc"));
    }
}

double RMSDCal::rmsvalue(std::shared_ptr<Frame> &frame) {
    auto nfit = static_cast<int>(this->atoms_for_superpose.size());
    auto n_rms_calc = nfit + static_cast<int>(this->atoms_for_rmscalc.size());

    if (first_frame) {
        first_frame = false;
        update(frame);
        save_frame_coord(x1.get(), y1.get(), z1.get(), frame);
        double mid[3];
        center(n_rms_calc, x1.get(), y1.get(), z1.get(), mid, nfit);
        return 0.0;
    }
    save_frame_coord(x2.get(), y2.get(), z2.get(), frame);

    double mid[3];
    center(n_rms_calc, x2.get(), y2.get(), z2.get(), mid, nfit);

    quatfit(n_rms_calc, x1.get(), y1.get(), z1.get(), n_rms_calc, x2.get(), y2.get(), z2.get(), nfit);

    if (rmsds.size() == 1) save_superposed_frame(x1.get(), y1.get(), z1.get(), frame);

    save_superposed_frame(x2.get(), y2.get(), z2.get(), frame);
    return rmsfit(x1.get(), y1.get(), z1.get(), x2.get(), y2.get(), z2.get(), n_rms_calc);
}

void RMSDCal::save_frame_coord(double x[], double y[], double z[], const std::shared_ptr<Frame> &frame) {
    PBCUtils::move(mols, frame);
    for (const auto &element : join(atoms_for_superpose, atoms_for_rmscalc) | boost::adaptors::indexed()) {
        std::tie(x[element.index()], y[element.index()], z[element.index()]) = element.value()->getCoordinate();
    }
}

void RMSDCal::center(int n1, double x1[], double y1[], double z1[], double mid[], int nfit) {
    mid[0] = mid[1] = mid[2] = 0.0;
    double norm = 0.0;
    for (int i = 0; i < nfit; ++i) {
        mid[0] += x1[i];
        mid[1] += y1[i];
        mid[2] += z1[i];
        norm += 1.0;
    }
    mid[0] /= norm;
    mid[1] /= norm;
    mid[2] /= norm;
    for (int i = 0; i < n1; ++i) {
        x1[i] -= mid[0];
        y1[i] -= mid[1];
        z1[i] -= mid[2];
    }
}

void RMSDCal::quatfit(int /* n1 */, double x1[], double y1[], double z1[], int n_rms_calc, double x2[], double y2[],
                      double z2[], int nfit) {
    int i;
    //    int i1, i2;
    //    double weigh;
    double xrot, yrot, zrot;
    double xxyx, xxyy, xxyz;
    double xyyx, xyyy, xyyz;
    double xzyx, xzyy, xzyz;
    double q[4], d[4];
    double rot[3][3];
    double c[4][4], v[4][4];

    xxyx = 0.0;
    xxyy = 0.0;
    xxyz = 0.0;
    xyyx = 0.0;
    xyyy = 0.0;
    xyyz = 0.0;
    xzyx = 0.0;
    xzyy = 0.0;
    xzyz = 0.0;

    for (i = 0; i < nfit; ++i) {
        xxyx += x1[i] * x2[i];
        xxyy += y1[i] * x2[i];
        xxyz += z1[i] * x2[i];
        xyyx += x1[i] * y2[i];
        xyyy += y1[i] * y2[i];
        xyyz += z1[i] * y2[i];
        xzyx += x1[i] * z2[i];
        xzyy += y1[i] * z2[i];
        xzyz += z1[i] * z2[i];
    }

    c[0][0] = xxyx + xyyy + xzyz;
    c[0][1] = xzyy - xyyz;
    c[1][1] = xxyx - xyyy - xzyz;
    c[0][2] = xxyz - xzyx;
    c[1][2] = xxyy + xyyx;
    c[2][2] = xyyy - xzyz - xxyx;
    c[0][3] = xyyx - xxyy;
    c[1][3] = xzyx + xxyz;
    c[2][3] = xyyz + xzyy;
    c[3][3] = xzyz - xxyx - xyyy;

    jacobi(4, c, d, v);

    q[0] = v[0][3];
    q[1] = v[1][3];
    q[2] = v[2][3];
    q[3] = v[3][3];

    rot[0][0] = q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3];
    rot[1][0] = 2.0 * (q[1] * q[2] - q[0] * q[3]);
    rot[2][0] = 2.0 * (q[1] * q[3] + q[0] * q[2]);
    rot[0][1] = 2.0 * (q[2] * q[1] + q[0] * q[3]);
    rot[1][1] = q[0] * q[0] - q[1] * q[1] + q[2] * q[2] - q[3] * q[3];
    rot[2][1] = 2.0 * (q[2] * q[3] - q[0] * q[1]);
    rot[0][2] = 2.0 * (q[3] * q[1] - q[0] * q[2]);
    rot[1][2] = 2.0 * (q[3] * q[2] + q[0] * q[1]);
    rot[2][2] = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];

    for (i = 0; i < n_rms_calc; ++i) {
        xrot = x2[i] * rot[0][0] + y2[i] * rot[0][1] + z2[i] * rot[0][2];
        yrot = x2[i] * rot[1][0] + y2[i] * rot[1][1] + z2[i] * rot[1][2];
        zrot = x2[i] * rot[2][0] + y2[i] * rot[2][1] + z2[i] * rot[2][2];
        x2[i] = xrot;
        y2[i] = yrot;
        z2[i] = zrot;
    }
}

double RMSDCal::rmsfit(double x1[], double y1[], double z1[], double x2[], double y2[], double z2[], int n_rms_calc) {
    double fit;
    double xr, yr, zr, dist2;
    double norm;

    fit = 0.0;
    norm = 0.0;
    for (int i = 0; i < n_rms_calc; ++i) {
        xr = x1[i] - x2[i];
        yr = y1[i] - y2[i];
        zr = z1[i] - z2[i];
        dist2 = xr * xr + yr * yr + zr * zr;
        norm += 1.0;
        fit += dist2;
    }
    return sqrt(fit / norm);
}

double RMSDCal::rms_max(double x1[], double y1[], double z1[], double x2[], double y2[], double z2[], int n_rms_calc) {
    double xr, yr, zr, dist2;
    double rms2 = 0.0;
    for (int i = 0; i < n_rms_calc; ++i) {
        xr = x1[i] - x2[i];
        yr = y1[i] - y2[i];
        zr = z1[i] - z2[i];
        dist2 = xr * xr + yr * yr + zr * zr;
        rms2 = std::max(rms2, dist2);
    }
    return std::sqrt(rms2);
}

void RMSDCal::jacobi(int n, double a[4][4], double d[], double v[4][4]) {
    // taken from tinker

    int i, j, k;
    int ip, iq;
    int nrot, maxrot;
    double sm, tresh, s, c, t;
    double theta, tau, h, g, p;

    double b[4];
    double z[4];

    maxrot = 100;
    nrot = 0;
    for (ip = 0; ip < n; ++ip) {
        for (iq = 0; iq < n; ++iq) {
            v[ip][iq] = 0.0;
        }
        v[ip][ip] = 1.0;
    }
    for (ip = 0; ip < n; ++ip) {
        b[ip] = a[ip][ip];
        d[ip] = b[ip];
        z[ip] = 0.0;
    }

    //  perform the jacobi rotations

    for (i = 0; i < maxrot; ++i) {
        sm = 0.0;
        for (ip = 0; ip < n - 1; ++ip) {
            for (iq = ip + 1; iq < n; ++iq) {
                sm += abs(a[ip][iq]);
            }
        }
        if (sm == 0.0) goto label_10;
        if (i < 3) {
            tresh = 0.2 * sm / (n * n);
        } else {
            tresh = 0.0;
        }
        for (ip = 0; ip < n - 1; ++ip) {
            for (iq = ip + 1; iq < n; ++iq) {
                g = 100.0 * abs(a[ip][iq]);
                if (i > 3 && abs(d[ip]) + g == abs(d[ip]) && abs(d[iq]) + g == abs(d[iq]))
                    a[ip][iq] = 0.0;
                else if (abs(a[ip][iq]) > tresh) {
                    h = d[iq] - d[ip];
                    if (abs(h) + g == abs(h))
                        t = a[ip][iq] / h;
                    else {
                        theta = 0.5 * h / a[ip][iq];
                        t = 1.0 / (abs(theta) + sqrt(1.0 + theta * theta));
                        if (theta < 0.0) t = -t;
                    }
                    c = 1.0 / sqrt(1.0 + t * t);
                    s = t * c;
                    tau = s / (1.0 + c);
                    h = t * a[ip][iq];
                    z[ip] -= h;
                    z[iq] += h;
                    d[ip] -= h;
                    d[iq] += h;
                    a[ip][iq] = 0.0;
                    for (j = 0; j <= ip - 1; ++j) {
                        g = a[j][ip];
                        h = a[j][iq];
                        a[j][ip] = g - s * (h + g * tau);
                        a[j][iq] = h + s * (g - h * tau);
                    }
                    for (j = ip + 1; j <= iq - 1; ++j) {
                        g = a[ip][j];
                        h = a[j][iq];
                        a[ip][j] = g - s * (h + g * tau);
                        a[j][iq] = h + s * (g - h * tau);
                    }
                    for (j = iq + 1; j < n; ++j) {
                        g = a[ip][j];
                        h = a[iq][j];
                        a[ip][j] = g - s * (h + g * tau);
                        a[iq][j] = h + s * (g - h * tau);
                    }
                    for (j = 0; j < n; ++j) {
                        g = v[j][ip];
                        h = v[j][iq];
                        v[j][ip] = g - s * (h + g * tau);
                        v[j][iq] = h + s * (g - h * tau);
                    }
                    ++nrot;
                }
            }
        }
        for (ip = 0; ip < n; ++ip) {
            b[ip] += z[ip];
            d[ip] = b[ip];
            z[ip] = 0.0;
        }
    }

label_10:

    if (nrot == maxrot) std::cerr << " JACOBI  --  Matrix Diagonalization not Converged" << std::endl;

    for (i = 0; i < n - 1; ++i) {
        k = i;
        p = d[i];
        for (j = i + 1; j < n; ++j) {
            if (d[j] < p) {
                k = j;
                p = d[j];
            }
        }
        if (k != i) {
            d[k] = d[i];
            d[i] = p;
            for (j = 0; j < n; ++j) {
                p = v[j][i];
                v[j][i] = v[j][k];
                v[j][k] = p;
            }
        }
    }
}

void RMSDCal::processFirstFrame(std::shared_ptr<Frame> &frame) {
    boost::for_each(frame->atom_list, [this](std::shared_ptr<Atom> &atom) {
        if (is_match(atom, mask_for_superpose)) {
            atoms_for_superpose.insert(atom);
        } else if (is_match(atom, mask_for_rmscalc)) {
            atoms_for_rmscalc.insert(atom);
        }
    });
    auto n_size = atoms_for_superpose.size() + atoms_for_rmscalc.size();

    x1 = std::make_unique<double[]>(n_size);
    y1 = std::make_unique<double[]>(n_size);
    z1 = std::make_unique<double[]>(n_size);
    x2 = std::make_unique<double[]>(n_size);
    y2 = std::make_unique<double[]>(n_size);
    z2 = std::make_unique<double[]>(n_size);
}

void RMSDCal::save_superposed_frame(double *x, double *y, double *z, const std::shared_ptr<Frame> &frame) {
    if (!writer) return;
    for (const auto &element : join(atoms_for_superpose, atoms_for_rmscalc) | boost::adaptors::indexed()) {
        element.value()->x = x[element.index()];
        element.value()->y = y[element.index()];
        element.value()->z = z[element.index()];
    }
    writer->write(frame);
}

void RMSDCal::update(const std::shared_ptr<Frame> &frame) {
    mols = PBCUtils::calculate_intermol(join(atoms_for_superpose, atoms_for_rmscalc), frame);
}

void RMSDCal::setParameters(const AmberMask &mask_for_superpose, const AmberMask &mask_for_rmscalc,
                            const std::string &out) {
    this->mask_for_superpose = mask_for_superpose;
    this->mask_for_rmscalc = mask_for_rmscalc;
    
    outfilename = out;
    boost::trim(outfilename);
    if (outfilename.empty()) {
        throw std::runtime_error("outfilename cannot empty");
    }
}