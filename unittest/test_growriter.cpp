//
// Created by xiamr on 6/28/19.
//

#include <gmock/gmock.h>

#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "data_structure/molecule.hpp"
#include "utils/FileInterface.hpp"
#include "utils/common.hpp"
#include "utils/gro_writer.hpp"

using namespace std;
using namespace testing;

class FileMock : public FileInterface, public std::ostringstream {
   public:
    MOCK_METHOD2(open, void(const std::string &, std::ios_base::openmode));

    MOCK_METHOD0(close, void());
};

TEST(GROWriter, OpenCloseFileAndWriteFrame) {
    auto mock = std::make_shared<FileMock>();
    GROWriter writer(mock);

    auto frame = make_shared<Frame>();
    frame->title = "Unit Test Case";
    frame->box = PBCBox(10.0, 10.0, 10.0, 90.0, 90.0, 90.0);
    frame->enable_bound = true;

    auto atom1 = make_shared<Atom>();

    atom1->x = -9.90;
    atom1->y = 8.89;
    atom1->z = 2.1;
    atom1->atom_name = "N";
    atom1->seq = 1;

    auto atom2 = make_shared<Atom>();
    atom2->x = 0.90;
    atom2->y = 3.19;
    atom2->z = 4.1;
    atom2->atom_name = "O2";
    atom2->seq = 2;

    frame->atom_list = {atom1, atom2};
    frame->atom_map = {{atom1->seq, atom1}, {atom2->seq, atom2}};

    auto mol = make_shared<Molecule>();
    mol->atom_list = {atom1, atom2};
    frame->molecule_list = {mol};
    atom1->molecule = mol;
    atom2->molecule = mol;
    mol->sequence = 1;

    EXPECT_CALL(*mock, open(StrEq("output.gro"), std::ios::out)).Times(1);
    EXPECT_CALL(*mock, close()).Times(1);

    writer.open("output.gro");
    writer.write(frame);

    ostringstream except;
    except << "Unit Test Case\n";
    except << "2\n";
    except << format("%5d%-5s%5s%5d%8.3f%8.3f%8.3f\n", 1, "1", "N", 1, -0.990, 0.889, 0.21);
    except << format("%5d%-5s%5s%5d%8.3f%8.3f%8.3f\n", 1, "1", "O2", 2, 0.090, 0.319, 0.41);
    except << "   1.00000   1.00000   1.00000\n";
    ASSERT_THAT(mock->str(), StrEq(except.str()));
    writer.close();
}
