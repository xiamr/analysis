#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_set>
#include <memory>
#include <cmath>
#include <tbb/tbb.h>
#include <readline/readline.h>
#include <tbb/scalable_allocator.h>
#include <alloca.h>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <string>
#include <unistd.h>
#include <functional>

using namespace std;


#include <Eigen/Eigen>

namespace gmx {

#include "gromacs/fileio/xtcio.h"
#include "gromacs/fileio/trnio.h"
#include "gromacs/utility/smalloc.h"

}


#include "AmberNetcdf.h"
#include "amber_netcdf.h"
#include "gmxtrr.h"

#define BOOST_RESULT_OF_USE_DECLTYPE
#define BOOST_SPIRIT_USE_PHOENIX_V3

#include <boost/program_options.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

namespace po = boost::program_options;


#include <boost/format.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/support.hpp>
#include <boost/spirit/include/lex.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include <boost/variant.hpp>

const int ATOM_MAX = 10000;

// global variables

bool enable_read_velocity = false;
bool enable_tbb = false;
bool enable_outfile = false;

std::fstream outfile;


void *operator new(size_t size) noexcept(false) {
    if (size == 0) size = 1;
    if (void *ptr = scalable_malloc(size))
        return ptr;
    throw std::bad_alloc();
}

void *operator new[](size_t size) noexcept(false) {
    return operator new(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept {
    if (size == 0) size = 1;
    if (void *ptr = scalable_malloc(size))
        return ptr;
    return nullptr;
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept {
    return operator new(size, std::nothrow);
}

void operator delete(void *ptr) throw() {
    if (ptr != nullptr) scalable_free(ptr);
}

void operator delete[](void *ptr) throw() {
    operator delete(ptr);
}

void operator delete(void *ptr, const std::nothrow_t &) throw() {
    if (ptr != nullptr) scalable_free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) throw() {
    operator delete(ptr, std::nothrow);
}

bool file_exist(const string &name) {
    fstream in(name, ofstream::in);
    return in.good();
}

std::vector<std::string> split(const std::string &str, char sep = ' ') {
    std::vector<std::string> ret_;
    if (str.empty()) {
        return ret_;
    }
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = 0;
    std::string::size_type i = 0;

    bool prev = false;
    for (auto &ch : str) {
        if (ch != sep && ch != '\t') {
            if (!prev)
                pos1 = i;
            prev = true;
        } else {
            if (prev) {
                pos2 = i;
                prev = false;
                ret_.push_back(str.substr(pos1, pos2 - pos1));
            }
        }
        i++;
    }
    if (prev) ret_.push_back(str.substr(pos1, str.size() - pos1));

    return ret_;
}

std::string &trim(std::string &str) {
    if (str.empty()) {
        return str;
    }
    str.erase(0, str.find_first_not_of(' '));
    str.erase(str.find_last_not_of(' ') + 1);
    return str;
}

std::string &str_toupper(string &str) {
    for (auto &c: str) c = toupper(c);
    return str;
}

std::string &str_tolower(string &str) {
    for (auto &c: str) c = tolower(c);
    return str;
}

string input(const string &prompt = "") {
    std::cout << prompt;
    string inputline;
    std::getline(std::cin, inputline);
    if (isatty(STDIN_FILENO) == 0) {
        std::cout << inputline << std::endl;
    }
    return inputline;
}

int choose(int min, int max, const string &prompt, bool hasdefault = false, int value = 0) {
    while (true) {
        string input_line = input(prompt);
        trim(input_line);
        if (input_line.empty()) {
            if (!hasdefault) continue;
            return value;
        }
        try {
            int option = stoi(input_line);
            if (option >= min and option <= max) return option;

            cerr << fmt::sprintf("must be a integer range %d and %d ! please retype!\n", min, max);
        } catch (std::invalid_argument) {
            cerr << "must be a integer ! please retype!" << endl;
        }
    }
}

double choose(double min, double max, const string &prompt, bool hasdefault = false, double value = 0.0) {
    while (true) {
        string input_line = input(prompt);
        trim(input_line);
        if (input_line.empty()) {
            if (!hasdefault) continue;
            return value;
        }
        try {
            double option = stod(input_line);
            if (option > min and option <= max) return option;

            cerr << fmt::sprintf("must be a float range %f and %f ! please retype!\n", min, max);
        } catch (std::invalid_argument) {
            cerr << "invalid_argument ! please retype!" << endl;
        }
    }
}


string ext_filename(const string &filename) {
    auto field = split(filename, '.');
    return str_tolower(field[field.size() - 1]);
}

string choose_file(const string &prompt, bool exist, string ext = "", bool canempty = false) {
    while (true) {
        string input_line = input(prompt);
        trim(input_line);
        if (!input_line.empty()) {
            if (ext.length()) {
                if (ext_filename(input_line) != str_tolower(ext)) {
                    cerr << "wrong file extesion name : must be " << ext << endl;
                    continue;
                }
            }
            if (!exist) return input_line;
            fstream in(input_line, ofstream::in);
            if (in.good()) {
                return input_line;
                break;
            } else {
                cerr << "The file is bad [retype]" << endl;
            }
        }
        if (canempty) return "";
    }
}


template<typename T>
T sign(T &x, T &y) { return y > 0 ? std::abs(x) : -std::abs(x); }

class Molecule;

class Atom {
public:
    int seq;
    std::string symbol;

    double x, y, z;  // position

    double vx = 0.0, vy = 0.0, vz = 0.0; // velocity

    int typ; // atom type
    std::string type_name;

    double charge;

    std::list<int> con_list; // atom num that connect to

    std::weak_ptr<Molecule> molecule;

    bool mark = false; // used in NMR analysis

    bool adj(std::shared_ptr<Atom> &atom) {
        for (int i : con_list) {
            if (atom->seq == i) return true;
        }
        return false;
    }

    enum class SelectType {
        Symbol, TypeName, TypeNumber
    };

    struct Atom_Indenter_Symbol {
        std::string name;
    };
    struct Atom_Indenter_TypeName {
        std::string name;
    };
    struct Atom_Indenter_TypeNumber {
        int type_number;
    };
    typedef boost::variant<Atom_Indenter_Symbol, Atom_Indenter_TypeName, Atom_Indenter_TypeNumber> AtomIndenter;

    static bool is_match(std::shared_ptr<Atom> &atom, const std::vector<AtomIndenter> &ids);

    static bool is_match(std::shared_ptr<Atom> &atom, const AtomIndenter &id);

    static void select2group(std::vector<AtomIndenter> &ids1, std::vector<AtomIndenter> &ids2);
};

ostream &operator<<(ostream &out, std::vector<Atom::AtomIndenter> &ids) {
    struct Output : public boost::static_visitor<> {
        ostream &out;
    public:
        Output(ostream &out) : out(out) {}

        void operator()(Atom::Atom_Indenter_Symbol &t) const {
            out << "S:" << t.name << " ";
        }

        void operator()(Atom::Atom_Indenter_TypeName &t) const {
            out << "T:" << t.name << " ";
        }

        void operator()(Atom::Atom_Indenter_TypeNumber &t) const {
            out << "N:" << t.type_number << " ";
        }
    } output(out);

    for (auto &id : ids) {
        boost::apply_visitor(output, id);
    }
    return out;
}

void Atom::select2group(std::vector<AtomIndenter> &ids1, std::vector<AtomIndenter> &ids2) {
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;
    using ascii::char_;

    auto action = [](auto &v, auto ids, auto &message, auto type) {
        std::string result(v.begin(), v.end());
        std::cout << "<" << message << ":" << result << ">" << std::endl;
        switch (type) {
            case SelectType::Symbol:
                const_cast<std::vector<AtomIndenter> *>(ids)->emplace_back(Atom_Indenter_Symbol{result});
                break;
            case SelectType::TypeName:
                const_cast<std::vector<AtomIndenter> *>(ids)->emplace_back(Atom_Indenter_TypeName{result});
                break;
            case SelectType::TypeNumber:
                const_cast<std::vector<AtomIndenter> *>(ids)->emplace_back(
                        Atom_Indenter_TypeNumber{boost::lexical_cast<int>(result)});
                break;
        }

    };


    for (;;) {
        const string &line = input("Enter mask (expample: name{Am} && type{OW}) : ");
        if (qi::phrase_parse(line.cbegin(), line.cend(),
                             +((qi::lit("name") >> "{" >> +(qi::lexeme[+(char_ - qi::space - '}')][
                                     boost::phoenix::bind(action, qi::_1, &ids1, "mask1:name", SelectType::Symbol)
                             ]) >> "}")
                               | (qi::lit("type") >> "{" >> +(qi::lexeme[+(char_ - qi::space - '}')][
                                     boost::phoenix::bind(action, qi::_1, &ids1, "mask1:type",
                                                          SelectType::TypeName)
                             ]) >> "}")
                               | (qi::lit("typenumber") >> "{" >> +(qi::lexeme[+(char_ - qi::space - '}')][
                                     boost::phoenix::bind(action, qi::_1, &ids1, "mask1:typenumber",
                                                          SelectType::TypeNumber)
                             ]) >> "}"))
                                     >> qi::lit("&&")
                                     >> +((qi::lit("name") >> "{" >> +(qi::lexeme[+(char_ - qi::space - '}')][
                                             boost::phoenix::bind(action, qi::_1, &ids2, "mask2:name",
                                                                  SelectType::Symbol)
                                     ]) >> "}")
                                          | (qi::lit("type") >> "{" >> +(qi::lexeme[+(char_ - qi::space - '}')][
                                             boost::phoenix::bind(action, qi::_1, &ids2, "mask2:type",
                                                                  SelectType::TypeName)
                                     ]) >> "}")
                                          |
                                          (qi::lit("typenumber") >> "{" >> +(qi::lexeme[+(char_ - qi::space - '}')][
                                                  boost::phoenix::bind(action, qi::_1, &ids2, "mask2:typenumber",
                                                                       SelectType::TypeNumber)
                                          ]) >> "}")),
                             ascii::space)) {
            break;
        }
        std::cout << "Syntax error , Please Reenter !" << std::endl;
        ids1.clear();
        ids2.clear();
    }
    std::cout << "First Type : " << ids1 << " Second Type : " << ids2 << std::endl;
}

bool Atom::is_match(std::shared_ptr<Atom> &atom, const std::vector<AtomIndenter> &ids) {

    struct Equal : public boost::static_visitor<bool> {
    public:
        Equal(shared_ptr<Atom> &atom) : atom(atom) {}

        bool operator()(const Atom::Atom_Indenter_Symbol &t) const {
            return atom->symbol == t.name;
        }

        bool operator()(const Atom::Atom_Indenter_TypeName &t) const {
            return atom->type_name == t.name;
        }

        bool operator()(const Atom::Atom_Indenter_TypeNumber &t) const {
            return atom->typ == t.type_number;
        }

    private:
        shared_ptr<Atom> &atom;
    };
    for (auto &id : ids) {
        if (boost::apply_visitor(Equal(atom), id)) return true;
    }
    return false;
}

bool Atom::is_match(std::shared_ptr<Atom> &atom, const AtomIndenter &id) {

    struct Equal : public boost::static_visitor<bool> {
    public:
        Equal(shared_ptr<Atom> &atom) : atom(atom) {}

        bool operator()(const Atom::Atom_Indenter_Symbol &t) const {
            return atom->symbol == t.name;
        }

        bool operator()(const Atom::Atom_Indenter_TypeName &t) const {
            return atom->type_name == t.name;
        }

        bool operator()(const Atom::Atom_Indenter_TypeNumber &t) const {
            return atom->typ == t.type_number;
        }

    private:
        shared_ptr<Atom> &atom;
    };
    return boost::apply_visitor(Equal(atom), id);

}


class Frame;

class Molecule {
public:
    double mass;  // molecular mass
    std::list<std::shared_ptr<Atom>> atom_list;

    double center_x, center_y, center_z;

    bool bExculde = false;

    void calc_center(shared_ptr<Frame> &frame);

    void calc_mass();

    tuple<double, double, double> calc_weigh_center(shared_ptr<Frame> &frame);

    tuple<double, double, double> calc_dipole(shared_ptr<Frame> &frame);

    /**
     * @return return seq of first atom for temporary use
     *         else return 0
     */
    int seq() {
        if (atom_list.empty()) return 0;
        return atom_list.front()->seq;
    }

    shared_ptr<Atom> ow;
};


class Frame {
public:
    double a_axis = 0.0;
    double b_axis = 0.0;
    double c_axis = 0.0;

    double a_axis_half = 0.0;
    double b_axis_half = 0.0;
    double c_axis_half = 0.0;

    double alpha = 0.0;
    double beta = 0.0;
    double gamma = 0.0;

    std::string title;

    std::list<std::shared_ptr<Atom>> atom_list;
    std::map<int, std::shared_ptr<Atom>> atom_map;

    std::list<std::shared_ptr<Molecule>> molecule_list;

    bool enable_bound = false;

    void image(double &xr, double &yr, double &zr) {
        if (!enable_bound) return;
        while (std::abs(xr) > a_axis_half) xr -= sign(a_axis, xr);
        while (std::abs(yr) > b_axis_half) yr -= sign(b_axis, yr);
        while (std::abs(zr) > c_axis_half) zr -= sign(c_axis, zr);
    }
};


double atom_distance(std::shared_ptr<Atom> &atom1, std::shared_ptr<Atom> &atom2, std::shared_ptr<Frame> &frame) {
    auto xr = atom1->x - atom2->x;
    auto yr = atom1->y - atom2->y;
    auto zr = atom1->z - atom2->z;
    frame->image(xr, yr, zr);
    return std::sqrt(xr * xr + yr * yr + zr * zr);
}


void add_to_mol(std::shared_ptr<Atom> &atom, std::shared_ptr<Molecule> &mol, std::shared_ptr<Frame> &frame) {
    if (atom->molecule.lock()) return;
    mol->atom_list.push_back(atom);
    atom->molecule = mol;
    for (auto &i : atom->con_list)
        add_to_mol(frame->atom_map[i], mol, frame);
}

class TrajectoryReader {
    std::fstream position_file;
    std::fstream velocity_file; // The velocity file

    bool isbin = false;
    bool isnetcdf = false;
    bool istrr = false;
    bool isxtc = false;

    bool enable_binaray_file = false;
    string topology_filename;

    enum class TOPOLOGY_TYPE {
        ARC, MOL2
    } topology_type;


    struct AmberNetcdf NC;
    std::shared_ptr<Frame> frame;

    gmx::t_fileio *fio = nullptr;

    bool first_time = true;
    bool openvel = false;

    int natoms, step;
    gmx::rvec *x = nullptr;
    gmx::real prec, time;

    std::list<std::string> arc_filename_list; // the continuous trajectory files



    void close() {
        if (isnetcdf) {
            netcdfClose(&NC);
            isnetcdf = false;
        } else if (istrr) {
            gmx::close_trn(fio);
            fio = nullptr;
            istrr = false;
        } else if (isxtc) {
            gmx::sfree(x);
            x = nullptr;
            gmx::close_xtc(fio);
            fio = nullptr;

            isxtc = false;
        } else {
            position_file.close();
        }
        if (velocity_file.is_open()) velocity_file.close();
    }

    void readOneFrameVel() {
        std::string line;
        std::vector<std::string> field;
        getline(velocity_file, line);
        for (auto &atom : frame->atom_list) {
            std::getline(velocity_file, line);
            field = split(line);
            auto len1 = field[2].length();
            auto len2 = field[3].length();
            auto len3 = field[4].length();
            atom->vx = std::stod(field[2].replace(len1 - 4, 1, "E"));
            atom->vy = std::stod(field[3].replace(len2 - 4, 1, "E"));
            atom->vz = std::stod(field[4].replace(len3 - 4, 1, "E"));
        }
    }

    shared_ptr<Frame> readOneFrameTraj() {
        char str[256];
        if (!frame) {
            frame = make_shared<Frame>();
            int length;
            position_file.read((char *) &length, 4);
            int atom_num;
            position_file.read((char *) &atom_num, 4);
            position_file.read(str, length - 4);
            frame->title = std::string(str, static_cast<unsigned long>(length - 4));
            for (int i = 0; i < atom_num; i++) {
                auto atom_ptr = make_shared<Atom>();
                position_file.read((char *) &length, 4);
                position_file.read((char *) &atom_ptr->seq, 4);
                position_file.read((char *) &atom_ptr->typ, 4);
                position_file.read((char *) &str, length - 8);
                atom_ptr->symbol = std::string(str, static_cast<unsigned long>(length - 8));
                int num;
                position_file.read((char *) &num, 4);
                int n;
                for (int k = 0; k < num; k++) {
                    position_file.read((char *) &n, 4);
                    atom_ptr->con_list.push_back(n);
                }

                frame->atom_list.push_back(atom_ptr);
                frame->atom_map[atom_ptr->seq] = atom_ptr;
            }
            for (auto &atom : frame->atom_list) {
                if (!atom->molecule.lock()) {
                    auto molecule = std::make_shared<Molecule>();
                    add_to_mol(atom, molecule, frame);
                    frame->molecule_list.push_back(molecule);
                }
            }
        }
        float box[6];
        position_file.read((char *) box, 24);
        frame->a_axis = box[0];
        frame->b_axis = box[1];
        frame->c_axis = box[2];
        frame->alpha = box[3];
        frame->beta = box[4];
        frame->gamma = box[5];
        for (auto &atom : frame->atom_list) {
            float vect[3];
            position_file.read((char *) vect, 12);
            atom->x = vect[0];
            atom->y = vect[1];
            atom->z = vect[2];
        }
        frame->a_axis_half = frame->a_axis / 2;
        frame->b_axis_half = frame->b_axis / 2;
        frame->c_axis_half = frame->c_axis / 2;

        return frame;


    }

    int readOneFrameNetCDF() {
        auto coord = new double[NC.ncatom3];
        double box[6];
        int ret = netcdfGetNextFrame(&NC, coord, box);
        if (ret == 0) return ret;
        int i = 0;
        for (auto &atom : frame->atom_list) {
            atom->x = coord[3 * i];
            atom->y = coord[3 * i + 1];
            atom->z = coord[3 * i + 2];
            i++;
        }
        frame->a_axis = box[0];
        frame->b_axis = box[1];
        frame->c_axis = box[2];
        frame->alpha = box[3];
        frame->beta = box[4];
        frame->gamma = box[5];

        if (frame->enable_bound) {
            frame->a_axis_half = frame->a_axis / 2;
            frame->b_axis_half = frame->b_axis / 2;
            frame->c_axis_half = frame->c_axis / 2;
        }

        delete[] coord;
        return ret;
    }

    int readOneFrameTrr() {
        gmx::t_trnheader trnheader;
        gmx::gmx_bool bOK;
        gmx::rvec box[3];
        gmx::rvec *coord = nullptr;
        if (gmx::fread_trnheader(fio, &trnheader, &bOK)) {
            if (bOK) {
                coord = new gmx::rvec[trnheader.natoms];
                if (trnheader.box_size) {
                    gmx::fread_htrn(fio, &trnheader, box, coord, NULL, NULL);
                    translate(box, &(frame->a_axis), &(frame->b_axis), &(frame->c_axis),
                              &(frame->alpha), &(frame->beta), &(frame->gamma));
                    if (frame->enable_bound) {
                        frame->a_axis_half = frame->a_axis / 2;
                        frame->b_axis_half = frame->b_axis / 2;
                        frame->c_axis_half = frame->c_axis / 2;
                    }
                } else {
                    if (frame->enable_bound) {
                        cerr << "WARING ! then trajectory have PBC enabled" << endl;
                        exit(1);
                    }
                    frame->a_axis = 0.0;
                    frame->b_axis = 0.0;
                    frame->c_axis = 0.0;
                    frame->alpha = 0.0;
                    frame->beta = 0.0;
                    frame->gamma = 0.0;
                    frame->enable_bound = false;
                    gmx::fread_htrn(fio, &trnheader, NULL, coord, NULL, NULL);
                }
                if (frame->atom_list.size() != trnheader.natoms) {
                    cerr << "ERROR! the atom number do not match" << endl;
                    exit(1);
                }
                int i = 0;
                for (auto &atom : frame->atom_list) {
                    atom->x = coord[i][0] * 10;
                    atom->y = coord[i][1] * 10;
                    atom->z = coord[i][2] * 10;
                    i++;
                }
                delete[] coord;
                return 1;
            }
        }
        return 0;

    }

    int readOneFrameXtc() {
        gmx::matrix box;
        gmx::gmx_bool bOK;

        int ret;
        if (!x) {
            ret = gmx::read_first_xtc(fio, &natoms, &step, &time, box, &x, &prec, &bOK);
        } else {
            ret = gmx::read_next_xtc(fio, natoms, &step, &time, box, x, &prec, &bOK);
        }
        if (!ret) return 0;

        if (bOK) {
            if (natoms != frame->atom_list.size()) {
                cerr << "ERROR! the atom number do not match" << endl;
                exit(1);
            }
            if (frame->enable_bound) {
                translate(box, &(frame->a_axis), &(frame->b_axis), &(frame->c_axis),
                          &(frame->alpha), &(frame->beta), &(frame->gamma));
                frame->a_axis_half = frame->a_axis / 2;
                frame->b_axis_half = frame->b_axis / 2;
                frame->c_axis_half = frame->c_axis / 2;
            }
            int i = 0;
            for (auto &atom : frame->atom_list) {
                atom->x = x[i][0] * 10;
                atom->y = x[i][1] * 10;
                atom->z = x[i][2] * 10;
                i++;
            }
            return 1;
        }
        cerr << fmt::sprintf("\nWARNING: Incomplete frame at time %g\n", time);
        return 0;
    }

    shared_ptr<Frame> readOneFrameMol2() {
        std::string line;
        std::vector<std::string> fields;

        enum class State {
            NONE, MOLECULE, ATOM, BOND, SUBSTRUCTURE
        } state;
        state = State::NONE;
        auto whitespace_regex = boost::regex("\\s+");
        // watch out memery leak

        if (!frame) {
            frame = std::make_shared<Frame>();
            if (enable_binaray_file) frame->enable_bound = true;
        }

        auto iter = frame->atom_list.begin();

        for (;;) {
            std::getline(position_file, line);
            boost::trim(line);
            if (line.empty()) continue;
            fields.clear();

            bool break_loop = false;

            if (boost::starts_with(line, "@<TRIPOS>MOLECULE")) {
                state = State::MOLECULE;
            } else if (boost::starts_with(line, "@<TRIPOS>ATOM")) {
                state = State::ATOM;
            } else if (boost::starts_with(line, "@<TRIPOS>BOND")) {
                state = State::BOND;
            } else if (boost::starts_with(line, "@<TRIPOS>SUBSTRUCTURE")) {
                state = State::SUBSTRUCTURE;
            } else {
                switch (state) {
                    case State::MOLECULE:
                        continue;
                    case State::ATOM: {
                        boost::regex_split(std::back_inserter(fields), line, whitespace_regex);
                        shared_ptr<Atom> atom;
                        if (first_time) {
                            atom = std::make_shared<Atom>();
                            atom->seq = boost::lexical_cast<int>(fields[0]);
                            atom->symbol = fields[1];
                            atom->type_name = fields[5];
                            atom->charge = boost::lexical_cast<double>(fields[8]);
                            frame->atom_list.push_back(atom);
                            frame->atom_map[atom->seq] = atom;
                        } else {
                            atom = *iter;
                            ++iter;
                        }
                        atom->x = boost::lexical_cast<double>(fields[2]);
                        atom->y = boost::lexical_cast<double>(fields[3]);
                        atom->z = boost::lexical_cast<double>(fields[4]);
                    }
                        break;
                    case State::BOND: {
                        if (!first_time) {
                            break_loop = true;
                            break;
                        }
                        boost::regex_split(std::back_inserter(fields), line, whitespace_regex);
                        int atom_num1 = boost::lexical_cast<int>(fields[1]);
                        int atom_num2 = boost::lexical_cast<int>(fields[2]);

                        auto atom1 = frame->atom_map[atom_num1];
                        auto atom2 = frame->atom_map[atom_num2];

                        atom1->con_list.push_back(atom_num2);
                        atom2->con_list.push_back(atom_num1);
                    }
                        break;
                    case State::SUBSTRUCTURE:
                        break_loop = true;
                        break;
                    default:
                        break;
                }
            }
            if (break_loop) {
                break;
            }
        }
        if (first_time) {
            for (auto &atom : frame->atom_list) {
                if (!atom->molecule.lock()) {
                    auto molecule = std::make_shared<Molecule>();
                    add_to_mol(atom, molecule, frame);
                    frame->molecule_list.push_back(molecule);
                }
            }
        }
        return frame;

    }

    shared_ptr<Frame> readOneFrameArc() {
        int atom_num = 0;
        std::string line;
        std::vector<std::string> field;
        if (!frame) {
            frame = std::make_shared<Frame>();
            std::getline(position_file, line);
            field = split(line);
            atom_num = std::stoi(field[0]);
            frame->title = line.substr(line.rfind(field[0]) + field[0].size());
            trim(frame->title);
        } else {
            std::getline(position_file, line);
            if (line.empty()) throw std::exception();
        }

        if (frame->enable_bound) {
            std::getline(position_file, line);
            field = split(line);
            if (field.empty()) throw std::exception();
            frame->a_axis = std::stod(field[0]);
            frame->b_axis = std::stod(field[1]);
            frame->c_axis = std::stod(field[2]);
            frame->alpha = std::stod(field[3]);
            frame->beta = std::stod(field[4]);
            frame->gamma = std::stod(field[5]);
        }

        if (first_time) {
            bool first_loop = true;
            for (int i = 0; i < atom_num; i++) {
                std::getline(position_file, line);
                field = split(line);
                if (first_loop) {
                    first_loop = false;
                    if (field.empty()) throw std::exception();
                    try {
                        std::stod(field[1]);
                        frame->a_axis = std::stod(field[0]);
                        frame->b_axis = std::stod(field[1]);
                        frame->c_axis = std::stod(field[2]);
                        frame->alpha = std::stod(field[3]);
                        frame->beta = std::stod(field[4]);
                        frame->gamma = std::stod(field[5]);
                        frame->enable_bound = true;
                        i--;
                        continue;
                    } catch (std::exception &e) {

                    }
                }

                auto atom = std::make_shared<Atom>();
                atom->seq = std::stoi(field[0]);
                atom->symbol = field[1];
                atom->x = std::stod(field[2]);
                atom->y = std::stod(field[3]);
                atom->z = std::stod(field[4]);
                atom->typ = std::stoi(field[5]);
                for (size_t j = 6; j < field.size(); j++) {
                    atom->con_list.push_back(std::stoi(field[j]));
                }
                frame->atom_list.push_back(atom);
                frame->atom_map[atom->seq] = atom;
            }

            for (auto &atom : frame->atom_list) {
                if (!atom->molecule.lock()) {
                    auto molecule = std::make_shared<Molecule>();
                    add_to_mol(atom, molecule, frame);
                    frame->molecule_list.push_back(molecule);
                }
            }

        } else {
            for (auto &atom : frame->atom_list) {
                std::getline(position_file, line);
                field = split(line);
                atom->x = std::stod(field[2]);
                atom->y = std::stod(field[3]);
                atom->z = std::stod(field[4]);
            }
        }
        if (frame->enable_bound) {
            frame->a_axis_half = frame->a_axis / 2;
            frame->b_axis_half = frame->b_axis / 2;
            frame->c_axis_half = frame->c_axis / 2;
        }

        return frame;
    }


    void open(const string &filename) {
        auto field = split(filename, '.');
        auto ext = ext_filename(filename);
        if (enable_binaray_file) {
            if (ext == "nc") {
                if (netcdfLoad(&NC, filename.c_str())) {
                    cerr << "error open NETCDF file: " << filename << endl;
                    exit(2);
                }
                isnetcdf = true;
                istrr = false;
                isxtc = false;
            } else if (ext == "trr") {
                fio = gmx::open_trn(filename.c_str(), "r");
                isnetcdf = false;
                istrr = true;
                isxtc = false;

            } else if (ext == "xtc") {
                fio = gmx::open_xtc(filename.c_str(), "r");
                isnetcdf = false;
                istrr = false;
                isxtc = true;
            }
        } else if (ext == "traj") {
            isbin = true;
            position_file.exceptions(fstream::eofbit | fstream::failbit | fstream::badbit);
            position_file.open(filename, std::fstream::in | std::fstream::binary);
        } else {
            isbin = false;
            position_file.open(filename);
        }
        if (enable_read_velocity) {
            velocity_file.open(field[0] + ".vel");
            velocity_file.exceptions(fstream::eofbit | fstream::failbit | fstream::badbit);
            if (velocity_file.good()) this->openvel = true;
            else {
                cerr << "Error open velocity file" << endl;
                exit(3);
            }
        }
    }


public:
    void add_filename(const string &filename) {
        arc_filename_list.push_back(filename);
    }

    void add_topology(const string &filename) {
        enable_binaray_file = true;
        topology_filename = filename;
        std::string ext_name = ext_filename(filename);
        boost::to_lower(ext_name);

        if (ext_name == "arc" or ext_name == "xyz") {
            topology_type = TOPOLOGY_TYPE::ARC;
        } else if (ext_name == "mol2") {
            topology_type = TOPOLOGY_TYPE::MOL2;
        } else {
            std::cerr << " Error file type of topology file [ " << filename << "] " << std::endl;
            exit(1);
        }

    }

    std::shared_ptr<Frame> readOneFrame() {
        if (first_time) {
            if (enable_binaray_file) {
                position_file.open(topology_filename, ofstream::in);
                switch (topology_type) {
                    case TOPOLOGY_TYPE::ARC:
                        frame = readOneFrameArc();
                        break;
                    case TOPOLOGY_TYPE::MOL2:
                        frame = readOneFrameMol2();
                        break;
                }
                position_file.close();
            }
            std::string filename = arc_filename_list.front();
            open(filename);
            arc_filename_list.pop_front();
        }
        loop:
        try {
            if (isnetcdf) {
                if (!readOneFrameNetCDF()) {
                    throw std::exception();
                }
            } else if (istrr) {
                if (!readOneFrameTrr()) {
                    throw std::exception();
                }
            } else if (isxtc) {
                if (!readOneFrameXtc()) {
                    throw std::exception();
                }
            } else if (isbin) {
                frame = readOneFrameTraj();
                frame->enable_bound = frame->a_axis != 0.0;
            } else {
                frame = readOneFrameArc();
            }

            if (openvel && frame) {
                readOneFrameVel();
            }
        } catch (std::exception &e) {
            if (arc_filename_list.empty()) {
                frame.reset();
                close();
            } else {
                close();
                string filename = arc_filename_list.front();
                open(filename);
                arc_filename_list.pop_front();
                goto loop;
            }
        }

        first_time = false;

        return frame;
    }

};


class AtomItem {
public:
    int typ;
    double mass;
};

class Forcefield {
public:

    void read(const string &filename);

    void read_tinker_prm(const string &filename);

    void read_mass_map(const string &filename);

    double find_mass(shared_ptr<Atom> &atom) {
        for (auto &id : items) {
            if (Atom::is_match(atom, id.first)) return id.second;
        }
        std::cerr << "Atom mass not found !" << std::endl;
        exit(7);
    }

private:
    std::list<std::pair<Atom::AtomIndenter, double>> items;
};

void Forcefield::read(const string &filename) {
    auto ext = ext_filename(filename);
    if (ext == "prm") {
        read_tinker_prm(filename);
    } else if (ext == "map") {
        read_mass_map(filename);
    } else {
        std::cerr << "Unrecognized file extension" << std::endl;
        exit(6);
    }
}

void Forcefield::read_tinker_prm(const string &filename) {
    fstream f;
    f.open(filename, ofstream::in);
    f.exceptions(fstream::eofbit | fstream::failbit | fstream::badbit);
    string line;
    while (true) {
        try {
            std::getline(f, line);
            auto field = split(line);
            if (field.empty()) continue;
            if (field[0] != "atom") continue;
            int type = stoi(field[1]);
            double mass = stod(field[field.size() - 2]);
            items.emplace_back(Atom::Atom_Indenter_TypeNumber{type}, mass);
        } catch (std::exception &e) {
            f.close();
            return;
        }
    }
}

void Forcefield::read_mass_map(const string &filename) {
    fstream f;
    f.open(filename, ofstream::in);
    f.exceptions(fstream::eofbit | fstream::failbit | fstream::badbit);
    string line;
    while (true) {
        try {
            std::getline(f, line);
            boost::trim(line);
            auto field = split(line);
            if (field.empty()) continue;
            if (field.size() != 3) {
                std::cerr << "Force field mass map syntax error : " << line << std::endl;
                exit(8);
            }
            auto type = field[0];
            double mass = boost::lexical_cast<double>(field[2]);

            if (type == "name") {
                items.emplace_back(Atom::Atom_Indenter_Symbol{field[1]}, mass);
            } else if (type == "type") {
                items.emplace_back(Atom::Atom_Indenter_TypeName{field[1]}, mass);
            } else if (type == "typenumber") {
                items.emplace_back(Atom::Atom_Indenter_TypeNumber{boost::lexical_cast<int>(field[1])}, mass);
            } else {
                std::cerr << "unrecognized keyword : " << type << std::endl;
                exit(8);
            }

        } catch (boost::bad_lexical_cast &e) {
            std::cerr << "boost::bad_lexical_cast  " << e.what() << std::endl;
            exit(9);
        } catch (std::exception &e) {
            f.close();
            return;
        }
    }
}


Forcefield forcefield;
bool enable_forcefield = false;


void Molecule::calc_mass() {
    double mol_mass = 0.0;
    for (auto &atom : atom_list) {
        mol_mass += forcefield.find_mass(atom);
    }
    mass = mol_mass;
}

std::tuple<double, double, double> Molecule::calc_weigh_center(std::shared_ptr<Frame> &frame) {
    double xmid = 0.0;
    double ymid = 0.0;
    double zmid = 0.0;
    bool first_atom = true;
    double first_x, first_y, first_z;
    double mol_mass = 0.0;
    for (auto &atom : atom_list) {
        if (first_atom) {
            first_atom = false;
            first_x = atom->x;
            first_y = atom->y;
            first_z = atom->z;
            double weigh = forcefield.find_mass(atom);
            mol_mass += weigh;
            xmid = first_x * weigh;
            ymid = first_y * weigh;
            zmid = first_z * weigh;

        } else {
            double xr = atom->x - first_x;
            double yr = atom->y - first_y;
            double zr = atom->z - first_z;
            frame->image(xr, yr, zr);
            double weigh = forcefield.find_mass(atom);
            mol_mass += weigh;
            xmid += (first_x + xr) * weigh;
            ymid += (first_y + yr) * weigh;
            zmid += (first_z + zr) * weigh;
        }
    }

    return make_tuple(xmid / mol_mass, ymid / mol_mass, zmid / mol_mass);
}

tuple<double, double, double> Molecule::calc_dipole(shared_ptr<Frame> &frame) {
    auto mass_center = calc_weigh_center(frame);

    double dipole_x = 0.0;
    double dipole_y = 0.0;
    double dipole_z = 0.0;

    for (auto &atom : atom_list) {
        double xr = atom->x - get<0>(mass_center);
        double yr = atom->y - get<1>(mass_center);
        double zr = atom->z - get<2>(mass_center);

        frame->image(xr, yr, zr);

        dipole_x += atom->charge * (xr + get<0>(mass_center));
        dipole_y += atom->charge * (yr + get<1>(mass_center));
        dipole_z += atom->charge * (zr + get<2>(mass_center));
    }
    return make_tuple(dipole_x, dipole_y, dipole_z);
}

void Molecule::calc_center(std::shared_ptr<Frame> &frame) {
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;
    bool first_atom = true;
    double first_x, first_y, first_z;
    for (auto &atom : atom_list) {
        if (first_atom) {
            first_atom = false;
            sum_x = first_x = atom->x;
            sum_y = first_y = atom->y;
            sum_z = first_z = atom->z;
        } else {
            double xr = atom->x - first_x;
            double yr = atom->y - first_y;
            double zr = atom->z - first_z;
            frame->image(xr, yr, zr);
            sum_x += first_x + xr;
            sum_y += first_y + yr;
            sum_z += first_z + zr;
        }
    }
    auto len = atom_list.size();
    center_x = sum_x / len;
    center_y = sum_y / len;
    center_z = sum_z / len;
}


class GROWriter {
public:
    void write(const string &filename, shared_ptr<Frame> &frame) {
        fstream f;
        f.open(filename, std::iostream::out);

        f << frame->title << ", Generated by Analsyis (Miaoren) , t = 0.0" << endl;

        f << frame->atom_list.size() << endl;
        int mol_index = 1;
        map<shared_ptr<Molecule>, int> mol_index_map;
        for (auto &atom : frame->atom_list) {
            int local_mol_index;
            if (mol_index_map.count(atom->molecule.lock())) {
                local_mol_index = mol_index_map[atom->molecule.lock()];
            } else {
                local_mol_index = mol_index;
                mol_index_map[atom->molecule.lock()] = local_mol_index;
                mol_index++;
            }

            f << fmt::sprintf("%5d%-5s%5s%5d%8.3f%8.3f%8.3f\n", mol_index, to_string(mol_index),
                              atom->symbol, atom->seq, atom->x / 10.0, atom->y / 10.0, atom->z / 10.0);
        }
        if (frame->enable_bound) {
            if (frame->alpha == 90.00 and frame->beta == 90.00 and frame->gamma == 90.00) {
                f << fmt::sprintf("%f   %f   %f \n", frame->a_axis / 10.0, frame->b_axis / 10.0, frame->c_axis / 10.0);
            } else {
                gmx::rvec box[3];
                translate(frame->a_axis / 10.0, frame->b_axis / 10.0, frame->c_axis / 10.0,
                          frame->alpha, frame->beta, frame->gamma, box);
                f << fmt::sprintf("%f   %f   %f   %f   %f   %f    %f   %f   %f\n",
                                  box[0][0], box[1][1], box[2][2], box[0][1], box[0][2], box[1][2], box[2][0],
                                  box[2][1]);
            }
        }
        f.close();
    }
};


class XTCWriter {
    gmx::t_fileio *xd = nullptr;
    gmx::rvec *x = nullptr;
    int step;
    gmx::real prec, time;
public:
    void open(const string &filename) {
        // First Time
        xd = gmx::open_xtc(filename.c_str(), "w");
        step = 0;
        time = 0.0;
        prec = 1000.0;

    }

    void write(shared_ptr<Frame> &frame) {
        if (!x) {
            x = new gmx::rvec[frame->atom_list.size()];
        }
        int i = 0;
        for (auto &atom : frame->atom_list) {
            x[i][0] = atom->x / 10.0;
            x[i][1] = atom->y / 10.0;
            x[i][2] = atom->z / 10.0;
            i++;
        }

        gmx::rvec box[3];
        translate(frame->a_axis / 10.0, frame->b_axis / 10.0, frame->c_axis / 10.0,
                  frame->alpha, frame->beta, frame->gamma, box);
        gmx::write_xtc(xd, i, step, time, box, x, prec);

        step++;
        time++;
    }

    void close() {
        gmx::close_xtc(xd);
        delete[] x;
    }

};


class TRRWriter {
    gmx::t_fileio *xd = nullptr;
    gmx::rvec *x = nullptr;
    int step;
    float time;
public:
    void open(const string &filename) {
        // First Time
        xd = gmx::open_trn(filename.c_str(), "w");
        step = 0;
        time = 0.0;
    }

    void write(shared_ptr<Frame> &frame) {
        if (!x) {
            x = new gmx::rvec[frame->atom_list.size()];
        }
        int i = 0;
        for (auto &atom : frame->atom_list) {
            x[i][0] = atom->x / 10.0;
            x[i][1] = atom->y / 10.0;
            x[i][2] = atom->z / 10.0;
            i++;
        }
        gmx::rvec box[3];
        translate(frame->a_axis / 10.0, frame->b_axis / 10.0, frame->c_axis / 10.0,
                  frame->alpha, frame->beta, frame->gamma, box);

        gmx::fwrite_trn(xd, step, time, 0.0, box, i, x, NULL, NULL);
        step++;
        time++;
    }

    void close() {
        gmx::close_trn(xd);
        delete[] x;
    }

};

class NetCDFWriter {
    struct AmberNetcdf NC;
    double *x = nullptr;
    int step = 0;
public:
    void open(const string &filename, int natom) {
        if (netcdfCreate(&NC, filename.c_str(), natom, 1)) {
            cerr << "Error open " << filename << endl;
        }
    }

    void close() {
        netcdfClose(&NC);
    }

    void write(shared_ptr<Frame> &frame) {
        if (step == 0) {
            x = new double[NC.ncatom3];

        }
        double box[6];
        box[0] = frame->a_axis;
        box[1] = frame->b_axis;
        box[2] = frame->c_axis;
        box[3] = frame->alpha;
        box[4] = frame->beta;
        box[5] = frame->gamma;
        double *p = x;
        for (auto &atom : frame->atom_list) {
            *p = atom->x;
            p++;
            *p = atom->y;
            p++;
            *p = atom->z;
            p++;
        }
        if (netcdfWriteNextFrame(&NC, x, box)) {
            cerr << "Error write  mdcrd frame " << endl;
        }
        step++;
    }
};


class BasicAnalysis {
public:
    virtual void process(std::shared_ptr<Frame> &frame) = 0;

    virtual void print() = 0;

    virtual void readInfo() = 0;
};


class GmxTrj : public BasicAnalysis {
    enum class PBCType {
        None,
        OneAtom,
        OneMol
    } pbc_type;
    int step = 0;
    string grofilename;
    string xtcfilename;
    string trrfilename;
    string mdcrdfilename;
    XTCWriter xtc;
    TRRWriter trr;
    NetCDFWriter mdcrd;

    int num;

    bool enable_xtc = true;
    bool enable_trr = true;
    bool enable_gro = true;
    bool enable_mdcrd = true;
public:
    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;


};

void GmxTrj::process(std::shared_ptr<Frame> &frame) {
    if (pbc_type == PBCType::OneAtom) {
        auto center_x = frame->atom_map[this->num]->x;
        auto center_y = frame->atom_map[this->num]->y;
        auto center_z = frame->atom_map[this->num]->z;

        for (auto &mol : frame->molecule_list) {
            for (auto &atom : mol->atom_list) {
                atom->x -= center_x;
                atom->y -= center_y;
                atom->z -= center_z;
            }
        }
    } else if (pbc_type == PBCType::OneMol) {
        auto center_mol = frame->atom_map[this->num]->molecule.lock();
        center_mol->calc_center(frame);
        auto center_x = center_mol->center_x;
        auto center_y = center_mol->center_y;
        auto center_z = center_mol->center_z;
        for (auto &mol : frame->molecule_list) {
            for (auto &atom : mol->atom_list) {
                atom->x -= center_x;
                atom->y -= center_y;
                atom->z -= center_z;
            }
        }
    }
    if (pbc_type != PBCType::None) {
        for (auto &mol : frame->molecule_list) {
            mol->calc_center(frame);
            double x_move = 0.0;
            double y_move = 0.0;
            double z_move = 0.0;
            while (mol->center_x > frame->a_axis_half) {
                mol->center_x -= frame->a_axis;
                x_move -= frame->a_axis;
            }
            while (mol->center_x < -frame->a_axis_half) {
                mol->center_x += frame->a_axis;
                x_move += frame->a_axis;
            }
            while (mol->center_y > frame->b_axis_half) {
                mol->center_y -= frame->b_axis;
                y_move -= frame->b_axis;
            }
            while (mol->center_y < -frame->b_axis_half) {
                mol->center_y += frame->b_axis;
                y_move += frame->b_axis;
            }
            while (mol->center_z > frame->c_axis_half) {
                mol->center_z -= frame->c_axis;
                z_move -= frame->c_axis;
            }
            while (mol->center_z < -frame->c_axis_half) {
                mol->center_z += frame->c_axis;
                z_move += frame->c_axis;
            }
            for (auto &atom : mol->atom_list) {
                atom->x += x_move + frame->a_axis_half;
                atom->y += y_move + frame->b_axis_half;
                atom->z += z_move + frame->c_axis_half;
            }
        }
    }

    if (step == 0) {
        GROWriter w;
        w.write(grofilename, frame);
        if (enable_xtc) xtc.open(xtcfilename);
        if (enable_trr) trr.open(trrfilename);
        if (enable_mdcrd) mdcrd.open(mdcrdfilename, frame->atom_list.size());
    }
    if (enable_xtc) xtc.write(frame);
    if (enable_trr) trr.write(frame);
    if (enable_mdcrd) mdcrd.write(frame);
    step++;
}

void GmxTrj::print() {
    if (enable_xtc) xtc.close();
    if (enable_trr) trr.close();
    if (enable_mdcrd) mdcrd.close();
}

void GmxTrj::readInfo() {
    string line = input("output gro file [Y]: ");
    trim(line);
    if (!line.empty()) {
        if (line[0] == 'N' or line[0] == 'n') {
            enable_gro = false;
        }
    }
    if (enable_gro) {
        grofilename = input("GRO filename : ");
        trim(grofilename);
    }
    line = input("output xtc file [Y]: ");
    trim(line);
    if (!line.empty()) {
        if (line[0] == 'N' or line[0] == 'n') {
            enable_xtc = false;
        }
    }
    if (enable_xtc) {
        xtcfilename = input("XTC filename : ");
        trim(xtcfilename);
    }

    line = input("output trr file [Y]: ");
    trim(line);
    if (!line.empty()) {
        if (line[0] == 'N' or line[0] == 'n') {
            enable_trr = false;
        }
    }
    if (enable_trr) {
        trrfilename = input("TRR filename : ");
        trim(trrfilename);
    }

    line = input("output amber nc file [Y]: ");
    trim(line);
    if (!line.empty()) {
        if (line[0] == 'N' or line[0] == 'n') {
            enable_mdcrd = false;
        }
    }
    if (enable_mdcrd) {
        mdcrdfilename = input("nc filename : ");
        trim(mdcrdfilename);
    }
    cout << "PBC transform option" << endl;
    cout << "(0) Do nothing" << endl;
    cout << "(1) Make atom i as center" << endl;
    cout << "(2) Make molecule i as center" << endl;

    int option = 0;
    while (true) {
        line = input("Choose:");
        trim(line);

        if (!line.empty()) {

            try {
                option = std::stoi(line);
                string line2;
                switch (option) {
                    case 0:
                        pbc_type = PBCType::None;
                        break;
                    case 1:
                        pbc_type = PBCType::OneAtom;
                        while (true) {
                            line2 = input("Plese enter the atom NO. : ");
                            try {
                                num = std::stoi(line2);
                            } catch (std::invalid_argument) {
                                cerr << "must be a number !" << endl;
                                continue;
                            }


                            if (num <= 0) {
                                cerr << "the atom number must be positve" << endl;
                                continue;
                            }
                            break;
                        }
                        break;
                    case 2:
                        pbc_type = PBCType::OneMol;
                        while (true) {
                            line2 = input("Plese enter one atom NO. that the molecule include:");
                            try {
                                num = std::stoi(line2);
                            } catch (std::invalid_argument) {
                                cerr << "must be a number !" << endl;
                                continue;
                            }
                            if (num <= 0) {
                                cerr << "the atom number must be positve" << endl;
                                continue;
                            }
                            break;
                        }
                        break;
                    default:
                        cerr << "option not found !" << endl;
                        continue;
                }
                break;

            } catch (std::invalid_argument) {
                cerr << "must be a number !" << endl;
            }
        }
    }


}


// Distance 
class Distance : public BasicAnalysis {
    std::list<double> group_dist_list;
    std::set<int> group1;
    std::set<int> group2;
public:
    Distance() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

};

void Distance::process(std::shared_ptr<Frame> &frame) {
    double x1, y1, z1, x2, y2, z2;
    x1 = y1 = z1 = x2 = y2 = z2 = 0.0;
    double weigh1, weigh2;
    weigh1 = weigh2 = 0.0;
    for (auto &num1 : group1) {
        auto &atom1 = frame->atom_map[num1];
        double mass = forcefield.find_mass(atom1);
        x1 += atom1->x * mass;
        y1 += atom1->y * mass;
        z1 += atom1->z * mass;
        weigh1 += mass;
    }
    x1 /= weigh1;
    y1 /= weigh1;
    z1 /= weigh1;

    for (auto &num2 : group2) {
        auto &atom2 = frame->atom_map[num2];
        double mass = forcefield.find_mass(atom2);
        x2 += atom2->x * mass;
        y2 += atom2->y * mass;
        z2 += atom2->z * mass;
        weigh2 += mass;
    }
    x2 /= weigh2;
    y2 /= weigh2;
    z2 /= weigh2;

    double xr = x2 - x1;
    double yr = y2 - y1;
    double zr = z2 - z1;

    frame->image(xr, yr, zr);

    double dist = sqrt(xr * xr + yr * yr + zr * zr);
    group_dist_list.push_back(dist);
}


void Distance::print() {
    outfile << "****************************************" << endl;
    outfile << "Distance between " << endl;
    outfile << "group 1 :";
    for (auto num1 : group1) {
        outfile << num1 << "  ";
    }
    outfile << endl << "group 2 :";
    for (auto num2 : group2) {
        outfile << num2 << "  ";
    }
    outfile << endl;
    outfile << "****************************************" << endl;


    int cyc = 1;
    for (auto &dist : group_dist_list) {
        outfile << cyc << "    " << dist << std::endl;
        cyc++;
    }
}

void Distance::readInfo() {
    enable_forcefield = true;
    std::string input_line;
    std::vector<std::string> field;
    input_line = input("Please enter group 1:");
    field = split(input_line);
    for (auto it = field.cbegin(); it != field.cend(); ++it) {
        auto i = std::stoi(*it);
        if (i > 0) {
            group1.insert(i);
        } else if (i < 0) {
            ++it;
            auto j = std::stoi(*it);
            for (int k = std::abs(i); k <= j; k++) group1.insert(k);
        } else {
            cerr << "error of atom num select" << endl;
            exit(1);
        }
    }
    input_line = input("Please enter group 2:");
    field = split(input_line);
    for (auto it = field.cbegin(); it != field.cend(); ++it) {
        auto i = std::stoi(*it);
        if (i > 0) {
            group2.insert(i);
        } else if (i < 0) {
            ++it;
            auto j = std::stoi(*it);
            for (int k = std::abs(i); k <= j; k++) group2.insert(k);
        } else {
            cerr << "error of atom num select" << endl;
            exit(1);
        }
    }


}
// End of Distance


class CoordinateNumPerFrame : public BasicAnalysis {

    int typ1, typ2;
    double dist_cutoff;
    list<int> cn_list;
public:
    CoordinateNumPerFrame() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;
};

void CoordinateNumPerFrame::process(std::shared_ptr<Frame> &frame) {
    int cn_sum = 0;
    for (auto &atom1 : frame->atom_list) {
        if (atom1->typ == typ1) {
            for (auto &atom2 : frame->atom_list) {
                if (atom2->typ == typ2) {
                    if (atom_distance(atom1, atom2, frame) <= this->dist_cutoff) {
                        cn_sum++;
                    }
                }
            }
        }
    }
    cn_list.push_back(cn_sum);

}

void CoordinateNumPerFrame::print() {
    outfile << "***************************" << endl;
    outfile << "******* CN per Frame ******" << endl;
    outfile << "type 1 :" << typ1 << endl;
    outfile << "type 2 :" << typ2 << endl;
    outfile << "cutoff :" << dist_cutoff << endl;
    outfile << "***************************" << endl;
    int cyc = 1;
    for (auto &cn : cn_list) {
        outfile << cyc << "   " << cn << endl;
        cyc++;
    }
    outfile << "***************************" << endl;
}

void CoordinateNumPerFrame::readInfo() {
    typ1 = choose(1, INT32_MAX, "Please enter typ1:");
    typ2 = choose(1, INT32_MAX, "Please enter typ2:");
    dist_cutoff = choose(0.0, GMX_DOUBLE_MAX, "Please enter distance cutoff:");
}


class FirstCoordExchangeSearch : public BasicAnalysis {
    int typ1, typ2;
    std::string type_name1, type_name2;
    bool use_name = false;
    double dist_cutoff, tol_dist, time_cutoff;
    enum class Direction {
        IN, OUT
    };
    typedef struct {
        Direction direction;
        int seq;
        int exchange_frame;
    } ExchangeItem;

    list<ExchangeItem> exchange_list;

    int step = 0;

    typedef struct {
        bool inner;
    } State;
    map<int, State> state_machine;

    std::set<int> init_seq_in_shell;

public:
    FirstCoordExchangeSearch() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;
};

void FirstCoordExchangeSearch::process(std::shared_ptr<Frame> &frame) {
    step++;

    for (auto &atom1 : frame->atom_list) {
        if (use_name ? atom1->type_name == type_name1 : atom1->typ == typ1) {
            for (auto &atom2 : frame->atom_list) {
                if (use_name ? atom2->type_name == type_name2 : atom2->typ == typ2) {
                    if (step == 1) {
                        State state;
                        state.inner = atom_distance(atom1, atom2, frame) <= this->dist_cutoff;
                        state_machine[atom2->seq] = state;
                        if (state.inner) init_seq_in_shell.insert(atom2->seq);
                    } else {
                        auto &state = state_machine[atom2->seq];
                        if (state.inner) {
                            if (atom_distance(atom1, atom2, frame) >= this->dist_cutoff + tol_dist) {
                                state.inner = false;
                                ExchangeItem item;
                                item.seq = atom2->seq;
                                item.direction = Direction::OUT;
                                item.exchange_frame = step;
                                exchange_list.push_back(item);
                            }
                        } else {
                            if (atom_distance(atom1, atom2, frame) <= this->dist_cutoff - tol_dist) {
                                state.inner = true;
                                ExchangeItem item;
                                item.seq = atom2->seq;
                                item.direction = Direction::IN;
                                item.exchange_frame = step;
                                exchange_list.push_back(item);
                            }
                        }
                    }
                }
            }
        }
    }
}

void FirstCoordExchangeSearch::print() {
    outfile << "***************************" << endl;
    outfile << "***** Exchange Search *****" << endl;
    outfile << "type 1 :";
    use_name ? outfile << type_name1 : outfile << typ1;
    outfile << std::endl;
    outfile << "type 2 :";
    use_name ? outfile << type_name2 : outfile << typ2;
    outfile << std::endl;

    outfile << "cutoff :" << dist_cutoff << endl;
    outfile << "tol dist :" << tol_dist << endl;
    outfile << "time_cutoff :" << time_cutoff << endl;
    outfile << "***************************" << endl;
    for (int seq : init_seq_in_shell) {
        outfile << "  " << seq;
    }
    outfile << "\n***************************" << endl;

    outfile << "** seq **  direction ***** exchange frame *****" << endl;
    for (auto it = exchange_list.begin(); it != exchange_list.end(); it++) {
        outfile << boost::format("%10d%6s%15d   !   ")
                   % it->seq
                   % (it->direction == Direction::IN ? "IN" : "OUT")
                   % it->exchange_frame;
        if (it->direction == Direction::IN) {
            init_seq_in_shell.insert(it->seq);
        } else {
            init_seq_in_shell.erase(it->seq);
        }
        for (int seq : init_seq_in_shell) {
            outfile << "  " << seq;
        }
        outfile << std::endl;
    }
    outfile << "***************************" << endl;


}

void FirstCoordExchangeSearch::readInfo() {

    use_name = (choose(1, 2, "(1) use_name \n(2) use type\n") == 1);
    if (use_name) {
        while (!type_name1.length()) {
            type_name1 = input("Please enter typ1:");
            boost::trim(type_name1);
        }
        while (!type_name2.length()) {
            type_name2 = input("Please enter typ2:");
            boost::trim(type_name2);
        }
    } else {
        typ1 = choose(1, INT32_MAX, "Please enter typ1:");
        typ2 = choose(1, INT32_MAX, "Please enter typ2:");
    }
    dist_cutoff = choose(0.0, GMX_DOUBLE_MAX, "Please enter distance cutoff:");
    tol_dist = choose(0.0, GMX_DOUBLE_MAX, "Please enter tol dist:");
    time_cutoff = choose(0.0, GMX_DOUBLE_MAX, "Please enter timecutoff:");
}


class RadicalDistribtuionFunction : public BasicAnalysis {

    double rmax;
    double width;

    bool intramol = false;
    int nframe = 0;
    int numj = 0, numk = 0;

    int nbin;
    map<int, int> hist;
    map<int, double> gr, gs, integral;

    double xbox, ybox, zbox;
public:

    std::vector<Atom::AtomIndenter> ids1;
    std::vector<Atom::AtomIndenter> ids2;

    RadicalDistribtuionFunction() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;


};


void RadicalDistribtuionFunction::process(std::shared_ptr<Frame> &frame) {
    nframe++;
    if (nframe == 1) {
        for (auto &atom : frame->atom_list) {
            if (Atom::is_match(atom, ids1)) numj++;
            else if (Atom::is_match(atom, ids2)) numk++;
        }
    }
    xbox = frame->a_axis;
    ybox = frame->b_axis;
    zbox = frame->c_axis;

    for (auto &atom_j : frame->atom_list) {
        if (Atom::is_match(atom_j, ids1)) {
            double xj = atom_j->x;
            double yj = atom_j->y;
            double zj = atom_j->z;
            auto molj = atom_j->molecule.lock();

            for (auto &atom_k : frame->atom_list) {
                if (Atom::is_match(atom_k, ids2)) {
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
    }
}

void RadicalDistribtuionFunction::readInfo() {

    Atom::select2group(ids1, ids2);
    rmax = choose(0.0, GMX_DOUBLE_MAX, "Enter Maximum Distance to Accumulate[10.0 Ang]:", true, 10.0);
    width = choose(0.0, GMX_DOUBLE_MAX, "Enter Width of Distance Bins [0.01 Ang]:", true, 0.01);
    string inputline = input("Include Intramolecular Pairs in Distribution[N]:");
    trim(inputline);
    if (!inputline.empty()) {
        if (str_tolower(inputline) == "y") {
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
            gs[i] = max(0.0, gs[i]);
        }
    }

    outfile << "************************************************" << endl;
    outfile << "***** Pairwise Radial Distribution Function ****" << endl;

    outfile << "First Type : " << ids1 << " Second Type : " << ids2 << endl;

    outfile << "************************************************" << endl;
    outfile << "Bin    Counts    Distance    Raw g(r)  Smooth g(r)   Integral" << endl;

    for (int i = 1; i <= nbin; i++) {
        outfile << fmt::sprintf("%d        %d      %.4f      %.4f     %.4f      %.4f\n",
                                i, hist[i], (i - 0.5) * width, gr[i], gs[i], integral[i]);
    }

    outfile << "************************************************" << endl;
}

class RMSDCal : public BasicAnalysis {

    std::set<int> group; // contain the NO. of fitting atom
    int steps = 0; // current frame number
    std::map<int, double> rmsd_map;
    bool first_frame = true;


    double x1[ATOM_MAX], y1[ATOM_MAX], z1[ATOM_MAX];
    double x2[ATOM_MAX], y2[ATOM_MAX], z2[ATOM_MAX];

    double rmsvalue(shared_ptr<Frame> &frame);

public:
    RMSDCal() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

    static double rmsfit(double x1[], double y1[], double z1[],
                         double x2[], double y2[], double z2[], int nfit);

    static void jacobi(int n, double a[4][4], double d[], double v[4][4]);

    static void quatfit(int n1, double x1[], double y1[], double z1[],
                        int n2, double x2[], double y2[], double z2[], int nfit);

    static void center(int n1, double x1[], double y1[], double z1[],
                       int n2, double x2[], double y2[], double z2[],
                       double mid[], int nfit);

};

void RMSDCal::process(std::shared_ptr<Frame> &frame) {
    steps++;
    rmsd_map[steps] = rmsvalue(frame);
}

void RMSDCal::print() {
    outfile << "***************************" << endl;
    outfile << "****** RMSD Calculator ****" << endl;
    outfile << "GROUP:";
    for (auto &i : this->group) outfile << i << "   ";
    outfile << endl;
    outfile << "***************************" << endl;
    for (int cyc = 1; cyc <= steps; cyc++) {
        outfile << cyc << "     " << rmsd_map[cyc] << endl;
    }
    outfile << "***************************" << endl;
}

void RMSDCal::readInfo() {
    auto input_line = input("Please enter group:");
    auto field = split(input_line);
    for (auto it = field.cbegin(); it != field.cend(); ++it) {
        auto i = std::stoi(*it);
        if (i > 0) {
            group.insert(i);
        } else if (i < 0) {
            ++it;
            auto j = std::stoi(*it);
            for (int k = std::abs(i); k <= j; k++) group.insert(k);
        } else {
            cerr << "error of atom num select" << endl;
            exit(1);
        }
    }
}


double RMSDCal::rmsvalue(shared_ptr<Frame> &frame) {

    int nfit, n;
    nfit = n = static_cast<int>(this->group.size());

    if (first_frame) {
        first_frame = false;
        int index = 0;
        bool first_atom = true;
        double first_x, first_y, first_z;
        for (auto i : this->group) {
            auto &atom = frame->atom_map[i];
            if (first_atom) {
                first_atom = false;
                first_x = x1[index] = atom->x;
                first_y = y1[index] = atom->y;
                first_z = z1[index] = atom->z;
            } else {
                double xr = atom->x - first_x;
                double yr = atom->y - first_y;
                double zr = atom->z - first_z;
                frame->image(xr, yr, zr);
                x1[index] = first_x + xr;
                y1[index] = first_y + yr;
                z1[index] = first_z + zr;
            }
            index++;
        }
        return 0.0;
    } else {
        int index = 0;
        bool first_atom = true;
        double first_x, first_y, first_z;
        for (auto i : this->group) {
            auto &atom = frame->atom_map[i];
            if (first_atom) {
                first_atom = false;
                first_x = x2[index] = atom->x;
                first_y = y2[index] = atom->y;
                first_z = z2[index] = atom->z;
            } else {
                double xr = atom->x - first_x;
                double yr = atom->y - first_y;
                double zr = atom->z - first_z;
                frame->image(xr, yr, zr);
                x2[index] = first_x + xr;
                y2[index] = first_y + yr;
                z2[index] = first_z + zr;
            }
            index++;
        }
    }

    double mid[3];
    center(n, x1, y1, z1, n, x2, y2, z2, mid, nfit);
    quatfit(n, x1, y1, z1, n, x2, y2, z2, nfit);
    double rms = rmsfit(x1, y1, z1, x2, y2, z2, nfit);
    return rms;
}

void RMSDCal::center(int n1, double x1[], double y1[], double z1[],
                     int n2, double x2[], double y2[], double z2[],
                     double mid[], int nfit) {
    mid[0] = mid[1] = mid[2] = 0.0;
    double norm = 0.0;
    for (int i = 0; i < nfit; ++i) {
        mid[0] += x2[i];
        mid[1] += y2[i];
        mid[2] += z2[i];
        norm += 1.0;
    }
    mid[0] /= norm;
    mid[1] /= norm;
    mid[2] /= norm;
    for (int i = 0; i < n2; ++i) {
        x2[i] -= mid[0];
        y2[i] -= mid[1];
        z2[i] -= mid[2];
    }

    mid[0] = mid[1] = mid[2] = 0.0;
    norm = 0.0;
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

void RMSDCal::quatfit(int n1, double x1[], double y1[], double z1[],
                      int n2, double x2[], double y2[], double z2[], int nfit) {
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

    for (i = 0; i < n2; ++i) {
        xrot = x2[i] * rot[0][0] + y2[i] * rot[0][1] + z2[i] * rot[0][2];
        yrot = x2[i] * rot[1][0] + y2[i] * rot[1][1] + z2[i] * rot[1][2];
        zrot = x2[i] * rot[2][0] + y2[i] * rot[2][1] + z2[i] * rot[2][2];
        x2[i] = xrot;
        y2[i] = yrot;
        z2[i] = zrot;
    }

}

double RMSDCal::rmsfit(double x1[], double y1[], double z1[],
                       double x2[], double y2[], double z2[], int nfit) {

    double fit;
    double xr, yr, zr, dist2;
    double norm;

    fit = 0.0;
    norm = 0.0;
    for (int i = 0; i < nfit; ++i) {
        xr = x1[i] - x2[i];
        yr = y1[i] - y2[i];
        zr = z1[i] - z2[i];
        dist2 = xr * xr + yr * yr + zr * zr;
        norm += 1.0;
        fit += dist2;
    }
    return sqrt(fit / norm);
}

void RMSDCal::jacobi(int n, double a[4][4], double d[], double v[4][4]) {
    // taken from tinker

    int i, j, k;
    int ip, iq;
    int nrot, maxrot;
    double sm, tresh, s, c, t;
    double theta, tau, h, g, p;
    //    double *b; //traditional point
    //    double *z;
    double b[4];
    double z[4];
    //    b = new double[n];
    //    z = new double[n];

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
    //    delete [] b; b = nullptr;
    //    delete [] z; z = nullptr;

    if (nrot == maxrot)
        std::cerr << " JACOBI  --  Matrix Diagonalization not Converged" << std::endl;

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

class RMSFCal : public BasicAnalysis {


    std::set<int> group; // contain the NO. of fitting atom

    int steps = 0; // current frame number
    bool first_frame = true;

    static double rmsfit(double x1[], double y1[], double z1[],
                         double x2[], double y2[], double z2[], int nfit) {
        return RMSDCal::rmsfit(x1, y1, z1, x2, y2, z2, nfit);
    }

    static void jacobi(int n, double a[4][4], double d[], double v[4][4]) {
        RMSDCal::jacobi(n, a, d, v);
    }

    static void quatfit(int n1, double x1[], double y1[], double z1[],
                        int n2, double x2[], double y2[], double z2[], int nfit) {
        RMSDCal::quatfit(n1, x1, y1, z1, n2, x2, y2, z2, nfit);
    }

    static void center(int n, double x[], double y[], double z[], double mid[], int nfit) {
        mid[0] = mid[1] = mid[2] = 0.0;
        double norm = 0.0;
        for (int i = 0; i < nfit; ++i) {
            mid[0] += x[i];
            mid[1] += y[i];
            mid[2] += z[i];
            norm += 1.0;
        }
        mid[0] /= norm;
        mid[1] /= norm;
        mid[2] /= norm;

        for (int i = 0; i < n; ++i) {
            x[i] -= mid[0];
            y[i] -= mid[1];
            z[i] -= mid[2];
        }
    }

    double x_avg[ATOM_MAX], y_avg[ATOM_MAX], z_avg[ATOM_MAX];
    double x1[ATOM_MAX], y1[ATOM_MAX], z1[ATOM_MAX];
    double x2[ATOM_MAX], y2[ATOM_MAX], z2[ATOM_MAX];

    double rmsvalue(int index);

    map<int, map<int, double>> x, y, z;

public:
    RMSFCal() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;
};

void RMSFCal::process(std::shared_ptr<Frame> &frame) {
    steps++;
    int nfit, n;
    nfit = n = static_cast<int>(this->group.size());

    if (!first_frame) {
        first_frame = false;
        int index = 0;
        bool first_atom = true;
        double first_x, first_y, first_z;
        for (auto i : this->group) {
            auto &atom = frame->atom_map[i];
            if (first_atom) {
                first_atom = false;
                first_x = x1[index] = atom->x;
                first_y = y1[index] = atom->y;
                first_z = z1[index] = atom->z;
            } else {
                double xr = atom->x - first_x;
                double yr = atom->y - first_y;
                double zr = atom->z - first_z;
                frame->image(xr, yr, zr);
                x1[index] = first_x + xr;
                y1[index] = first_y + yr;
                z1[index] = first_z + zr;
            }
            index++;
        }

        double mid[3];
        center(n, x1, y1, z1, mid, nfit);
        map<int, double> f1x, f1y, f1z;
        for (unsigned index = 0; index < this->group.size(); index++) {
            x_avg[index] = x1[index];
            y_avg[index] = y1[index];
            z_avg[index] = z1[index];
            f1x[index] = x1[index];
            f1y[index] = y1[index];
            f1z[index] = z1[index];
        }
        x[steps] = f1x;
        y[steps] = f1y;
        z[steps] = f1z;
    } else {
        int index = 0;
        bool first_atom = true;
        double first_x, first_y, first_z;
        for (auto i : this->group) {
            auto &atom = frame->atom_map[i];
            if (first_atom) {
                first_atom = false;
                first_x = x2[index] = atom->x;
                first_y = y2[index] = atom->y;
                first_z = z2[index] = atom->z;
            } else {
                double xr = atom->x - first_x;
                double yr = atom->y - first_y;
                double zr = atom->z - first_z;
                frame->image(xr, yr, zr);
                x2[index] = first_x + xr;
                y2[index] = first_y + yr;
                z2[index] = first_z + zr;
            }
            index++;
        }
        double mid[3];
        center(n, x2, y2, z2, mid, nfit);
        quatfit(n, x1, y1, z1, n, x2, y2, z2, nfit);
        map<int, double> f2x, f2y, f2z;
        for (unsigned index = 0; index < this->group.size(); index++) {
            x_avg[index] += x2[index];
            y_avg[index] += y2[index];
            z_avg[index] += z2[index];
            f2x[index] = x2[index];
            f2y[index] = y2[index];
            f2z[index] = z2[index];
        }
        x[steps] = f2x;
        y[steps] = f2y;
        z[steps] = f2z;
    }

}

void RMSFCal::print() {

    for (unsigned int index = 0; index < this->group.size(); index++) {
        x_avg[index] /= this->steps;
        y_avg[index] /= this->steps;
        z_avg[index] /= this->steps;
    }

    outfile << "***************************" << endl;
    outfile << "****** RMSF Calculator ****" << endl;
    outfile << "SET:";
    for (auto i : this->group) outfile << i << "   ";
    outfile << endl;
    outfile << "***************************" << endl;
    int index = 0;
    for (auto at : group) {
        outfile << at << "     " << rmsvalue(index) << endl;
        index++;
    }
    outfile << "***************************" << endl;
}

void RMSFCal::readInfo() {
    auto input_line = input("Please enter group:");
    auto field = split(input_line);
    for (auto it = field.cbegin(); it != field.cend(); ++it) {
        auto i = std::stoi(*it);
        if (i > 0) {
            group.insert(i);
        } else if (i < 0) {
            ++it;
            auto j = std::stoi(*it);
            for (int k = std::abs(i); k <= j; k++) group.insert(k);
        } else {
            cerr << "error of atom num select" << endl;
            exit(1);
        }
    }
}

double RMSFCal::rmsvalue(int index) {

    int nfit, n;
    nfit = n = static_cast<int>(this->group.size());

    double x2[ATOM_MAX], y2[ATOM_MAX], z2[ATOM_MAX];

    double dx2_y2_z2 = 0.0;
    double dx, dy, dz;
    for (int frame = 1; frame <= steps; frame++) {
        dx = x[frame][index] - x_avg[index];
        dy = y[frame][index] - y_avg[index];
        dz = z[frame][index] - z_avg[index];
        dx2_y2_z2 += dx * dx + dy * dy + dz * dz;

    }

    return sqrt(dx2_y2_z2 / steps);

}

enum class Symbol {
    Hydrogen,
    Carbon,
    Nitrogen,
    Oxygen,
    X,
    Unknown
};


Symbol which(shared_ptr<Atom> &atom) {
    double mass = forcefield.find_mass(atom);
    if (mass < 2.0)
        return Symbol::Hydrogen;
    else if (mass > 11.5 and mass < 13.0)
        return Symbol::Carbon;
    else if (mass > 13.0 and mass < 15.0)
        return Symbol::Nitrogen;
    else if (mass >= 15.0 and mass < 17.0)
        return Symbol::Oxygen;
    else return Symbol::Unknown;
}

enum class Selector {
    Acceptor,
    Donor,
    Both
};

enum class HBondType {
    VMDVerion,
    GMXVersion
};

class HBond : public BasicAnalysis {

    set<int> group1;
    Selector mode;
    set<int> group2;
    double donor_acceptor_dist_cutoff;
    double angle_cutoff;
    int steps = 0;
    std::map<int, int> hbonds;

    HBondType hbond_type = HBondType::VMDVerion;


    void Selector_Both(std::shared_ptr<Frame> &frame) {
        for (int atom2_no : group1) {
            // Atom2
            auto &atom2 = frame->atom_map[atom2_no];
            if (which(atom2) == Symbol::Hydrogen) {
                // Atom1
                auto &atom1 = frame->atom_map[atom2->con_list.front()];
                auto atom1_symbol = which(atom1);
                if (atom1_symbol == Symbol::Nitrogen or atom1_symbol == Symbol::Oxygen) {
                    for (int atom3_no : group1) {
                        // Atom3
                        auto &atom3 = frame->atom_map[atom3_no];
                        // atom1 and atom3 can not the same atom
                        if (atom1 == atom3) continue;
                        auto atom3_symbol = which(atom3);
                        if (atom3_symbol == Symbol::Nitrogen or atom3_symbol == Symbol::Oxygen) {
                            auto distance = atom_distance(atom1, atom3, frame);
                            if (distance <= this->donor_acceptor_dist_cutoff) {
                                if (atom1 != atom3 && !atom1->adj(atom3)) {
                                    auto vx1 = atom2->x - atom1->x;
                                    auto vy1 = atom2->y - atom1->y;
                                    auto vz1 = atom2->z - atom1->z;
                                    // atom1 -> atom2
                                    frame->image(vx1, vy1, vz1);
                                    auto len1 = std::sqrt(vx1 * vx1 + vy1 * vy1 + vz1 * vz1);

                                    double vx2, vy2, vz2;
                                    switch (hbond_type) {
                                        case HBondType::VMDVerion:
                                            // atom2 -> atom3
                                            vx2 = atom3->x - atom2->x;
                                            vy2 = atom3->y - atom2->y;
                                            vz2 = atom3->z - atom2->z;
                                            break;
                                        case HBondType::GMXVersion:
                                            // atom1 -> atom3
                                            vx2 = atom3->x - atom1->x;
                                            vy2 = atom3->y - atom1->y;
                                            vz2 = atom3->z - atom1->z;
                                            break;
                                    }

                                    frame->image(vx2, vy2, vz2);
                                    auto len2 = std::sqrt(vx2 * vx2 + vy2 * vy2 + vz2 * vz2);

                                    auto cosine = (vx1 * vx2 + vy1 * vy2 + vz1 * vz2) / (len1 * len2);
                                    auto ang = radian * std::acos(cosine);
                                    if (std::abs(ang) <= this->angle_cutoff) {
                                        hbonds[steps]++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void Selector_Donor(std::shared_ptr<Frame> &frame) {
        for (int atom2_no : group1) {
            // Atom2
            auto &atom2 = frame->atom_map[atom2_no];
            if (which(atom2) == Symbol::Hydrogen) {
                // Atom1
                auto atom1 = frame->atom_map[atom2->con_list.front()];
                auto atom1_symbol = which(atom1);
                if (atom1_symbol == Symbol::Nitrogen or atom1_symbol == Symbol::Oxygen) {
                    for (int atom3_no : group2) {
                        // Atom3
                        auto atom3 = frame->atom_map[atom3_no];
                        auto atom3_symbol = which(atom3);
                        if (atom3_symbol == Symbol::Nitrogen or atom3_symbol == Symbol::Oxygen) {
                            auto distance = atom_distance(atom1, atom3, frame);
                            if (distance <= this->donor_acceptor_dist_cutoff) {
                                auto vx1 = atom2->x - atom1->x;
                                auto vy1 = atom2->y - atom1->y;
                                auto vz1 = atom2->z - atom1->z;
                                frame->image(vx1, vy1, vz1);
                                auto len1 = std::sqrt(vx1 * vx1 + vy1 * vy1 + vz1 * vz1);

                                double vx2, vy2, vz2;
                                switch (hbond_type) {
                                    case HBondType::VMDVerion:
                                        // atom2 -> atom3
                                        vx2 = atom3->x - atom2->x;
                                        vy2 = atom3->y - atom2->y;
                                        vz2 = atom3->z - atom2->z;
                                        break;
                                    case HBondType::GMXVersion:
                                        // atom1 -> atom3
                                        vx2 = atom3->x - atom1->x;
                                        vy2 = atom3->y - atom1->y;
                                        vz2 = atom3->z - atom1->z;
                                        break;
                                }

                                frame->image(vx2, vy2, vz2);
                                auto len2 = std::sqrt(vx2 * vx2 + vy2 * vy2 + vz2 * vz2);
                                auto cosine = (vx1 * vx2 + vy1 * vy2 + vz1 * vz2) / (len1 * len2);
                                auto ang = radian * std::acos(cosine);
                                if (std::abs(ang) <= this->angle_cutoff) {
                                    hbonds[steps]++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void Selector_Acceptor(std::shared_ptr<Frame> &frame) {
        for (int atom2_no : group2) {
            // Atom2
            auto &atom2 = frame->atom_map[atom2_no];
            if (which(atom2) == Symbol::Hydrogen) {
                // Atom1
                auto atom1 = frame->atom_map[atom2->con_list.front()];
                auto atom1_symbol = which(atom1);
                if (atom1_symbol == Symbol::Nitrogen or atom1_symbol == Symbol::Oxygen) {
                    for (int atom3_no : group1) {
                        // Atom3
                        auto atom3 = frame->atom_map[atom3_no];
                        auto atom3_symbol = which(atom3);
                        if (atom3_symbol == Symbol::Nitrogen or atom3_symbol == Symbol::Oxygen) {
                            auto distance = atom_distance(atom1, atom3, frame);
                            if (distance <= this->donor_acceptor_dist_cutoff) {
                                auto vx1 = atom2->x - atom1->x;
                                auto vy1 = atom2->y - atom1->y;
                                auto vz1 = atom2->z - atom1->z;
                                frame->image(vx1, vy1, vz1);
                                auto len1 = std::sqrt(vx1 * vx1 + vy1 * vy1 + vz1 * vz1);

                                double vx2, vy2, vz2;
                                switch (hbond_type) {
                                    case HBondType::VMDVerion:
                                        // atom2 -> atom3
                                        vx2 = atom3->x - atom2->x;
                                        vy2 = atom3->y - atom2->y;
                                        vz2 = atom3->z - atom2->z;
                                        break;
                                    case HBondType::GMXVersion:
                                        // atom1 -> atom3
                                        vx2 = atom3->x - atom1->x;
                                        vy2 = atom3->y - atom1->y;
                                        vz2 = atom3->z - atom1->z;
                                        break;
                                }

                                frame->image(vx2, vy2, vz2);
                                auto len2 = std::sqrt(vx2 * vx2 + vy2 * vy2 + vz2 * vz2);
                                auto cosine = (vx1 * vx2 + vy1 * vy2 + vz1 * vz2) / (len1 * len2);
                                auto ang = radian * std::acos(cosine);
                                if (std::abs(ang) <= this->angle_cutoff) {
                                    hbonds[steps]++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

public:
    HBond() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

};


void HBond::print() {
    outfile << "***************************" << endl;
    outfile << "****** Hydrogen Bond ******" << endl;
    outfile << "TYPE:";
    switch (mode) {
        case Selector::Both:
            outfile << "Both" << endl;
            outfile << "SET1:";
            for (auto i : group1) outfile << i << "  ";
            outfile << endl;
            break;
        case Selector::Donor:
            outfile << "Donor" << endl;
            outfile << "SET1:";
            for (auto i : group1) outfile << i << "  ";
            outfile << endl;
            outfile << "SET2:";
            for (auto i : group2) outfile << i << "  ";
            outfile << endl;
            break;
        case Selector::Acceptor:
            outfile << "Acceptor" << endl;
            outfile << "SET1:";
            for (auto i : group1) outfile << i << "  ";
            outfile << endl;
            outfile << "SET2:";
            for (auto i : group2) outfile << i << "  ";
            outfile << endl;
            break;
    }
    switch (hbond_type) {
        case HBondType::VMDVerion:
            outfile << "HBond criteria : VMD version" << endl;
            break;
        case HBondType::GMXVersion:
            outfile << "HBond criteria : GMX version" << endl;
            break;
    }
    outfile << "distance cutoff : " << this->donor_acceptor_dist_cutoff << endl;
    outfile << "angle cutoff : " << this->angle_cutoff << endl;
    outfile << "***************************" << endl;
    for (int cyc = 1; cyc <= steps; cyc++) {
        outfile << cyc << "   " << hbonds[cyc] << endl;
    }
    outfile << "***************************" << endl;
}

void HBond::process(std::shared_ptr<Frame> &frame) {
    steps++;
    hbonds[steps] = 0;
    switch (mode) {
        case Selector::Both:
            Selector_Both(frame);
            break;
        case Selector::Donor:
            Selector_Donor(frame);
            break;
        case Selector::Acceptor:
            Selector_Acceptor(frame);
            break;
    }

}


void HBond::readInfo() {

    auto input_line = input("Please enter group:");
    auto field = split(input_line);
    for (auto it = field.cbegin(); it != field.cend(); ++it) {
        auto i = std::stoi(*it);
        if (i > 0) {
            group1.insert(i);
        } else if (i < 0) {
            ++it;
            auto j = std::stoi(*it);
            for (int k = abs(i); k <= j; k++) group1.insert(k);
        } else {
            cerr << "error of atom num select" << endl;
            exit(1);
        }
    }
    input_line = input("Which selector: [(1)Acceptor | (2)Donor | (3)Both]:");
    auto mode_no = stoi(input_line);
    switch (mode_no) {
        case 1:
            this->mode = Selector::Acceptor;
            break;
        case 2:
            this->mode = Selector::Donor;
            break;
        case 3:
            this->mode = Selector::Both;
            break;
        default:
            cerr << "Error type" << endl;
            exit(1);
    }
    if (this->mode not_eq Selector::Both) {
        input_line = input("Please enter group2:");
        field = split(input_line);
        for (auto it = field.cbegin(); it != field.cend(); ++it) {
            auto i = std::stoi(*it);
            if (i > 0) {
                group2.insert(i);
            } else if (i < 0) {
                ++it;
                auto j = std::stoi(*it);
                for (int k = abs(i); k <= j; k++) group2.insert(k);
            } else {
                cerr << "error of atom num select" << endl;
                exit(1);
            }
        }
    }
    cout << "HBond criteria:\n(1)VMD version\n(2)GMX version\n";
    switch (choose(1, 2, "choose:")) {
        case 1:
            hbond_type = HBondType::VMDVerion;
            break;
        case 2:
            hbond_type = HBondType::GMXVersion;
            break;
        default:
            cerr << "wrong type! \n";
            exit(2);
    }

    input_line = input("Donor-Acceptor Distance:");
    this->donor_acceptor_dist_cutoff = std::stod(input_line);
    input_line = input("Angle cutoff:");
    this->angle_cutoff = std::stod(input_line);
    enable_forcefield = true;
}

class ResidenceTime : public BasicAnalysis {

    double dis_cutoff;
    size_t steps = 0;
    int atom_num = 0;
    int *time_array = nullptr;
    double *Rt_array = nullptr;
    int **mark = nullptr;
    double time_star = 0;

    int typ1 = 0, typ2 = 0;
    std::string type_name1, type_name2;

    bool use_name = false;

    std::map<int, std::list<bool>> mark_map;

    void calculate();

    void setSteps(size_t steps, int atom_num);

public:
    ResidenceTime() {
        enable_tbb = true;
        enable_outfile = true;
    }

    ~ResidenceTime() {
        delete[] time_array;
        delete[] Rt_array;
        if (mark) {
            for (int i = 0; i < atom_num; ++i)
                delete[] mark[i];
            delete[] mark;

        }
    };

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

};

void ResidenceTime::setSteps(size_t steps, int atom_num) {
    this->steps = steps;
    delete[] time_array;
    time_array = new int[steps - 1];
    bzero(time_array, sizeof(int) * (steps - 1));

    delete[] Rt_array;
    Rt_array = new double[steps - 1];
    bzero(Rt_array, sizeof(double) * (steps - 1));
    this->atom_num = atom_num;
    mark = new int *[atom_num];
    for (int i = 0; i < atom_num; ++i)
        mark[i] = new int[steps];

}

void ResidenceTime::calculate() {

    auto atom_star_map = new std::list<pair<unsigned int, unsigned int>>[atom_num];
    for (int atom = 0; atom < atom_num; atom++) {
        unsigned int count1 = 0;
        bool swi = true;
        for (unsigned int k = 0; k < steps; k++) {
            if ((!mark[atom][k]) && swi) {
                swi = false;
                count1 = k;
            } else if (mark[atom][k] && (!swi)) {
                swi = true;
                atom_star_map[atom].emplace_back(count1, k - 1);
            }
        }

    }
    class Body {
    public:
        size_t steps;
        int atom_num;
        int *time_array = nullptr;
        double *Rt_array = nullptr;
        int **mark = nullptr;
        double time_star;
        std::list<std::pair<unsigned int, unsigned int>> *atom_star_map;

        Body(size_t &steps, int &atom_num, int *time_array,
             double *Rt_array, int **mark, double time_star,
             std::list<std::pair<unsigned int, unsigned int>> *atom_star_map) :
                steps(steps), atom_num(atom_num), mark(mark),
                time_star(time_star), atom_star_map(atom_star_map) {
            this->time_array = new int[steps - 1];
            bzero(this->time_array, sizeof(int) * (steps - 1));
            this->Rt_array = new double[steps - 1];
            bzero(this->Rt_array, sizeof(double) * (steps - 1));
        }

        Body(Body &body, tbb::split) :
                steps(body.steps), atom_num(body.atom_num), mark(body.mark),
                time_star(body.time_star), atom_star_map(body.atom_star_map) {
            this->time_array = new int[body.steps - 1];
            bzero(this->time_array, sizeof(int) * (body.steps - 1));
            this->Rt_array = new double[body.steps - 1];
            bzero(this->Rt_array, sizeof(double) * (body.steps - 1));
        }

        ~Body() {
            delete[] time_array;
            delete[] Rt_array;
        }

        void join(const Body &y) {
            for (unsigned int step = 0; step < steps - 1; step++) {
                time_array[step] += y.time_array[step];
                Rt_array[step] += y.Rt_array[step];
            }
        }

        void operator()(const tbb::blocked_range<size_t> &r) const {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                int CN = 0;
                for (int atom = 0; atom < atom_num; atom++)
                    CN += mark[atom][i];
                for (size_t j = i + 1; j < steps; j++) {
                    double value = 0.0;
                    for (int atom = 0; atom < atom_num; atom++) {
                        if (mark[atom][i] && mark[atom][j]) {
                            auto &li = atom_star_map[atom];
                            unsigned int maxcount = 0;
                            for (auto &pi : li) {
                                unsigned int a = pi.first;
                                unsigned int b = pi.second;
                                if (i < a and j < b) continue;
                                else if (i < a and b < j) {
                                    if (maxcount < b - a) maxcount = b - a;
                                } else if (a < i and b < j) continue;
                                else {
                                    cerr << fmt::sprintf(" i = %d, j = %d, a = %d, d = %d\n", i, j, a, b);
                                    cerr << "error " << __FILE__ << " : " << __LINE__ << endl;
                                    exit(1);
                                }
                            }
                            if (maxcount <= time_star) value++;
                        }
                    }
                    time_array[j - i - 1]++;
                    Rt_array[j - i - 1] += value / CN;
                }
            }

        }
    } body(steps, atom_num, time_array, Rt_array, mark, time_star, atom_star_map);
    tbb::parallel_reduce(tbb::blocked_range<size_t>(0, steps - 1), body, tbb::auto_partitioner());
    for (unsigned int step = 0; step < steps - 1; step++) {
        Rt_array[step] = body.Rt_array[step];
        time_array[step] = body.time_array[step];
    }

    for (unsigned int i = 0; i < steps - 1; i++)
        Rt_array[i] /= time_array[i];
    delete[] atom_star_map;
}


void ResidenceTime::process(std::shared_ptr<Frame> &frame) {
    steps++;
    int atom_no = 0;
    for (auto &atom1 : frame->atom_list) {
        if (use_name ? atom1->type_name == type_name1 : atom1->typ == typ1) {
            for (auto &atom2 : frame->atom_list) {
                if (use_name ? atom2->type_name == type_name2 : atom2->typ == typ2) {
                    double xr = atom1->x - atom2->x;
                    double yr = atom1->y - atom2->y;
                    double zr = atom1->z - atom2->z;
                    frame->image(xr, yr, zr);
                    double dist = std::sqrt(xr * xr + yr * yr + zr * zr);
                    try {
                        mark_map[atom_no].push_back(dist <= dis_cutoff);
                    } catch (std::out_of_range &e) {
                        mark_map[atom_no] = std::list<bool>();
                        mark_map[atom_no].push_back(dist <= dis_cutoff);
                    }
                    atom_no += 1;
                }
            }
        }
    }
}

void ResidenceTime::print() {
    if (steps < 2) {
        cerr << "Too few frame number :" << steps << endl;
        exit(1);
    }
    setSteps(steps, static_cast<int>(mark_map.size()));
    for (const auto &it : mark_map) {
        auto atom_no = it.first;
        int cyc = 0;
        for (auto value : it.second) {
            mark[atom_no][cyc] = int(value);
            cyc++;
        }
    }
    calculate();
    if (use_name) {
        outfile << "typ1: " << type_name1 << ",  typ2: " << type_name2 << "  dist_cutoff = " << dis_cutoff << " t* = "
                << time_star
                << std::endl;
    } else {
        outfile << "typ1: " << typ1 << ",  typ2: " << typ2 << "  dist_cutoff = " << dis_cutoff << " t* = " << time_star
                << std::endl;
    }
    outfile << "# Frame        R " << std::endl;
    for (unsigned int i = 0; i < steps - 1; i++)
        outfile << i + 1 << "    " << Rt_array[i] << std::endl;
}

void ResidenceTime::readInfo() {

    begin:
    std::cout << "(1) use name\n(2) use number\n";
    int num;
    std::cin >> num;
    if (num == 1) {
        use_name = true;
    } else if (num == 2) {
        use_name = false;
    } else {
        std::cerr << "wront number" << std::endl;
        goto begin;
    }

    std::cout << "Please enter typ1:";
    if (use_name) {
        std::cin >> type_name1;
        boost::trim(type_name1);
    } else {
        std::cin >> typ1;
    }
    std::cout << "Please enter typ2:";
    if (use_name) {
        std::cin >> type_name2;
        boost::trim(type_name2);
    } else {
        std::cin >> typ2;
    }

    std::cout << "Please enter distance cutoff1:";
    std::cin >> dis_cutoff;
    std::cout << "Please enter t*: ( unit: frame)";
    std::cin >> time_star;

}


// Use Green-Kubo equation to calculate self-diffuse coefficients
class GreenKubo : public BasicAnalysis {

    list<int> atom_seqs_list;
    //        list<double> atom_mass_list;
    double timestep;
    size_t steps = 0;

    map<int, double> vecx_map;
    map<int, double> vecy_map;
    map<int, double> vecz_map;

public:
    GreenKubo() {
        enable_read_velocity = true;
        enable_tbb = true;
        enable_outfile = true;
    }

    void process(shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;
};


void GreenKubo::print() {

    if (steps < 2) {
        cerr << "Too few frame number :" << steps << endl;
        exit(1);
    }
    auto vecx = new double[steps];
    auto vecy = new double[steps];
    auto vecz = new double[steps];
    for (unsigned int i = 0; i < steps; i++) {
        vecx[i] = vecx_map[i + 1];
        vecy[i] = vecy_map[i + 1];
        vecz[i] = vecz_map[i + 1];
    }

    class Body {
    public:
        double *vecx;
        double *vecy;
        double *vecz;

        double *cxx = nullptr;
        double *cyy = nullptr;
        double *czz = nullptr;
        int *numbers = nullptr;
        size_t steps;

        Body(double *vecx, double *vecy, double *vecz, size_t steps) :
                vecx(vecx), vecy(vecy), vecz(vecz), steps(steps) {
            allocate();
        }

        Body(const Body &c, tbb::split) :
                vecx(c.vecx), vecy(c.vecy), vecz(c.vecz), steps(c.steps) {
            allocate();
        }

        void allocate() {
            cxx = new double[steps];
            cyy = new double[steps];
            czz = new double[steps];
            numbers = new int[steps];
            bzero(cxx, sizeof(double) * steps);
            bzero(cyy, sizeof(double) * steps);
            bzero(czz, sizeof(double) * steps);
            bzero(numbers, sizeof(int) * steps);
        }

        ~Body() {
            delete[] cxx;
            delete[] cyy;
            delete[] czz;
            delete[] numbers;
        }

        void join(const Body &y) {
            for (unsigned int step = 0; step < steps - 1; step++) {
                cxx[step] += y.cxx[step];
                cyy[step] += y.cyy[step];
                czz[step] += y.czz[step];
                numbers[step] += y.numbers[step];
            }
        }

        void operator()(const tbb::blocked_range<size_t> &r) const {
            for (size_t i = r.begin(); i != r.end(); i++) {
                for (size_t j = i; j < steps; j++) {
                    cxx[j - i] += vecx[i] * vecx[j];
                    cyy[j - i] += vecy[i] * vecy[j];
                    czz[j - i] += vecz[i] * vecz[j];
                    numbers[j - i]++;
                }
            }
        }


    } body(vecx, vecy, vecz, steps);

    tbb::parallel_reduce(tbb::blocked_range<size_t>(0, steps - 1), body, tbb::auto_partitioner());


    for (unsigned int i = 0; i < steps; i++) {
        int no = body.numbers[i];
        body.cxx[i] /= no;
        body.cyy[i] /= no;
        body.czz[i] /= no;
    }
    outfile << "Green-Kubo self-diffuse " << endl;
    outfile << "Time (ps)             DA(10^-9 m2/s)" << endl;
    double pre_c = body.cxx[0] + body.cyy[0] + body.czz[0];
    double integral = 0.0;
    for (unsigned int i = 1; i < steps; i++) {
        double c = body.cxx[i] + body.cyy[i] + body.czz[i];
        integral += 5 * (pre_c + c) * timestep / 3;
        pre_c = c;
        outfile << i * timestep << "    " << integral << endl;
    }
    delete[] vecx;
    delete[] vecy;
    delete[] vecz;
}


void GreenKubo::readInfo() {
    string value;
    while ((value = input("Please enter atom no seq(s):")).empty());
    auto field = split(value);
    for (auto &num : field) {
        atom_seqs_list.push_back(stoi(num));
    }
    //   while(!(value = input("Please enter mass for each atom:").size()));
    //   field = spilt(value);
    //   for (auto &mass : field){
    //       atom_mass_list.push_back(stod(mass));
    //   }
    while ((value = input("Please enter time step for each frame(ps):")).empty());
    timestep = stod(value);


}

void GreenKubo::process(shared_ptr<Frame> &frame) {
    steps++;
    double xv, yv, zv;
    xv = yv = zv = 0.0;
    for (auto &atom_ptr : frame->atom_list) {
        for (auto &atom_no : atom_seqs_list) {
            if (atom_ptr->seq == atom_no) {
                xv += atom_ptr->vx;
                yv += atom_ptr->vy;
                zv += atom_ptr->vz;
                break;
            }
        }
    }

    int atom_nums = static_cast<int>(atom_seqs_list.size());
    xv /= atom_nums;
    yv /= atom_nums;
    zv /= atom_nums;

    vecx_map[steps] = xv;
    vecy_map[steps] = yv;
    vecz_map[steps] = zv;

}

class Cluster : public BasicAnalysis {
public:
    std::set<int> set1; // contain the NO. of fitting atom
    double cutoff = 0.0;
    int steps = 0; // current frame number
    map<pair<int, int>, double> rmsd_map;


    double rmsfit(double x1[], double y1[], double z1[],
                  double x2[], double y2[], double z2[], int nfit) {
        return RMSDCal::rmsfit(x1, y1, z1, x2, y2, z2, nfit);
    }

    void jacobi(int n, double a[4][4], double d[], double v[4][4]) {
        RMSDCal::jacobi(n, a, d, v);
    }

    void quatfit(int n1, double x1[], double y1[], double z1[],
                 int n2, double x2[], double y2[], double z2[], int nfit) {
        RMSDCal::quatfit(n1, x1, y1, z1, n2, x2, y2, z2, nfit);
    }

    void center(int n1, double x1[], double y1[], double z1[],
                int n2, double x2[], double y2[], double z2[],
                double mid[], int nfit) {
        RMSDCal::center(n1, x1, y1, z1, n2, x2, y2, z2, mid, nfit);
    }

    map<int, map<int, double>> x, y, z;

    double rmsvalue(int index1, int index2);

public:
    Cluster() {
        enable_tbb = true;
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;
};


void Cluster::process(std::shared_ptr<Frame> &frame) {

    map<int, double> xx, yy, zz;
    int index = 0;
    bool first_atom = true;
    double first_x, first_y, first_z;
    for (auto i : this->set1) {
        auto atom = frame->atom_map[i];
        if (first_atom) {
            first_atom = false;
            first_x = xx[index] = atom->x;
            first_y = yy[index] = atom->y;
            first_z = zz[index] = atom->z;
        } else {
            double xr = atom->x - first_x;
            double yr = atom->y - first_y;
            double zr = atom->z - first_z;
            frame->image(xr, yr, zr);
            xx[index] = first_x + xr;
            yy[index] = first_y + yr;
            zz[index] = first_z + zr;
        }
        index++;
    }
    x[steps] = xx;
    y[steps] = yy;
    z[steps] = zz;
    steps++;
}

double Cluster::rmsvalue(int index1, int index2) {
    int nfit, n;
    nfit = n = static_cast<int>(this->set1.size());

    double x1[ATOM_MAX], y1[ATOM_MAX], z1[ATOM_MAX];
    double x2[ATOM_MAX], y2[ATOM_MAX], z2[ATOM_MAX];


    auto &f1x = x[index1];
    auto &f1y = y[index1];
    auto &f1z = z[index1];
    for (int i = 0; i < n; i++) {
        x1[i] = f1x[i];
        y1[i] = f1y[i];
        z1[i] = f1z[i];

    }

    auto &f2x = x[index2];
    auto &f2y = y[index2];
    auto &f2z = z[index2];
    for (int i = 0; i < n; i++) {
        x2[i] = f2x[i];
        y2[i] = f2y[i];
        z2[i] = f2z[i];

    }

    double mid[3];
    center(n, x1, y1, z1, n, x2, y2, z2, mid, nfit);
    quatfit(n, x1, y1, z1, n, x2, y2, z2, nfit);
    double rms = rmsfit(x1, y1, z1, x2, y2, z2, nfit);
    return rms;
}

class rms_m {
public:
    rms_m(int i, int j, double rms) : i(i), j(j), rms(rms) {}

    int i;
    int j;
    double rms;
};

class rms_m2 {
public:
    rms_m2(int conf, int clust) : conf(conf), clust(clust) {};
    int conf;
    int clust;
};

void Cluster::print() {

    //  list<rms_m> rms_list;


    class CalCore {
    public:
        list<rms_m> local_rms_list;
        int steps;
        double cutoff;

        Cluster *parent;

        CalCore(int steps, double cutoff, Cluster *parent) : steps(steps), cutoff(cutoff), parent(parent) {};

        CalCore(CalCore &c, tbb::split) {
            steps = c.steps;
            cutoff = c.cutoff;
            parent = c.parent;
        }

        void join(CalCore &c) {
            local_rms_list.merge(c.local_rms_list,
                                 [](const rms_m &m1, const rms_m &m2) { return (m1.rms < m2.rms); });
        }

        void operator()(const tbb::blocked_range<int> &range) {
            double rms;
            for (int index1 = range.begin(); index1 != range.end(); ++index1) {
                for (int index2 = index1 + 1; index2 < steps; ++index2) {
                    rms = parent->rmsvalue(index1, index2);
                    if (rms < cutoff) local_rms_list.emplace_back(index1, index2, rms);
                }
            }
            local_rms_list.sort([](const rms_m &m1, const rms_m &m2) { return (m1.rms < m2.rms); });
        }
    } core(steps, cutoff, this);

    tbb::parallel_reduce(tbb::blocked_range<int>(0, steps - 1), core, tbb::auto_partitioner());

    //    cout << "Sorting rms values... ("<< core.local_rms_list.size()<<" numbers)";
    //    rms_m** vect = new rms_m*[core.local_rms_list.size()];

    //    int num = 0;
    //    for (auto &k : core.local_rms_list){
    //        vect[num] = &k;
    //        if (k.i == 0 and k.j < 100) cout << k.j <<"  " << k.rms << endl;
    //        num++;
    //    }
    //
    //    tbb::parallel_sort(vect,vect+num, [](const rms_m *m1, const rms_m *m2){return (m1->rms < m2->rms);});
    //    for (int n =0 ; n< 100; n++){
    //        cout <<vect[n]->i << "  " <<  vect[n]->j <<"  " << vect[n]->rms << endl;
    //    }
    //    cout << vect[num-1]->i << "  " << vect[num-1]->j <<"  " <<vect[num-1]->rms << endl;
    //  core.local_rms_list.sort([](const rms_m &m1, const rms_m &m2){return (m1.rms < m2.rms);});


    //    for (int index1 = 0; index1 < 100; ++index1){
    //        cout << rmsvalue(0,index1) << endl;
    //    }

    //    for (int index1 = 0; index1 < steps - 1; ++index1) {
    //        for (int index2 = index1 + 1; index2 < steps; ++index2) {
    //            rms_list.emplace_back(index1, index2, rmsvalue(index1, index2));
    //        }
    //    }

    //    rms_list.sort([](const rms_m &m1, const rms_m &m2){return (m1.rms < m2.rms);});
    //   cout << "    Done" << endl;

    int n = 0;
    for (auto &v : core.local_rms_list) {
        cout << v.i << "  " << v.j << "  " << v.rms << endl;
        n++;
        if (n > 99) break;
    }
    vector<rms_m2> c;
    for (int i = 0; i < this->steps; i++) {
        c.emplace_back(i, i);
    }
    bool bChange;
    do {
        bChange = false;
        for (auto &k : core.local_rms_list) {
            //        for(int n = 0; n < num; n++){
            //            rms_m k = *(vect[n]);
            if (k.rms >= this->cutoff) break;
            int diff = c[k.j].clust - c[k.i].clust;
            if (diff) {
                bChange = true;
                if (diff > 0) {
                    c[k.j].clust = c[k.i].clust;
                } else {
                    c[k.i].clust = c[k.j].clust;
                }
            }
        }
    } while (bChange);
    //   delete [] vect;
    cout << "Sorting and renumbering clusters..." << endl;
    tbb::parallel_sort(c.begin(), c.end(), [](const rms_m2 &i, const rms_m2 &j) { return (i.clust < j.clust); });
    int cid = 1;
    unsigned int k;
    for (k = 1; k < c.size(); k++) {
        if (c[k].clust != c[k - 1].clust) {
            c[k - 1].clust = cid;
            cid++;
        } else {
            c[k - 1].clust = cid;
        }
    }
    c[k - 1].clust = cid;
    tbb::parallel_sort(c.begin(), c.end(), [](const rms_m2 &i, const rms_m2 &j) { return (i.conf < j.conf); });
    outfile << "***************************" << endl;
    outfile << "*Cluster Analysis(Linkage)*" << endl;
    outfile << "SET:";
    for (auto i : this->set1) {
        outfile << i << "  ";
    }
    outfile << endl << "cutoff : " << this->cutoff << endl;
    outfile << "***************************" << endl;
    for (unsigned int k = 0; k < c.size(); k++) {
        outfile << k + 1 << "   " << c[k].clust << endl;
    }
    outfile << "***************************" << endl;

}

void Cluster::readInfo() {
    string str = input("Please enter group:");
    auto vec1 = split(str);
    for (auto it = vec1.cbegin(); it != vec1.cend(); ++it) {
        auto i = std::stoi(*it);
        if (i > 0) {
            set1.insert(i);
        } else if (i < 0) {
            ++it;
            auto j = std::stoi(*it);
            for (int k = -i; k <= j; k++) set1.insert(k);
        } else {
            cerr << "error of atom num select" << endl;
            exit(1);
        }
    }
    str = input("Cutoff : ");
    this->cutoff = std::stod(str);

}


enum class AminoAcidType {
    H3N_Ala, H3N_Gly, H3N_Ile, H3N_Leu, H3N_Pro, H3N_Val, H3N_Phe, H3N_Trp, H3N_Tyr, H3N_Asp,
    H3N_Glu, H3N_Arg, H3N_His, H3N_Lys, H3N_Ser, H3N_Thr, H3N_Cys, H3N_Met, H3N_Asn, H3N_Gln,
    Ala, Gly, Ile, Leu, Pro, Val, Phe, Trp, Tyr, Asp,
    Glu, Arg, His, Lys, Ser, Thr, Cys, Met, Asn, Gln
};


std::map<string, AminoAcidType> str_to_aminotype;
std::map<AminoAcidType, string> aminotype_to_str;

class AminoAcid {
public:
    AminoAcidType type;
    map<int, string> atom_no_map;
    int sequence_no;

};

class AminoTop {
public:
    class AminoItem {
    public:
        int no;
        Symbol symbol;
        std::list<int> linked_atom_nos;
        shared_ptr<Atom> atom;
        string H_;

    };

    std::map<int, std::shared_ptr<AminoItem>> topmap;

    void atom_null() {
        for (auto item : topmap) {
            item.second->atom.reset();
        }
    }

    AminoAcidType type;

    void readTop(const string &filename) {
        fstream f;
        f.open(filename);
        string line;
        while (true) {
            getline(f, line);
            auto field = split(line);
            if (field.empty()) break;
            auto item = make_shared<AminoItem>();
            item->no = stoi(field[0]);
            auto sym = field[1];
            if (sym == "H") item->symbol = Symbol::Hydrogen;
            else if (sym == "C") item->symbol = Symbol::Carbon;
            else if (sym == "N") item->symbol = Symbol::Nitrogen;
            else if (sym == "O") item->symbol = Symbol::Oxygen;
            else if (sym == "X") item->symbol = Symbol::X;


            item->H_ = field[2];

            for (unsigned int i = 3; i < field.size(); i++) {
                item->linked_atom_nos.push_back(stoi(field[i]));
            }
            topmap[item->no] = item;

        }

    }
};

class NMRRange : public BasicAnalysis {


    void recognize_amino_acid(std::shared_ptr<Frame> &frame);

    bool recognize_walk(shared_ptr<Atom> atom, shared_ptr<AminoTop::AminoItem> item,
                        AminoTop &top, shared_ptr<Frame> &frame,
                        list<shared_ptr<Atom>> &atom_list, list<shared_ptr<AminoTop::AminoItem>> &item_list);

    bool first_frame = true;


    void loadTop() {
        string path = std::getenv("ANALYSIS_TOP_PATH");
        for (auto t : aminotype_to_str) {
            AminoTop top;
            top.readTop(path + "/" + t.second + ".top");
            top.type = t.first;
            amino_top_list.push_back(top);
        }

    }

    list<AminoTop> amino_top_list;

    list<shared_ptr<AminoAcid>> amino_acid_list;

    map<pair<int, int>, list<double>> dist_range_map;

    map<int, string> name_map;

public:
    NMRRange() {
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;
};

void NMRRange::process(std::shared_ptr<Frame> &frame) {
    if (first_frame) {
        loadTop();
        recognize_amino_acid(frame);
        first_frame = false;
        vector<int> need_to_calc_list;
        for (auto &aminoacid : amino_acid_list) {
            for (auto &item : aminoacid->atom_no_map)
                need_to_calc_list.push_back(item.first);
        }
        for (size_t i = 0; i < need_to_calc_list.size() - 1; i++) {
            for (size_t j = i + 1; j < need_to_calc_list.size(); j++) {
                dist_range_map[make_pair(need_to_calc_list[i], need_to_calc_list[j])] = list<double>();
                dist_range_map[make_pair(need_to_calc_list[i], need_to_calc_list[j])].push_back(
                        atom_distance(frame->atom_map[need_to_calc_list[i]], frame->atom_map[need_to_calc_list[j]],
                                      frame));
            }
        }
    } else {
        for (auto &item : dist_range_map) {
            item.second.push_back(
                    atom_distance(frame->atom_map[item.first.first], frame->atom_map[item.first.second], frame));
        }
    }

}


void NMRRange::print() {

    for (auto &item : dist_range_map) {

        double sum = 0.0;
        int count = 0;
        for (auto v : item.second) {
            sum += pow(v, -6);
            count++;
        }
        double avg = sum / count;
        double e = -1.0 / 6;
        double value = pow(avg, e);
        outfile << name_map[item.first.first] << " <->\t"
                << name_map[item.first.second] << "\t" << value << endl;
    }
}

void NMRRange::readInfo() {
    enable_forcefield = true;

}


void NMRRange::recognize_amino_acid(std::shared_ptr<Frame> &frame) {
    cout << "First Frame, Analyze..." << endl;
    int amino_seq_no = 0;
    for (auto atom : frame->atom_list) {
        if (which(atom) == Symbol::Nitrogen) {
            for (auto &amino_top : amino_top_list) {
                list<shared_ptr<Atom>> child_atom_list;
                list<shared_ptr<AminoTop::AminoItem>> child_item_list;
                bool match = recognize_walk(atom, amino_top.topmap[1], amino_top, frame,
                                            child_atom_list, child_item_list);


                if (match) {
                    amino_seq_no++;
                    cout << aminotype_to_str[amino_top.type] << "   " << endl;

                    auto new_amino = make_shared<AminoAcid>();
                    new_amino->type = amino_top.type;
                    new_amino->sequence_no = amino_seq_no;

                    for (auto &item :amino_top.topmap) {
                        if (item.second->symbol == Symbol::Hydrogen) {
                            new_amino->atom_no_map[item.second->atom->seq] = item.second->H_;

                            name_map[item.second->atom->seq] = to_string(item.second->atom->seq) + ":"
                                                               + aminotype_to_str[amino_top.type]
                                                               + ":" + to_string(amino_seq_no) + " " + item.second->H_;
                        }
                        cout << item.first << " " << item.second->H_ <<
                             "," << item.second->atom->seq << " " << item.second->atom->symbol << endl;
                        //if (item.second->symbol == Symbol::X)
                        item.second->atom->mark = false;
                    }
                    amino_acid_list.push_back(new_amino);
                    amino_top.atom_null();
                    break;
                }


            }
        }
    }
    cout << endl;

}

bool NMRRange::recognize_walk(shared_ptr<Atom> atom, shared_ptr<AminoTop::AminoItem> item,
                              AminoTop &top, shared_ptr<Frame> &frame,
                              list<shared_ptr<Atom>> &atom_list,
                              list<shared_ptr<AminoTop::AminoItem>> &item_list) {
    if (item->symbol == Symbol::X) {
        atom->mark = true;
        item->atom = atom;
        atom_list.push_back(atom);
        item_list.push_back(item);
        return true;
    }
    if (which(atom) != item->symbol) {
        return false;
    }
    if (atom->con_list.size() != item->linked_atom_nos.size()) {

        return false;
    }
    atom->mark = true;
    item->atom = atom;

    bool match = true;
    list<shared_ptr<Atom>> child_atom_list;
    list<shared_ptr<AminoTop::AminoItem>> child_item_list;
    for (int i : atom->con_list) {
        auto next_atom = frame->atom_map[i];
        if (next_atom->mark) continue;
        bool ok = false;

        for (int j : item->linked_atom_nos) {
            auto next_item = top.topmap[j];
            if (next_item->atom) continue;
            ok = recognize_walk(next_atom, next_item, top, frame, child_atom_list, child_item_list);
            if (ok) break;
        }

        match = ok && match;
        if (!match) break;
    }
    if (match) {
        atom_list.push_back(atom);
        item_list.push_back(item);
        for (auto &child_atom: child_atom_list) {
            atom_list.push_back(child_atom);
        }
        for (auto &child_item : child_item_list) {
            item_list.push_back(child_item);
        }

        return true;
    }

    atom->mark = false;
    item->atom.reset();

    for (auto &child_atom: child_atom_list) {
        child_atom->mark = false;
    }
    for (auto &child_item : child_item_list) {
        child_item->atom.reset();
    }

    return false;
}

class RotAcfCutoff : public BasicAnalysis {
public:
    double time_increment_ps = 0.1;
    double cutoff2;

    std::vector<Atom::AtomIndenter> ids1;
    std::vector<Atom::AtomIndenter> ids2;

    explicit RotAcfCutoff() {
        enable_outfile = true;
        enable_forcefield = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

    struct InnerAtom {
        int index;
        std::list<std::tuple<double, double, double>> *list_ptr = nullptr;

        InnerAtom(int index, std::list<std::tuple<double, double, double>> *list_ptr)
                : index(index), list_ptr(list_ptr) {}
    };

    struct InnerAtomHasher {
        typedef InnerAtom argument_type;
        typedef std::size_t result_type;

        result_type operator()(argument_type const &s) const noexcept {
            result_type seed = 0;
            boost::hash_combine(seed, s.index);
            boost::hash_combine(seed, s.list_ptr);
            return seed;
        }
    };

    std::unordered_set<InnerAtom, InnerAtomHasher> inner_atoms;

    std::list<std::list<std::tuple<double, double, double>> *> rots;

    auto find_in(int seq);

    tuple<double, double, double> calVector(shared_ptr<Molecule> &mol, shared_ptr<Frame> &frame);

};

bool operator==(const RotAcfCutoff::InnerAtom &i1, const RotAcfCutoff::InnerAtom &i2) {
    return i1.index == i2.index and i1.list_ptr == i2.list_ptr;
}

auto RotAcfCutoff::find_in(int seq) {
    for (auto iter = inner_atoms.begin(); iter != inner_atoms.end(); ++iter) {
        if (seq == iter->index) return iter;
    }
    return inner_atoms.end();
}

void RotAcfCutoff::process(std::shared_ptr<Frame> &frame) {
    shared_ptr<Atom> ref;

    for (auto &mol : frame->molecule_list) {
        mol->bExculde = true;
        for (auto &atom : mol->atom_list) {
            if (Atom::is_match(atom, ids1)) {
                ref = atom;
            } else if (Atom::is_match(atom, ids2)) {
                mol->bExculde = false;
            }
        }
        mol->calc_mass();
    }
    if (!ref) {
        std::cerr << "reference atom not found" << std::endl;
        exit(5);
    }
    double ref_x = ref->x;
    double ref_y = ref->y;
    double ref_z = ref->z;
    for (auto &mol: frame->molecule_list) {
        if (!mol->bExculde) {
            auto coord = mol->calc_weigh_center(frame);
            double x1 = get<0>(coord);
            double y1 = get<1>(coord);
            double z1 = get<2>(coord);

            double xr = x1 - ref_x;
            double yr = y1 - ref_y;
            double zr = z1 - ref_z;

            frame->image(xr, yr, zr);

            auto it = find_in(mol->seq());
            if (xr * xr + yr * yr + zr * zr < cutoff2) {
                // in the shell
                if (it != inner_atoms.end()) {
                    it->list_ptr->push_back(calVector(mol, frame));
                } else {
                    auto list_ptr = new std::list<std::tuple<double, double, double>>();
                    list_ptr->push_back(calVector(mol, frame));
                    inner_atoms.insert(InnerAtom(mol->seq(), list_ptr));
                    rots.emplace_back(list_ptr);
                }
            } else {
                if (it != inner_atoms.end()) {
                    inner_atoms.erase(it);
                }
            }
        }
    }
}

void RotAcfCutoff::readInfo() {

    Atom::select2group(ids1, ids2);

    double cutoff = choose(0.0, GMX_DOUBLE_MAX, "Please enter distance cutoff:");
    this->cutoff2 = cutoff * cutoff;

    while (true) {
        time_increment_ps = 0.1;
        string input_line = input(" Enter the Time Increment in Picoseconds [0.1]: ");
        trim(input_line);
        if (!input_line.empty()) {
            time_increment_ps = stod(input_line);
            if (time_increment_ps <= 0.0) {
                cout << "error time increment " << time_increment_ps << endl;
                continue;
            }
        }
        break;
    }

}

tuple<double, double, double> RotAcfCutoff::calVector(shared_ptr<Molecule> &mol, shared_ptr<Frame> &frame) {
    auto iter = mol->atom_list.begin();

    auto atom_i = *iter;
    iter++;
    auto atom_j = *iter;
    iter++;
    auto atom_k = *iter;

    auto u1 = atom_i->x - atom_j->x;
    auto u2 = atom_i->y - atom_j->y;
    auto u3 = atom_i->z - atom_j->z;

    auto v1 = atom_k->x - atom_j->x;
    auto v2 = atom_k->y - atom_j->y;
    auto v3 = atom_k->z - atom_j->z;

    frame->image(u1, u2, u3);
    frame->image(v1, v2, v3);

    auto xv3 = u2 * v3 - u3 * v2;
    auto yv3 = u3 * v1 - u1 * v3;
    auto zv3 = u1 * v2 - u2 * v1;

    return std::make_tuple(xv3, yv3, zv3);
}

void RotAcfCutoff::print() {
    std::vector<std::pair<int, double>> acf;
    acf.emplace_back(0, 0.0);
    for (auto list_ptr : this->rots) {
        int i = 0;
        for (auto it1 = list_ptr->begin(); it1 != --list_ptr->end(); it1++) {
            i++;
            int j = i;
            auto it2 = it1;
            for (it2++; it2 != list_ptr->end(); it2++) {
                j++;
                int m = j - i;
                if (m >= acf.size()) acf.emplace_back(0, 0.0);

                double xr1 = get<0>(*it1);
                double yr1 = get<1>(*it1);
                double zr1 = get<2>(*it1);

                double xr2 = get<0>(*it2);
                double yr2 = get<1>(*it2);
                double zr2 = get<2>(*it2);


                double r1_2 = xr1 * xr1 + yr1 * yr1 + zr1 * zr1;
                double r2_2 = xr2 * xr2 + yr2 * yr2 + zr2 * zr2;

                double dot = xr1 * xr2 + yr1 * yr2 + zr1 * zr2;

                double cos = dot / std::sqrt(r1_2 * r2_2);

                acf[m].second += cos;
                acf[m].first++;
            }
        }
    }

    for (int i = 1; i < acf.size(); i++) {
        double counts = acf[i].first;
        acf[i].second /= counts;
    }

    acf[0].second = 1.0;
    // intergrate;

    std::vector<double> integrate(acf.size());
    integrate[0] = 0.0;

    for (std::size_t i = 1; i < integrate.size(); i++) {
        integrate[i] = integrate[i - 1] + 0.5 * (acf[i - 1].second + acf[i].second) * time_increment_ps;
    }

    outfile << "*********************************************************" << endl;
    outfile << "cutoff : " << std::sqrt(cutoff2) << std::endl;
    outfile << "First Type : " << ids1 << " Second Type : " << ids2 << endl;
    outfile << " rotational autocorrelation function" << endl;

    outfile << "    Time Gap      ACF       intergrate" << endl;
    outfile << "      (ps)                    (ps)" << endl;

    for (std::size_t t = 0; t < acf.size(); t++) {
        outfile << boost::format("%12.2f%18.14f%15.5f") % (t * time_increment_ps) % acf[t].second % integrate[t]
                << endl;
    }
    outfile << "*********************************************************" << endl;

}


class DiffuseCutoff : public BasicAnalysis {
public:
    double time_increment_ps = 0.1;
    double cutoff2;

    std::vector<Atom::AtomIndenter> ids1;
    std::vector<Atom::AtomIndenter> ids2;


    DiffuseCutoff() {
        enable_outfile = true;
        enable_forcefield = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

    struct InnerAtom {
        int index;
        std::list<std::tuple<double, double, double>> *list_ptr = nullptr;

        InnerAtom(int index, std::list<std::tuple<double, double, double>> *list_ptr)
                : index(index), list_ptr(list_ptr) {}
    };

    struct InnerAtomHasher {
        typedef InnerAtom argument_type;
        typedef std::size_t result_type;

        result_type operator()(argument_type const &s) const noexcept {
            result_type
            const h1 ( std::hash<int>()
            (s.index));
            result_type
            const h2 ( std::hash<std::list<std::tuple<double, double, double>> *>()
            (s.list_ptr));
            return h1 ^ (h2 << 1); // or use boost::hash_combine (see Discussion)
        }
    };

    std::unordered_set<InnerAtom, InnerAtomHasher> inner_atoms;

    std::list<std::list<std::tuple<double, double, double>> *> rcm;

    auto find_in(int seq);

};

bool operator==(const DiffuseCutoff::InnerAtom &i1, const DiffuseCutoff::InnerAtom &i2) {
    return i1.index == i2.index and i1.list_ptr == i2.list_ptr;
}

auto DiffuseCutoff::find_in(int seq) {
    for (auto iter = inner_atoms.begin(); iter != inner_atoms.end(); ++iter) {
        if (seq == iter->index) return iter;
    }
    return inner_atoms.end();
}

void DiffuseCutoff::process(std::shared_ptr<Frame> &frame) {
    shared_ptr<Atom> ref;

    for (auto &mol : frame->molecule_list) {
        mol->bExculde = true;
        for (auto &atom : mol->atom_list) {
            if (Atom::is_match(atom, ids1)) {
                ref = atom;
            } else if (Atom::is_match(atom, ids2)) {
                mol->bExculde = false;
            }
        }
        mol->calc_mass();
    }
    if (!ref) {
        std::cerr << "reference atom not found" << std::endl;
        exit(5);
    }
    double ref_x = ref->x;
    double ref_y = ref->y;
    double ref_z = ref->z;
    for (auto &mol: frame->molecule_list) {
        if (!mol->bExculde) {
            auto coord = mol->calc_weigh_center(frame);
            double x1 = get<0>(coord);
            double y1 = get<1>(coord);
            double z1 = get<2>(coord);

            double xr = x1 - ref_x;
            double yr = y1 - ref_y;
            double zr = z1 - ref_z;

            frame->image(xr, yr, zr);

            auto it = find_in(mol->seq());
            if (xr * xr + yr * yr + zr * zr < cutoff2) {
                // in the shell
                if (it != inner_atoms.end()) {
                    auto &old = it->list_ptr->back();

                    double xold = get<0>(old);
                    double yold = get<1>(old);
                    double zold = get<2>(old);

                    xr = x1 - xold;
                    yr = y1 - yold;
                    zr = z1 - zold;

                    frame->image(xr, yr, zr);

                    it->list_ptr->emplace_back(xr + xold, yr + yold, zr + zold);
                } else {
                    auto list_ptr = new std::list<std::tuple<double, double, double>>();
                    list_ptr->emplace_back(x1, y1, z1);
                    inner_atoms.insert(InnerAtom(mol->seq(), list_ptr));
                    rcm.emplace_back(list_ptr);
                }
            } else {
                if (it != inner_atoms.end()) {
                    inner_atoms.erase(it);
                }
            }

        }
    }
}

void DiffuseCutoff::print() {
    std::vector<std::pair<int, tuple<double, double, double>>> msd;
    for (auto list_ptr : this->rcm) {
        int i = 0;
        for (auto it1 = list_ptr->begin(); it1 != --list_ptr->end(); it1++) {
            i++;
            int j = i;
            auto it2 = it1;
            for (it2++; it2 != list_ptr->end(); it2++) {
                j++;
                int m = j - i - 1;
                if (m >= msd.size()) msd.emplace_back(0, std::make_tuple(0.0, 0.0, 0.0));
                double xdiff = get<0>(*it2) - get<0>(*it1);
                double ydiff = get<1>(*it2) - get<1>(*it1);
                double zdiff = get<2>(*it2) - get<2>(*it1);
                get<0>(msd[m].second) += xdiff * xdiff;
                get<1>(msd[m].second) += ydiff * ydiff;
                get<2>(msd[m].second) += zdiff * zdiff;
                msd[m].first++;
            }
        }
    }

    const double dunits = 10.0;

    for (int i = 0; i < msd.size(); i++) {
        double counts = msd[i].first;
        get<0>(msd[i].second) /= counts;
        get<1>(msd[i].second) /= counts;
        get<2>(msd[i].second) /= counts;
    }

    outfile << "*********************************************************" << endl;
    outfile << "cutoff : " << std::sqrt(cutoff2) << std::endl;
    outfile << "First Type : " << ids1 << " Second Type : " << ids2 << endl;
    outfile << "Mean Squared Displacements and Self-Diffusion Constant" << endl;
    outfile << "    Time Gap      X MSD       Y MSD       Z MSD       R MSD       Diff Const" << endl;
    outfile << "      (ps)       (Ang^2)     (Ang^2)     (Ang^2)     (Ang^2)    (x 10^-5 cm**2/sec)" << endl;

    for (int i = 0; i < msd.size(); i++) {
        double delta = time_increment_ps * (i + 1);
        double xvalue = get<0>(msd[i].second);
        double yvalue = get<1>(msd[i].second);
        double zvalue = get<2>(msd[i].second);
        double rvalue = xvalue + yvalue + zvalue;
        double dvalue = dunits * rvalue / delta / 6.0;
        outfile << fmt::sprintf("%12.2f%12.2f%12.2f%12.2f%12.2f%12.4f",
                                delta, xvalue, yvalue, zvalue, rvalue, dvalue) << endl;
    }
    outfile << "*********************************************************" << endl;
}

void DiffuseCutoff::readInfo() {

    Atom::select2group(ids1, ids2);

    double cutoff = choose(0.0, GMX_DOUBLE_MAX, "Please enter distance cutoff:");
    this->cutoff2 = cutoff * cutoff;

    while (true) {
        time_increment_ps = 0.1;
        string input_line = input(" Enter the Time Increment in Picoseconds [0.1]: ");
        trim(input_line);
        if (!input_line.empty()) {
            time_increment_ps = stod(input_line);
            if (time_increment_ps <= 0.0) {
                cout << "error time increment " << time_increment_ps << endl;
                continue;
            }
        }
        break;
    }

}


class Diffuse : public BasicAnalysis {
public:
    bool first_frame = true;
    int total_frame_number;
    int steps = 0;
    double time_increment_ps = 0.1;
    int total_mol = 0;
    set<int> exclude_atom;
    bool bSerial = true;
    bool bTradition = true;

    Diffuse() {
        enable_forcefield = true;
        enable_outfile = true;
    }

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

    Eigen::MatrixXd xcm;
    Eigen::MatrixXd ycm;
    Eigen::MatrixXd zcm;

};

void Diffuse::process(std::shared_ptr<Frame> &frame) {
    if (first_frame) {
        first_frame = false;

        for (auto &mol : frame->molecule_list) {
            for (auto &atom : mol->atom_list) {
                if (exclude_atom.count(atom->seq)) {
                    mol->bExculde = true;
                    break;
                }
            }
            if (mol->bExculde) continue;
            mol->calc_mass();
            total_mol++;
        }
        cout << "Total molecule number : " << total_mol << endl;
        xcm = Eigen::MatrixXd::Zero(total_frame_number, total_mol);
        ycm = Eigen::MatrixXd::Zero(total_frame_number, total_mol);
        zcm = Eigen::MatrixXd::Zero(total_frame_number, total_mol);

        int mol_index = 0;
        for (auto &mol: frame->molecule_list) {
            if (!mol->bExculde) {
                auto coord = mol->calc_weigh_center(frame);
                xcm(0, mol_index) = get<0>(coord);
                ycm(0, mol_index) = get<1>(coord);
                zcm(0, mol_index) = get<2>(coord);
                mol_index++;
            }
        }
    } else {
        int mol_index = 0;
        for (auto &mol: frame->molecule_list) {
            if (!mol->bExculde) {
                auto coord = mol->calc_weigh_center(frame);
                double xold = xcm(steps - 1, mol_index);
                double yold = ycm(steps - 1, mol_index);
                double zold = zcm(steps - 1, mol_index);
                double xr = get<0>(coord) - xold;
                double yr = get<1>(coord) - yold;
                double zr = get<2>(coord) - zold;
                frame->image(xr, yr, zr);
                xcm(steps, mol_index) = xr + xold;
                ycm(steps, mol_index) = yr + yold;
                zcm(steps, mol_index) = zr + zold;
                mol_index++;
            }
        }
    }
    steps++;

}

void Diffuse::print() {

    vector<int> ntime(total_frame_number, 0);
    vector<double> xmsd(total_frame_number, 0.0);
    vector<double> ymsd(total_frame_number, 0.0);
    vector<double> zmsd(total_frame_number, 0.0);

    if (bSerial) {
        if (bTradition) {
            for (int i = 0; i < total_frame_number - 1; i++) {
                for (int j = i + 1; j < total_frame_number; j++) {
                    int m = j - i - 1;
                    ntime[m]++;
                    for (int k = 0; k < total_mol; k++) {
                        double xdiff = xcm(j, k) - xcm(i, k);
                        double ydiff = ycm(j, k) - ycm(i, k);
                        double zdiff = zcm(j, k) - zcm(i, k);
                        xmsd[m] += xdiff * xdiff;
                        ymsd[m] += ydiff * ydiff;
                        zmsd[m] += zdiff * zdiff;

                    }
                }
            }
        } else {
            for (int m = 0; m < total_frame_number - 1; m++) {
                for (int i = 0; i < total_frame_number - 1; i += m + 1) {
                    int j = i + m + 1;
                    if (j < total_frame_number) {
                        ntime[m]++;
                        for (int k = 0; k < total_mol; k++) {
                            double xdiff = xcm(j, k) - xcm(i, k);
                            double ydiff = ycm(j, k) - ycm(i, k);
                            double zdiff = zcm(j, k) - zcm(i, k);
                            xmsd[m] += xdiff * xdiff;
                            ymsd[m] += ydiff * ydiff;
                            zmsd[m] += zdiff * zdiff;
                        }
                    }
                }
            }
        }

    } else {

        class Body {
        public:
            int total_frame_number;
            vector<int> ntime;
            vector<double> xmsd;
            vector<double> ymsd;
            vector<double> zmsd;

            int total_mol;
            Eigen::MatrixXd &xcm;
            Eigen::MatrixXd &ycm;
            Eigen::MatrixXd &zcm;

            Body(int total_frame_number, int total_mol, Eigen::MatrixXd &xcm, Eigen::MatrixXd &ycm,
                 Eigen::MatrixXd &zcm) :
                    total_frame_number(total_frame_number),
                    ntime(total_frame_number - 1), xmsd(total_frame_number - 1),
                    ymsd(total_frame_number - 1), zmsd(total_frame_number - 1),
                    total_mol(total_mol), xcm(xcm), ycm(ycm), zcm(zcm) {}

            Body(const Body &body, tbb::split) :
                    total_frame_number(body.total_frame_number),
                    ntime(body.total_frame_number - 1), xmsd(body.total_frame_number - 1),
                    ymsd(body.total_frame_number - 1), zmsd(body.total_frame_number - 1),
                    total_mol(body.total_mol), xcm(body.xcm), ycm(body.ycm), zcm(body.zcm) {}

            void join(const Body &body) {
                for (int i = 0; i < total_frame_number - 1; i++) {
                    ntime[i] += body.ntime[i];
                    xmsd[i] += body.xmsd[i];
                    ymsd[i] += body.ymsd[i];
                    zmsd[i] += body.zmsd[i];
                }

            }

            void operator()(const tbb::blocked_range<int> &range) {
                for (int i = range.begin(); i != range.end(); i++) {
                    for (int j = i + 1; j < total_frame_number; j++) {
                        int m = j - i - 1;
                        ntime[m]++;
                        for (int k = 0; k < total_mol; k++) {
                            double xdiff = xcm(j, k) - xcm(i, k);
                            double ydiff = ycm(j, k) - ycm(i, k);
                            double zdiff = zcm(j, k) - zcm(i, k);
                            xmsd[m] += xdiff * xdiff;
                            ymsd[m] += ydiff * ydiff;
                            zmsd[m] += zdiff * zdiff;

                        }
                    }
                }
            }
        } body(total_frame_number, total_mol, xcm, ycm, zcm);

        tbb::parallel_reduce(tbb::blocked_range<int>(0, total_frame_number - 1), body, tbb::auto_partitioner());


        ntime = body.ntime;
        xmsd = body.xmsd;
        ymsd = body.ymsd;
        zmsd = body.zmsd;
    }

    const double dunits = 10.0;


    for (int i = 0; i < total_frame_number - 1; i++) {
        double counts = total_mol * ntime[i];
        xmsd[i] /= counts;
        ymsd[i] /= counts;
        zmsd[i] /= counts;
    }

    outfile << "*********************************************************" << endl;
    outfile << "Mean Squared Displacements and Self-Diffusion Constant" << endl;
    outfile << "    Time Gap      X MSD       Y MSD       Z MSD       R MSD        Diff Const" << endl;
    outfile << "      (ps)       (Ang^2)     (Ang^2)     (Ang^2)     (Ang^2)     (x 10^-5 cm**2/sec)" << endl;

    for (int i = 0; i < total_frame_number - 1; i++) {
        double delta = time_increment_ps * (i + 1);
        double xvalue = xmsd[i];
        double yvalue = ymsd[i];
        double zvalue = zmsd[i];
        double rvalue = xmsd[i] + ymsd[i] + zmsd[i];
        double dvalue = dunits * rvalue / delta / 6.0;
        outfile << fmt::sprintf("%12.2f%12.2f%12.2f%12.2f%12.2f%12.4f",
                                delta, xvalue, yvalue, zvalue, rvalue, dvalue) << endl;

    }
    outfile << "*********************************************************" << endl;

}

void Diffuse::readInfo() {
    while (true) {
        time_increment_ps = 0.1;
        string input_line = input(" Enter the Time Increment in Picoseconds [0.1]: ");
        trim(input_line);
        if (!input_line.empty()) {
            time_increment_ps = stod(input_line);
            if (time_increment_ps <= 0.0) {
                cout << "error time increment " << time_increment_ps << endl;
                continue;
            }
        }
        break;
    }
    while (true) {
        string input_line = input(" Enter the Total Frame Number: ");
        trim(input_line);
        if (!input_line.empty()) {
            total_frame_number = stoi(input_line);
            if (total_frame_number <= 0) {
                cerr << "error total frame number " << total_frame_number << endl;
                continue;
            }
            break;
        }
    }
    while (true) {
        string input_line = input(" Numbers of any Atoms to be Removed :  ");
        trim(input_line);
        exclude_atom.clear();
        if (!input_line.empty()) {
            auto field = split(input_line);
            for (auto it = field.cbegin(); it != field.cend(); ++it) {
                auto i = std::stoi(*it);
                if (i > 0) {
                    exclude_atom.insert(i);
                } else if (i < 0) {
                    ++it;
                    auto j = std::stoi(*it);
                    for (int k = -i; k <= j; k++) exclude_atom.insert(k);
                } else {
                    cerr << "error of atom num select" << endl;
                    continue;
                }
            }
            break;
        }
    }

    while (true) {
        string input_line = input(" serial ? [T]:");
        trim(input_line);
        if (!input_line.empty()) {
            if (input_line.compare("T") == 0) {
                bSerial = true;
                while (true) {
                    string input_line = input(" trandition ? [T]:");
                    trim(input_line);
                    if (!input_line.empty()) {
                        if (input_line.compare("T") == 0) {
                            bTradition = true;
                            break;
                        } else if (input_line.compare("F") == 0) {
                            bTradition = false;
                            break;
                        }
                    }
                }
                break;
            } else if (input_line.compare("F") == 0) {
                bSerial = false;
                enable_tbb = true;
                break;
            }
        }
    }


}


class DipoleAngle : public BasicAnalysis {
public:
    DipoleAngle() {
        enable_forcefield = true;
        enable_outfile = true;
    }

    virtual ~DipoleAngle() = default;

    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    void readInfo() override;

protected:

    std::vector<Atom::AtomIndenter> ids1;
    std::vector<Atom::AtomIndenter> ids2;

    double distance_width;
    double angle_width;

    int distance_bins;
    int angle_bins;

    map<pair<int, int>, size_t> hist;
};

void DipoleAngle::process(std::shared_ptr<Frame> &frame) {
    shared_ptr<Atom> ref;

    for (auto &mol : frame->molecule_list) {
        mol->bExculde = true;
        for (auto &atom : mol->atom_list) {
            if (Atom::is_match(atom, ids1)) {
                ref = atom;
            } else if (Atom::is_match(atom, ids2)) {
                mol->ow = atom;
                mol->bExculde = false;
            }
        }
        mol->calc_mass();
    }
    if (!ref) {
        std::cerr << "reference atom not found" << std::endl;
        exit(5);
    }
    double ref_x = ref->x;
    double ref_y = ref->y;
    double ref_z = ref->z;
    for (auto &mol: frame->molecule_list) {
        if (!mol->bExculde) {

            double x1 = mol->ow->x;
            double y1 = mol->ow->y;
            double z1 = mol->ow->z;

            double xr = x1 - ref_x;
            double yr = y1 - ref_y;
            double zr = z1 - ref_z;

            frame->image(xr, yr, zr);

            double distance = sqrt(xr * xr + yr * yr + zr * zr);

            auto dipole = mol->calc_dipole(frame);

            double dipole_scalar = sqrt(pow(get<0>(dipole), 2) + pow(get<1>(dipole), 2) + pow(get<2>(dipole), 2));

            double _cos =
                    (xr * get<0>(dipole) + yr * get<1>(dipole) + zr * get<2>(dipole)) / (distance * dipole_scalar);

            double angle = acos(_cos) * 180.0 / 3.1415926;

            int i_distance_bin = int(distance / distance_width) + 1;
            int i_angle_bin = int(angle / angle_width) + 1;

            if (i_distance_bin <= distance_bins and i_angle_bin <= angle_bins) {
                hist[make_pair(i_distance_bin, i_angle_bin)] += 1;
            }
        }
    }
}

void DipoleAngle::print() {

//    outfile << boost::format("%5s") % "Dist";
//    for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
//        outfile << boost::format("%7.1f") % (i_angle  * angle_width);
//    }
//    outfile << endl;
//    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
//        outfile << boost::format("%5.3f") % (i_distance * distance_width);
//        size_t total = 0;
//        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
//            total += hist[make_pair(i_distance,i_angle)];
//        }
//        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
//            outfile << boost::format("%7.4f") % ( total == 0 ? 0.0 : double(hist[make_pair(i_distance,i_angle)]) / total);
//        }
//        outfile << endl;
//    }
    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        size_t total = 0;
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            total += hist[make_pair(i_distance, i_angle)];
        }
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            outfile << boost::format("%5.3f%10.3f%10.4f\n")
                       % ((i_distance - 0.5) * distance_width)
                       % ((i_angle - 0.5) * angle_width)
                       % (total == 0 ? 0.0 : double(hist[make_pair(i_distance, i_angle)]) / total);
        }
    }
}

void DipoleAngle::readInfo() {
    Atom::select2group(ids1, ids2);
    double rmax = choose(0.0, GMX_DOUBLE_MAX, "Enter Maximum Distance to Accumulate[10.0 Ang]:", true, 10.0);
    distance_width = choose(0.0, GMX_DOUBLE_MAX, "Enter Width of Distance Bins [0.01 Ang]:", true, 0.01);
    double angle_max = choose(0.0, 180.0, "Enter Maximum Angle to Accumulate[180.0 degree]:", true, 180.0);
    angle_width = choose(0.0, 180.0, "Enter Width of Angle Bins [0.5 degree]:", true, 0.5);

    distance_bins = int(rmax / distance_width);
    angle_bins = int(angle_max / angle_width);

    for (int i_distance = 1; i_distance <= distance_bins; i_distance++) {
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            hist[make_pair(i_distance, i_angle)] = 0;
        }
    }
}

class DipoleAngleSingleDistanceNormal : public DipoleAngle {
public:
    void print() override;

};

void DipoleAngleSingleDistanceNormal::print() {
    outfile << "DipoleAngleSingleDistanceNormal : " << std::endl;
     double factor = (4.0 / 3.0) * M_PI;
    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        size_t total = 0;
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            total += hist[make_pair(i_distance, i_angle)];
        }
        double dv = factor * (pow(i_distance * distance_width, 3) - pow((i_distance - 1) * distance_width, 3));
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            outfile << boost::format("%5.3f   %10.2f   %g\n")
                       % ((i_distance - 0.5) * distance_width)
                       % ((i_angle - 0.5) * angle_width)
                       % (total == 0 ? 0.0 : double(hist[make_pair(i_distance, i_angle)]) / (total * dv * angle_width));
        }
    }
}

class DipoleAngleVolumeNormal : public DipoleAngle {
public:
    void print() override;
};

void DipoleAngleVolumeNormal::print() {
    outfile << "DipoleAngleVolumeNormal : " << std::endl;
    size_t total = 0;
     for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            total += hist[make_pair(i_distance, i_angle)];
        }

    }
     double factor = (4.0 / 3.0) * M_PI ;
    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        double dv = factor * (pow(i_distance * distance_width, 3) - pow((i_distance - 1) * distance_width, 3));
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            outfile << boost::format("%5.3f   %10.3f   %g\n")
                       % ((i_distance - 0.5 )* distance_width)
                       % ((i_angle - 0.5)* angle_width)
                       % (total == 0 ? 0.0 : double(hist[make_pair(i_distance, i_angle)]) / ( total * dv * angle_width));
        }
    }
}


class DipoleAngle2Gibbs : public DipoleAngle {
public:
    void print() override;

    void readInfo() override;

protected:

    const double kb = 1.380649e-23; // unit: J/K
    double temperature;  // unit: K
    const double avogadro_constant = 6.022140857e23;

};

void DipoleAngle2Gibbs::print() {

    double factor = -kb * temperature * avogadro_constant / 4184.0;
    double max_value = 0.0;

    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        double dv = pow(i_distance * distance_width, 3) - pow((i_distance - 1) * distance_width, 3);
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            max_value = max(max_value,hist[make_pair(i_distance, i_angle)] / (dv));
        }
    }
    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        double dv = pow(i_distance * distance_width, 3) - pow((i_distance - 1) * distance_width, 3);
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            double pop = double(hist[make_pair(i_distance, i_angle)]) / (max_value * dv);
            outfile << boost::format("%5.3f   %10.3f   %15.6f\n")
                       % ((i_distance - 0.5) * distance_width)
                       % ((i_angle - 0.5) * angle_width)
                       % (pop == 0.0 ? 100.0 : factor * log(pop));
        //               %  pop;
        }
    }

    /*for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            max_value = max(max_value,hist[make_pair(i_distance, i_angle)] / (angle_width));
        }
    }
    for (int i_distance = 1; i_distance < distance_bins; i_distance++) {
        double dv = pow(i_distance * distance_width, 3) - pow((i_distance - 1) * distance_width, 3);
        for (int i_angle = 1; i_angle <= angle_bins; i_angle++) {
            double pop = double(hist[make_pair(i_distance, i_angle)]) / (max_value * angle_width);
            outfile << boost::format("%5.3f%7.1f%10.6f\n")
                       % (i_distance * distance_width)
                       % (i_angle * angle_width)
                     //  % (pop == 0.0 ? 10000.0 : factor * log(pop));
                       %  pop;
        }
    }
    */
}

void DipoleAngle2Gibbs::readInfo() {
    DipoleAngle::readInfo();
    temperature = choose(0.0, 10000.0, "Temperature [298] (K):", true, 298.0);
}



class ShellDensity : public BasicAnalysis {
public:
    void process(std::shared_ptr<Frame> &frame) override;

    void print() override;

    ShellDensity();

    void readInfo() override;

protected:
    std::vector<Atom::AtomIndenter> ids1;
    std::vector<Atom::AtomIndenter> ids2;

    double distance_width;

    int distance_bins;

    map<int, size_t> hist;

    size_t nframe = 0;

};

void ShellDensity::process(std::shared_ptr<Frame> &frame) {
    nframe++;
    for( auto & ref : frame->atom_list){
        if (Atom::is_match(ref,ids1)) {
            for (auto &atom : frame->atom_list){
                if (Atom::is_match(atom,ids2)) {
                    int ibin = int(atom_distance(ref,atom,frame) / distance_width) + 1;
                    if ( ibin <= distance_bins){
                        hist[ibin]++;
                    }
                }
            }
        }
    }
}

void ShellDensity::print() {
    outfile << "************************************************" << endl;
    outfile << "***** Shell Density Function ****" << endl;

    outfile << "First Type : " << ids1 << " Second Type : " << ids2 << endl;

    outfile << "************************************************" << endl;
    outfile << "Bin    Distance    Densitry (count / Ang3 / frame)" << endl;

    for (int i = 1; i <= distance_bins; i++) {
        double dv = (4.0 / 3.0) * M_PI * (pow(i*distance_width,3) -  pow((i-1)*distance_width,3));
        outfile << fmt::sprintf("%d      %.4f      %g \n",
                                i, (i - 0.5) * distance_width, hist[i] / (nframe * dv));
    }

    outfile << "************************************************" << endl;
}


void ShellDensity::readInfo() {
    Atom::select2group(ids1, ids2);
    double rmax = choose(0.0, GMX_DOUBLE_MAX, "Enter Maximum Distance to Accumulate[10.0 Ang]:", true, 10.0);
    distance_width = choose(0.0, GMX_DOUBLE_MAX, "Enter Width of Distance Bins [0.01 Ang]:", true, 0.01);
    distance_bins = int(rmax / distance_width);
    for (int i = 1; i <= distance_bins; i++) {
        hist[i] = 0;
    }
}

ShellDensity::ShellDensity() {
    enable_outfile = true;
}

void processOneFrame(shared_ptr<Frame> &frame,
                     shared_ptr<list<shared_ptr<BasicAnalysis>>> &task_list) {
    for (auto &task : *task_list) {
        task->process(frame);
    }
}




auto getTasks() {
    auto task_list = make_shared<list<shared_ptr<BasicAnalysis>>>();
    auto menu1 = []() {
        std::cout << "Please select the desired operation" << std::endl;
        std::cout << "(0) Gromacs XTC & TRR & GRO & NetCDF Output" << std::endl;
        std::cout << "(1) Distance" << std::endl;
        std::cout << "(2) Coordinate Number per Frame" << std::endl;
        std::cout << "(3) Radical Distribution Function" << std::endl;
        std::cout << "(4) ResidenceTime" << std::endl;
        std::cout << "(5) Green-Kubo " << std::endl;
        std::cout << "(6) Hydrogen Bond " << std::endl;
        std::cout << "(7) RMSD Calculator " << std::endl;
        std::cout << "(8) RMSF Calculator " << std::endl;
        std::cout << "(9) Cluster Analysis(linkage) " << std::endl;
        std::cout << "(10) NMRRange Analysis " << std::endl;
        std::cout << "(11) Diffusion Coefficient by Einstein equation" << std::endl;
        std::cout << "(12) Diffusion Cutoff Coefficient by Einstein equation" << std::endl;
        std::cout << "(13) Water exchange analysis" << std::endl;
        std::cout << "(14) Rotational autocorrelation function" << std::endl;
        std::cout << "(15) Dipole Angle" << std::endl;
        std::cout << "(16) Dipole Angle to Gibbs Free Energy" << std::endl;
        std::cout << "(17) Dipole Angle of single distance normal" << std::endl;
        std::cout << "(18) Dipole Angle of volume normal" << std::endl;
        std::cout << "(19) Shell Density function" << std::endl;
        std::cout << "(20) Start" << std::endl;
        return choose(0, 20, "select :");
    };
    while (true) {
        int num = menu1();
        shared_ptr<BasicAnalysis> task;
        switch (num) {
            case 0:
                task = std::make_shared<GmxTrj>();
                break;
            case 1:
                task = std::make_shared<Distance>();
                break;
            case 2:
                task = std::make_shared<CoordinateNumPerFrame>();
                break;
            case 3:
                task = std::make_shared<RadicalDistribtuionFunction>();
                break;
            case 4:
                task = std::make_shared<ResidenceTime>();
                break;
            case 5:
                task = std::make_shared<GreenKubo>();
                break;
            case 6:
                task = std::make_shared<HBond>();
                break;
            case 7:
                task = std::make_shared<RMSDCal>();
                break;
            case 8:
                task = std::make_shared<RMSFCal>();
                break;
            case 9:
                task = std::make_shared<Cluster>();
                break;
            case 10:
                task = std::make_shared<NMRRange>();
                break;
            case 11:
                task = std::make_shared<Diffuse>();
                break;
            case 12:
                task = std::make_shared<DiffuseCutoff>();
                break;
            case 13:
                task = std::make_shared<FirstCoordExchangeSearch>();
                break;
            case 14:
                task = std::make_shared<RotAcfCutoff>();
                break;
            case 15:
                task = std::make_shared<DipoleAngle>();
                break;
            case 16:
                task = std::make_shared<DipoleAngle2Gibbs>();
                break;
            case 17:
                task = std::make_shared<DipoleAngleSingleDistanceNormal>();
                break;
            case 18:
                task = std::make_shared<DipoleAngleVolumeNormal>();
                break;
            case 19:
                task = std::make_shared<ShellDensity>();
                break;
            default:
                return task_list;

        }
        task->readInfo();
        task_list->push_back(task);

    }
}

void init() {

    struct item {
        AminoAcidType type;
        string str;
    };

    struct item items[] = {
            {AminoAcidType::H3N_Ala, "H3N_Ala"},
            {AminoAcidType::H3N_Gly, "H3N_Gly"},
            {AminoAcidType::H3N_Pro, "H3N_Pro"},
            {AminoAcidType::H3N_Trp, "H3N_Trp"},
            {AminoAcidType::H3N_Asp, "H3N_Asp"},
            {AminoAcidType::H3N_Glu, "H3N_Glu"},
            {AminoAcidType::H3N_Arg, "H3N_Arg"},

            {AminoAcidType::Ala,     "Ala"},
            {AminoAcidType::Gly,     "Gly"},
            {AminoAcidType::Pro,     "Pro"},
            {AminoAcidType::Trp,     "Trp"},
            {AminoAcidType::Asp,     "Asp"},
            {AminoAcidType::Glu,     "Glu"},
            {AminoAcidType::Arg,     "Arg"}
    };

    for (item &it : items) {
        str_to_aminotype[it.str] = it.type;
        aminotype_to_str[it.type] = it.str;
    }


}

int main(int argc, char *argv[]) {

    std::cout << "Build DateTime : " << __DATE__ << " " << __TIME__ << endl;
    {
        char buffer[1024];
        getcwd(buffer, 1024);
        std::cout << "current work dir : " << buffer << std::endl;
    }

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "show this help message")
            ("topology,p", po::value<string>(), "topology file")
            ("file,f", po::value<string>(), "trajectory file")
            ("output,o", po::value<string>(), "output file")
            ("prm", po::value<string>(), "force field file");
    po::positional_options_description p;
    p.add("file", 1).add("output", 1);
    po::variables_map vm;

    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

    po::notify(vm);
    if (vm.count("help")) {
        cout << desc;
        return 0;
    }
    if (!vm.count("file")) {
        cerr << "input trajectory file is not set !" << endl;
        cerr << desc;
        exit(1);
    }

    init();
    std::string xyzfile = vm["file"].as<string>();

    {
        fstream in(xyzfile, ofstream::in);
        if (!in.good()) {
            cerr << "The file " << xyzfile << " is bad !" << endl;
            exit(2);
        }
    }
    auto task_list = getTasks();
    while (enable_forcefield) {
        if (vm.count("prm")) {
            auto ff = vm["prm"].as<string>();
            if (file_exist(ff)) {
                forcefield.read(ff);
                break;
            }
            cout << "force field file " << ff << " is bad ! please retype !" << endl;

        }
        forcefield.read(choose_file("force field filename:", true));
        break;
    }
    int start = choose(1, INT32_MAX, "Enter the start frame[1]:", true, 1);
    int step_size = choose(1, INT32_MAX, "Enter the step size[1]:", true, 1);
    int total_frames = choose(0, INT32_MAX, "How many frames to read [all]:", true, 0);

    tbb::task_scheduler_init tbb_init(tbb::task_scheduler_init::deferred);
    if (enable_tbb) {
        int threads = choose(0, sysconf(_SC_NPROCESSORS_ONLN), "How many cores to used in parallel[automatic]:",
                             true,
                             0);
        tbb_init.initialize(threads == 0 ? tbb::task_scheduler_init::automatic : threads);
    }
    int current_frame_num = 0;

    auto reader = make_shared<TrajectoryReader>();
    reader->add_filename(xyzfile);
    string ext = ext_filename(xyzfile);
    while (ext == "nc" or ext == "trr" or ext == "xtc") {
        if (vm.count("topology")) {
            string topol = vm["topology"].as<string>();
            if (file_exist(topol)) {
                reader->add_topology(topol);
                break;
            }
            cout << "topology file " << topol << " is bad ! please retype !" << endl;
        }
        reader->add_topology(choose_file("input topology file(xyz file):", true));
        break;
    }
    string input_line;
    if (ext == "traj") {
        cout << "traj file can not use multiple files" << endl;
    } else {
        input_line = input("Do you want to use multiple files [No]:");
        trim(input_line);
        if (!input_line.empty()) {
            if (input_line[0] == 'Y' or input_line[0] == 'y') {
                while (true) {
                    input_line = choose_file("next file [Enter for End]:", true, "", true);
                    if (input_line.empty()) break;
                    if (ext_filename(input_line) == "traj") {
                        cout << "traj file can not use multiple files [retype]" << endl;
                        continue;
                    }
                    reader->add_filename(input_line);
                }
            }
        }
    }

    if (enable_outfile) {
        outfile.open(vm.count("output") ? vm["output"].as<string>() : choose_file("Output file: ", false),
                     std::fstream::out);
    }
    shared_ptr<Frame> frame;
    int Clear = 0;
    while ((frame = reader->readOneFrame())) {
        current_frame_num++;
        if (total_frames != 0 and current_frame_num > total_frames)
            break;
        if (current_frame_num % 10 == 0) {
            if (Clear) {
                std::cout << "\r";
            }
            cout << "Processing Coordinate Frame  " << current_frame_num << "   " << std::flush;
            Clear = 1;
        }
        if (current_frame_num >= start && (current_frame_num - start) % step_size == 0)
            processOneFrame(frame, task_list);
    }
    std::cout << std::endl;


    for (auto &task : *task_list)
        task->print();
    if (outfile.is_open()) outfile.close();
    std::cout << "Mission Complete" << std::endl;

    return 0;

}



