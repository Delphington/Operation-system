#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>

enum ListFlags
{
    LS_ALL = 1,
    LS_LONG = 2
};

enum FileColor
{
    COL_FILE,
    COL_DIR,
    COL_EXEC,
    COL_LN
};

enum ErrorCode
{
    ERR_NEARGS,
    ERR_INVALIDOPT,
    ERR_FILE_ISNT_SPEC,
    ERR_OPENDIR,
    ERR_READDIR,
    ERR_STAT,
    ERR_PWD,
    ERR_GRP
};

static const int FILE_TYPE_COLORS[] = {
    39,  // По умолчанию (обычные файлы)
    34,  // Синий (директории)
    32,  // Зеленый (исполняемые файлы)
    36   // Бирюзовый (символические ссылки)
};

// Допустимые опции командной строки
static const char * const VALID_OPTIONS = "hla";

// Символы прав доступа для отображения rwx
static const char * const PERMISSION_CHARS = "rwx";

// Константы формата времени
static const int TIME_STRING_OFFSET = 4;
static const int TIME_STRING_LENGTH = 12;

// ANSI escape последовательности
static const char * const ANSI_RESET = "\x1b[0m";
static const char * const ANSI_COLOR_PREFIX = "\x1b[;";

const char * g_basePath = NULL;

// Максимальный размер буфера пути для файловых операций
static const size_t MAX_PATH_LENGTH =
#ifdef PATH_MAX
    (size_t)PATH_MAX
#else
    (size_t)4096
#endif
;
char * g_pathBuffer = NULL;

DIR *g_openDir = NULL;
size_t g_entryCount = 0;
struct dirent ** g_entries = NULL;
int g_flags = 0;

void initProgram();
void failWithError(enum ErrorCode);
void composePath(const char * fileName);
void collectEntries();
void printEntry(struct dirent* entry);
void listDirectory(const char *dirPath);
void closeDirOnExit();
void freePathOnExit();
void freeEntryListOnExit();
int direntCompare(const void* a, const void* b);


int main(int argc, char **argv){
    initProgram();
    opterr = 0; // подавить сообщения об ошибках getopt
    int option;
    while ((option = getopt(argc, argv, VALID_OPTIONS)) != -1){
        switch (option) {
            case 'h':
                printf("ls - list directory contents\n"
                       "usage: ls [params...] [file]\n"
                       " -a - do not ignore entries starting with '.'\n"
                       " -l - use a long listing format\n"
                       " -h - print this message\n");
                exit(EXIT_SUCCESS);
            case 'l':
                g_flags |= LS_LONG;
                break;
            case 'a':
                g_flags |= LS_ALL;
                break;
            default:
                failWithError(ERR_INVALIDOPT);
        }
    }
    // Определение целевой директории
    const char *targetDir = (optind == argc) ? "." : argv[optind];
    listDirectory(targetDir);
    exit(EXIT_SUCCESS);
}

// Инициализация ресурсов программы
void initProgram(){
    g_pathBuffer = (char *)malloc(MAX_PATH_LENGTH);
    atexit(freePathOnExit);
}

// Обработка ошибок и выход
void failWithError(enum ErrorCode err){
    switch (err){
    case ERR_NEARGS:
        fprintf(stderr, "[ls]: Not enough args! Usage: ls -h\n");
        break;

    case ERR_INVALIDOPT:
        fprintf(stderr, "[ls]: Error: invalid option, see \"ls -h\"\n");
        break;

    case ERR_FILE_ISNT_SPEC:
        fprintf(stderr, "[ls]: Error! File isn't specified\n");
        break;

    case ERR_OPENDIR:
        fprintf(stderr, "[ls]: Error while opening directory! %s\n",
                strerror(errno));
        break;

    case ERR_READDIR:
        fprintf(stderr, "[ls]: Error while parsing files! %s\n",
                strerror(errno));
        break;

    case ERR_STAT:
        fprintf(stderr, "[ls]: error while getting stat! %s\n",
                strerror(errno));
        break;

    case ERR_PWD:
        fprintf(stderr, "[ls]: error while getting username! %s\n",
                strerror(errno));
        break;

    case ERR_GRP:
        fprintf(stderr, "[ls]: error while getting groupname! %s\n",
                strerror(errno));
        break;
    }
    exit(EXIT_FAILURE);
}