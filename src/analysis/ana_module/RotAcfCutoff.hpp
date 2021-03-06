//
// Created by xiamr on 6/14/19.
//

#ifndef TINKER_ROTACFCUTOFF_HPP
#define TINKER_ROTACFCUTOFF_HPP

#include <boost/container_hash/hash.hpp>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_set>

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "dsl/AmberMask.hpp"
#include "utils/VectorSelector.hpp"

class Frame;

class Molecule;

class RotAcfCutoff : public AbstractAnalysis {
public:
    RotAcfCutoff();

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] std::string description();

    void setParameters(const AmberMask &M, const AmberMask &L, std::shared_ptr<VectorSelector> vector,
                       int LegendrePolynomial, double cutoff, double time_increment_ps, double max_time_gap_ps,
                       const std::string &outfilename);

    [[nodiscard]] static std::string_view title() {
        return "Rotational Autocorrelation Function within Solvation Shell";
    }

    struct InnerAtom {
        int index;
        std::shared_ptr<std::list<std::tuple<double, double, double>>> list_ptr;

        InnerAtom(int index, std::shared_ptr<std::list<std::tuple<double, double, double>>> list_ptr)
            : index(index), list_ptr(std::move(list_ptr)) {}
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
    
private:
    double time_increment_ps = 0.1;
    double cutoff2;

    AmberMask ids1;
    AmberMask ids2;

    std::unordered_set<std::shared_ptr<Atom>> group1;
    std::unordered_set<std::shared_ptr<Atom>> group2;

    std::unordered_set<InnerAtom, InnerAtomHasher> inner_atoms;

    std::list<std::shared_ptr<std::list<std::tuple<double, double, double>>>> rots;

    [[nodiscard]] auto find_in(int seq);

    [[nodiscard]] std::tuple<double, double, double> calVector(std::shared_ptr<Molecule> &mol,
                                                               std::shared_ptr<Frame> &frame);

    std::shared_ptr<VectorSelector> vectorSelector;

    template <typename Function>
    void calculateAutocorrelaionFunction(std::vector<std::pair<unsigned long long, double>> &acf, Function f) const;

    int LegendrePolynomial;

    double max_time_gap;
};

#endif  // TINKER_ROTACFCUTOFF_HPP
