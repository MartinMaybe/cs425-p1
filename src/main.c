#include "lab.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef TEST
#define main main_exclude
#endif



int main(int argc, char **argv)
{
    struct smtp_config cfg;
    int rc = parse_args(argc, argv, &cfg);

    // no args check
    if (rc == -1) {
        return 0;
    } 

    // wrong args check
    if (rc != 0) {
        return 1;
    }

    return 0;
}