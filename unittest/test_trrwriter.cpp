//
// Created by xiamr on 6/28/19.
//
#include <cmath>
#include <gmock/gmock.h>

#include "utils/GromacsInterface.hpp"
#include "utils/ThrowAssert.hpp"
#include "data_structure/atom.hpp"
#include "data_structure/frame.hpp"
#include "gtest_utility.hpp"
#include "utils/trr_writer.hpp"
#include "GromacsInterfaceMock.hpp"

using namespace testing;
using namespace std;


class TRRWriterTest : public TRRWriter {
protected:
    GromacsInterface *getGromacsImpl() override { return gromacsMock; }

public:
    GromacsMock *gromacsMock;

    TRRWriterTest(GromacsMock *gromacsMock) : gromacsMock(gromacsMock) {}
};

TEST(TRRWriter, OpenAndCloseFile) {

    GromacsMock mock;
    TRRWriterTest writer(&mock);

    std::string filename = "output.trr";
    auto fio = reinterpret_cast<gmx::t_fileio *>(1000);
    EXPECT_CALL(mock, open_trn(StrEq(filename), StrEq("w"))).WillOnce(Return(fio));
    EXPECT_CALL(mock, close_trn(fio)).Times(1);
    writer.open(filename);
    writer.close();
}

TEST(TRRWriter, WriteFrameToFile) {

    GromacsMock mock;
    TRRWriterTest writer(&mock);

    std::string filename = "output.trr";
    auto fio = reinterpret_cast<gmx::t_fileio *>(1000);
    auto frame = make_shared<Frame>();
    frame->box = PBCBox(10.0, 10.0, 10.0, 90.0, 90.0, 90.0);

    auto atom1 = make_shared<Atom>();

    atom1->x = -9.90;
    atom1->y = 8.89;
    atom1->z = 2.1;

    atom1->vx = -1.90;
    atom1->vy = 9.89;
    atom1->vz = 2.7;

    atom1->seq = 1;

    auto atom2 = make_shared<Atom>();

    atom2->x = 0.90;
    atom2->y = 3.19;
    atom2->z = 4.1;

    atom2->vx = 9.90;
    atom2->vy = 2.19;
    atom2->vz = 4.2;

    atom1->seq = 2;

    frame->atom_list = {atom1, atom2};
    frame->atom_map = {{atom1->seq, atom1},
                       {atom2->seq, atom2}};

    gmx::rvec box[3] = {{1.0, 0.0, 0.0},
                        {0.0, 1.0, 0.0},
                        {0.0, 0.0, 1.0}};
    gmx::rvec x[2] = {{-0.99, 0.889, 0.21},
                      {0.09,  0.319, 0.41}};

    gmx::rvec v[2] = {{-0.19, 0.989, 0.27},
                      {0.99,  0.219, 0.42}};

    EXPECT_CALL(mock, open_trn(StrEq(filename), StrEq("w"))).WillOnce(Return(fio));
    EXPECT_CALL(mock, fwrite_trn(fio, 0, 0.0, 0.0, FLOAT_ARRAY_EQ(box, 9), 2, FLOAT_ARRAY_EQ(x, 6), NULL, NULL)).Times(
            1);
    EXPECT_CALL(mock, fwrite_trn(fio, 1, 1.0, 0.0, FLOAT_ARRAY_EQ(box, 9), 2, FLOAT_ARRAY_EQ(x, 6), NULL, NULL)).Times(
            1);
    EXPECT_CALL(mock,
                fwrite_trn(fio, 2, FloatEq(100.0), 0.0, FLOAT_ARRAY_EQ(box, 9), 2, FLOAT_ARRAY_EQ(x, 6),
                           FLOAT_ARRAY_EQ(v, 6),
                           NULL)).Times(1);

    EXPECT_CALL(mock, close_trn(fio)).Times(1);

    writer.open(filename);
    writer.write(frame);
    writer.write(frame);
    writer.setCurrentTime(100);
    writer.setWriteVelocities(true);
    writer.write(frame);
    writer.close();
}

TEST(TRRWriter, CloseBeforeOpen) {
    GromacsMock mock;
    TRRWriterTest writer(&mock);
    ASSERT_THROW(writer.close(), AssertionFailureException);
}

TEST(TRRWriter, WriteBeforeOpen) {
    GromacsMock mock;
    TRRWriterTest writer(&mock);
    auto frame = make_shared<Frame>();
    ASSERT_THROW(writer.write(frame), AssertionFailureException);
}

TEST(TRRWriter, WriteAfterClose) {
    GromacsMock mock;
    TRRWriterTest writer(&mock);
    auto frame = make_shared<Frame>();
    EXPECT_CALL(mock, open_trn(StrEq("Output.trr"), StrEq("w"))).WillOnce(
            Return(reinterpret_cast<gmx::t_fileio *>(10100)));
    EXPECT_CALL(mock, close_trn(reinterpret_cast<gmx::t_fileio *>(10100))).Times(1);
    ASSERT_NO_THROW(writer.open("Output.trr"));
    ASSERT_NO_THROW(writer.close());
    ASSERT_THROW(writer.write(frame), AssertionFailureException);
}

TEST(TRRWriter, OpenError) {
    GromacsMock mock;
    TRRWriterTest writer(&mock);
    EXPECT_CALL(mock, open_trn(StrEq("Output.trr"), StrEq("w"))).WillOnce(Return(nullptr));
    ASSERT_THROW(writer.open("Output.trr"), AssertionFailureException);
}

TEST(TRRWriter, OpenTwice) {
    GromacsMock mock;
    TRRWriterTest writer(&mock);
    EXPECT_CALL(mock, open_trn(StrEq("Output.trr"), StrEq("w"))).WillOnce(
            Return(reinterpret_cast<gmx::t_fileio *>(10100)));
    ASSERT_NO_THROW(writer.open("Output.trr"));
    ASSERT_THROW(writer.open("Output.trr"), AssertionFailureException);
}

TEST(TRRWriter, CloseTwice) {
    GromacsMock mock;
    TRRWriterTest writer(&mock);
    EXPECT_CALL(mock, open_trn(StrEq("Output.trr"), StrEq("w"))).WillOnce(
            Return(reinterpret_cast<gmx::t_fileio *>(10100)));
    EXPECT_CALL(mock, close_trn(reinterpret_cast<gmx::t_fileio *>(10100))).Times(1);
    ASSERT_NO_THROW(writer.open("Output.trr"));
    ASSERT_NO_THROW(writer.close());
    ASSERT_THROW(writer.close(), AssertionFailureException);
}

TEST(TRRWriter, NULL_vs_nullptr) {
    auto fio = reinterpret_cast<gmx::t_fileio *>(1000);
    ASSERT_TRUE(fio != nullptr);
    fio = nullptr;
    ASSERT_TRUE(fio == nullptr);
}



