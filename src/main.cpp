#include "cli.hpp"

int main(int argc, char** argv)
{
    CmdLine cmd_line(argc, argv);

    cmd_line.setup();
    cmd_line.run();

    $PRINT_UNIMPLEMENTED_FEATURES();
    return EXIT_SUCCESS;
}
