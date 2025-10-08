#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int handle_octal_mode(const char *mode_str, const char *filename) {
    char *endptr;
    long mode = strtol(mode_str, &endptr, 8);

    if (*endptr != '\0' || mode < 0 || mode > 07777) {
        fprintf(stderr, "mychmod: invalid mode '%s'\n", mode_str);
        return 1;
    }

    if (chmod(filename, (mode_t)mode) == -1) {
        error_exit(filename);
    }
    return 0;
}

int handle_symbolic_mode(const char *mode_str, const char *filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        error_exit(filename);
    }

    mode_t current_mode = st.st_mode;
    mode_t new_mode = current_mode;

    const char *p = mode_str;

    int who = 0;
    while (*p && strchr("ugoa", *p)) {
        switch (*p) {
            case 'u': who |= 1 << 2; break;
            case 'g': who |= 1 << 1; break;
            case 'o': who |= 1 << 0; break;
            case 'a': who |= 0b111; break;
        }
        p++;
    }

    if (who == 0) {
        who = 0b111;
    }

    if (*p != '+' && *p != '-' && *p != '=') {
        fprintf(stderr, "mychmod: invalid operator in '%s'\n", mode_str);
        return 1;
    }

    char op = *p++;
    mode_t bits = 0;
    while (*p) {
        switch (*p) {
            case 'r': bits |= S_IRUSR | S_IRGRP | S_IROTH; break;
            case 'w': bits |= S_IWUSR | S_IWGRP | S_IWOTH; break;
            case 'x': bits |= S_IXUSR | S_IXGRP | S_IXOTH; break;
            default:
                fprintf(stderr, "mychmod: invalid permission '%c' in '%s'\n", *p, mode_str);
                return 1;
        }
        p++;
    }

    mode_t mask = 0;
    if (who & (1 << 2)) mask |= S_IRWXU;
    if (who & (1 << 1)) mask |= S_IRWXG;
    if (who & (1 << 0)) mask |= S_IRWXO;

    mode_t apply_bits = 0;
    if (who & (1 << 2)) apply_bits |= (bits & S_IRWXU);
    if (who & (1 << 1)) apply_bits |= (bits & S_IRWXG);
    if (who & (1 << 0)) apply_bits |= (bits & S_IRWXO);

    switch (op) {
        case '+':
            new_mode |= apply_bits;
            break;
        case '-':
            new_mode &= ~apply_bits;
            break;
        case '=':
            new_mode &= ~mask;
            new_mode |= apply_bits;
            break;
    }

    if (chmod(filename, new_mode) == -1) {
        error_exit(filename);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mode> <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *mode_str = argv[1];
    const char *filename = argv[2];

    int is_octal = 1;
    for (int i = 0; mode_str[i]; i++) {
        if (mode_str[i] < '0' || mode_str[i] > '7') {
            is_octal = 0;
            break;
        }
    }

    if (is_octal) {
        return handle_octal_mode(mode_str, filename);
    } else {
        return handle_symbolic_mode(mode_str, filename);
    }
}