//
// Created by xiamr on 6/1/19.
//

#ifndef TINKER_CENTER_SELECTION_GRAMMAR_HPP
#define TINKER_CENTER_SELECTION_GRAMMAR_HPP

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/phoenix.hpp>
#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/fusion/include/at_c.hpp>

#include <boost/spirit/repository/include/qi_kwd.hpp>
#include <boost/spirit/repository/include/qi_distinct.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "common.hpp"
#include "grammar.hpp"

namespace qi = boost::spirit::qi;
namespace fusion = boost::fusion;
namespace phoenix = boost::phoenix;

struct MassCenterRuleNode {
    Atom::Node SelectionMask;

    MassCenterRuleNode(const Atom::Node &selectionMask) : SelectionMask(selectionMask) {}
};

struct GeomCenterRuleNode {
    Atom::Node SelectionMask;

    GeomCenterRuleNode(const Atom::Node &selectionMask) : SelectionMask(selectionMask) {}
};

struct NoopRuleNode {
    Atom::Node SelectionMask;

    NoopRuleNode(const Atom::Node &selectionMask) : SelectionMask(selectionMask) {}
};

using CenterRuleNode = boost::variant<std::shared_ptr<MassCenterRuleNode>,
        std::shared_ptr<GeomCenterRuleNode>,
        std::shared_ptr<NoopRuleNode>>;


template<typename Iterator, typename Skipper>
struct CenterGrammar : qi::grammar<Iterator, CenterRuleNode(), Skipper> {
    CenterGrammar();

    qi::rule<Iterator, CenterRuleNode(), Skipper> expr;
    qi::rule<Iterator, Atom::Node(), Skipper> mask;
    Grammar<Iterator, Skipper> maskParser;
};


template<typename Iterator, typename Skipper>
CenterGrammar<Iterator, Skipper>::CenterGrammar() : CenterGrammar::base_type(expr) {
    using qi::uint_;
    using qi::eps;
    using qi::as_string;
    using qi::lexeme;
    using qi::alpha;
    using qi::alnum;
    using qi::ascii::char_;
    using qi::_val;
    using qi::_1;
    using qi::_2;
    using qi::_3;
    using qi::_4;
    using phoenix::new_;
    using phoenix::val;
    using phoenix::bind;
    using phoenix::ref;
    using qi::on_error;
    using qi::fail;
    using qi::lit;
    using boost::spirit::repository::qi::distinct;


    mask = (("{" >> maskParser >> "}") | maskParser)[_val = _1];

    expr = (distinct(char_("a-zA-Z_0-9"))["com"] >> distinct(char_("a-zA-Z_0-9"))["of"]
                                                 >> mask[_val = make_shared_<MassCenterRuleNode>(_1)])
           | (distinct(char_("a-zA-Z_0-9"))["geom"] >> distinct(char_("a-zA-Z_0-9"))["of"]
                                                    >> mask[_val = make_shared_<GeomCenterRuleNode>(_1)])
           | mask[_val = make_shared_<NoopRuleNode>(_1)];

    on_error<fail>(
            expr,
            std::cout
                    << val("Error! Expecting")
                    << _4
                    << val(" here: \"")
                    << phoenix::construct<std::string>(_3, _2)
                    << val("\"")
                    << std::endl
    );

}


template<typename Iterator, typename Skipper>
CenterRuleNode input_atom_selection(const CenterGrammar<Iterator, Skipper> &grammar, const std::string &promot) {

    for (;;) {
        CenterRuleNode mask;
        std::string input_string = input(promot);
        boost::trim(input_string);
        auto it = input_string.begin();

        bool status = qi::phrase_parse(it, input_string.end(), grammar, qi::ascii::space, mask);

        if (!(status and (it == input_string.end()))) {
            std::cout << "error-pos : " << std::endl;
            std::cout << input_string << std::endl;
            for (auto iter = input_string.begin(); iter != it; ++iter) std::cout << " ";
            std::cout << "^" << std::endl;

            continue;
        }
        return mask;
    }
}


inline void selectCentergroup(CenterRuleNode &ids, const std::string &prompt) {
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;
    using ascii::char_;

    CenterGrammar<std::string::iterator, qi::ascii::space_type> grammar;

    ids = input_atom_selection(grammar, prompt);
}


#endif //TINKER_CENTER_SELECTION_GRAMMAR_HPP
