

#ifndef TINKER_FIRSTCOORDEXCHANGESEARCH_HPP
#define TINKER_FIRSTCOORDEXCHANGESEARCH_HPP

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>

#include "AbstractAnalysis.hpp"
#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "dsl/AmberMask.hpp"

class Frame;

class FirstCoordExchangeSearch : public AbstractAnalysis {
public:
    FirstCoordExchangeSearch();

    void processFirstFrame(std::shared_ptr<Frame> &frame) override;

    void process(std::shared_ptr<Frame> &frame) override;

    void print(std::ostream &os) override;

    void readInfo() override;

    [[nodiscard]] static std::string_view title() { return "Water exchange analysis"; }

private:
    AmberMask ids1;
    AmberMask ids2;

    std::unordered_set<std::shared_ptr<Atom>> group1;
    std::unordered_set<std::shared_ptr<Atom>> group2;

    double dist_cutoff, tol_dist, time_cutoff;
    enum class Direction { IN, OUT };
    typedef struct {
        Direction direction;
        int seq;
        int exchange_frame;
    } ExchangeItem;

    std::list<ExchangeItem> exchange_list;

    int step = 0;

    typedef struct {
        bool inner;
    } State;
    std::map<int, State> state_machine;

    std::set<int> init_seq_in_shell;
};

#endif  // TINKER_FIRSTCOORDEXCHANGESEARCH_HPP
