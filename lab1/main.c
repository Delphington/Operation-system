#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <regex.h>
#include <stdlib.h>


// Константы для флагов
enum {
    NO_FLAGS = 0,  // без флагов, поведение как у обычного cat
    N_FLAG = 1,    // флаг -n: нумеровать все строки
    B_FLAG = 2,    // флаг -b: нумеровать только непустые строки
    E_FLAG = 4     // флаг -E: показывать символ $ в конце строки
};



int main(int argc, char *argv[]) {
    if (strstr(argv[0], "mycat")) {
        return mycat_main(argc, argv);
    } else if (strstr(argv[0], "mygrep")) {
        return mygrep_main(argc, argv);
    } else {
        fprintf(stderr, "Executable must be named 'mycat' or 'mygrep'\n");
        return 1;
    }
}
