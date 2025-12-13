#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FAILURE 1
#define SUCCESS 0
#define ALL_USERS 0b111
#define USER_BIT 1 << 2
#define GROUP_BIT 1 << 1
#define OTHER_BIT 1 << 0

/* Константы для символов операций */
#define OPERATOR_ADD '+'
#define OPERATOR_SUBTRACT '-'
#define OPERATOR_SET '='

/* Константы для символов пользователей */
#define USER_CHAR 'u'
#define GROUP_CHAR 'g'
#define OTHER_CHAR 'o'
#define ALL_CHAR 'a'

/* Константы для символов прав доступа */
#define READ_PERM 'r'
#define WRITE_PERM 'w'
#define EXECUTE_PERM 'x'

void display_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int validate_octal_string(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '7') {
            return 0;
        }
    }
    return 1;
}

int apply_numeric_mode(const char *mode_input, const char *file_path) {
    char *terminator;
    long mode_value = strtol(mode_input, &terminator, 8);

    if (*terminator != '\0' || mode_value < 0 || mode_value > 07777) {
        fprintf(stderr, "mychmod: invalid mode '%s'\n", mode_input);
        return FAILURE;
    }

    if (chmod(file_path, (mode_t)mode_value) == -1) {
        display_error(file_path);
    }
    return SUCCESS;
}

int determine_user_mask(const char **input_ptr) {
    int user_mask = 0;
    const char *ptr = *input_ptr;
    const char user_chars[] = {USER_CHAR, GROUP_CHAR, OTHER_CHAR, ALL_CHAR, '\0'};

    while (*ptr && strchr(user_chars, *ptr)) {
        switch (*ptr) {
            case USER_CHAR: user_mask |= USER_BIT; break;
            case GROUP_CHAR: user_mask |= GROUP_BIT; break;
            case OTHER_CHAR: user_mask |= OTHER_BIT; break;
            case ALL_CHAR: user_mask |= ALL_USERS; break;
        }
        ptr++;
    }

    *input_ptr = ptr;
    return user_mask ? user_mask : ALL_USERS;
}

mode_t extract_permission_bits(const char **input_ptr, const char *mode_input) {
    mode_t permission_mask = 0;
    const char *ptr = *input_ptr;
    const char perm_chars[] = {READ_PERM, WRITE_PERM, EXECUTE_PERM, '\0'};

    while (*ptr) {
        if (!strchr(perm_chars, *ptr)) {
            fprintf(stderr, "mychmod: invalid permission '%c' in '%s'\n", *ptr, mode_input);
            return 0;
        }

        switch (*ptr) {
            case READ_PERM: permission_mask |= S_IRUSR | S_IRGRP | S_IROTH; break;
            case WRITE_PERM: permission_mask |= S_IWUSR | S_IWGRP | S_IWOTH; break;
            case EXECUTE_PERM: permission_mask |= S_IXUSR | S_IXGRP | S_IXOTH; break;
        }
        ptr++;
    }

    *input_ptr = ptr;
    return permission_mask;
}

mode_t compute_affected_bits(int user_mask, mode_t permission_mask) {
    mode_t result = 0;

    if (user_mask & USER_BIT) result |= (permission_mask & S_IRWXU);
    if (user_mask & GROUP_BIT) result |= (permission_mask & S_IRWXG);
    if (user_mask & OTHER_BIT) result |= (permission_mask & S_IRWXO);

    return result;
}

mode_t compute_mask_bits(int user_mask) {
    mode_t mask = 0;

    if (user_mask & USER_BIT) mask |= S_IRWXU;
    if (user_mask & GROUP_BIT) mask |= S_IRWXG;
    if (user_mask & OTHER_BIT) mask |= S_IRWXO;

    return mask;
}

mode_t calculate_new_mode(mode_t current, char operation, mode_t affected, mode_t mask) {
    switch (operation) {
        case OPERATOR_ADD: return current | affected;
        case OPERATOR_SUBTRACT: return current & ~affected;
        case OPERATOR_SET: return (current & ~mask) | affected;
        default: return current;
    }
}

int apply_symbolic_mode(const char *mode_input, const char *file_path) {
    struct stat file_info;
    if (stat(file_path, &file_info) == -1) {
        display_error(file_path);
    }

    mode_t current_permissions = file_info.st_mode;
    const char *parser = mode_input;

    int user_mask = determine_user_mask(&parser);

    /* Проверка оператора с использованием констант */
    if (*parser != OPERATOR_ADD && *parser != OPERATOR_SUBTRACT && *parser != OPERATOR_SET) {
        fprintf(stderr, "mychmod: invalid operator in '%s'\n", mode_input);
        return FAILURE;
    }

    char operation = *parser++;
    mode_t permission_bits = extract_permission_bits(&parser, mode_input);

    if (permission_bits == 0 && *mode_input) {
        return FAILURE;
    }

    mode_t affected_bits = compute_affected_bits(user_mask, permission_bits);
    mode_t mask_bits = compute_mask_bits(user_mask);
    mode_t updated_permissions = calculate_new_mode(current_permissions, operation, affected_bits, mask_bits);

    if (chmod(file_path, updated_permissions) == -1) {
        display_error(file_path);
    }

    return SUCCESS;
}

int process_mode_change(const char *mode_input, const char *file_path) {
    if (validate_octal_string(mode_input)) {
        return apply_numeric_mode(mode_input, file_path);
    } else {
        return apply_symbolic_mode(mode_input, file_path);
    }
}

int main(int arg_count, char *arg_values[]) {
    if (arg_count != 3) {
        fprintf(stderr, "Usages: %s <mode> <file>\n", arg_values[0]);
        exit(EXIT_FAILURE);
    }

    const char *mode_specification = arg_values[1];
    const char *target_file = arg_values[2];

    return process_mode_change(mode_specification, target_file);
}