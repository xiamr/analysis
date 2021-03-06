#include <utility>

//
// Created by xiamr on 3/17/19.
//

#ifndef TINKER_FORCEFIELD_HPP
#define TINKER_FORCEFIELD_HPP

#include <boost/optional.hpp>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "config.h"
#include "data_structure/atom.hpp"

class Frame;

class AtomItem {
public:
    AtomItem(int typ, int classNum, std::string name, std::string res, int at_no, double mass)
        : typ(typ), class_num(classNum), name(std::move(name)), res(std::move(res)), at_no(at_no), mass(mass) {}

    int typ;
    int class_num;
    std::string name;
    std::string res;
    int at_no;
    double mass;

    boost::optional<double> charge;
};

class Forcefield {
public:
    void read(const std::string &filename);

    void read_tinker_prm(const std::string &filename);

    double find_mass(const std::shared_ptr<Atom> &atom);

    void assign_forcefield(std::shared_ptr<Frame> &frame);

    bool isValid() { return isvalid; }

private:
    std::unordered_map<int, AtomItem> mapping;

    bool isvalid = false;
};

#endif  // TINKER_FORCEFIELD_HPP
