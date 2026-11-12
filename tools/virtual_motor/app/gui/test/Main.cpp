#include "gmock/gmock.h"
#include <QApplication>

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application{ argc, argv };
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
