#include "Vid11.h"
#include "GlobalVariant.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("company");
    a.setApplicationName("VidApp");

    Vid11 w;
    w.show();
    return a.exec();
}