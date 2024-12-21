#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../inc/logger.h"
#include "../inc/myerrors.h"


int open_logs_files(void) {
    if (mkdir(LOG_DIR, 0777) && errno != EEXIST) {
        printf("Error creating directory: %s\n", strerror(errno));
        return LOG_DIR_OPEN_ERR;
    }

    // Открываем файл
    FILE *file = fopen(MAIN_LOG_FILE, "w");
    if (file == NULL) {
        printf("Error opening file: %s\n", strerror(errno));
        return MAIN_LOG_FILE_OPEN_ERR;
    }
    fprintf(file, "Logs are initialized.\n");
    fclose(file);

    file = fopen(STEPS_LOG_FILE, "w");
    if (file == NULL) {
        printf("Error opening file: %s\n", strerror(errno));
        return STEPS_LOG_FILE_OPEN_ERR;
    }
    fprintf(file, "Logs are initialized.\n");
    fclose(file);

    printf("Files and directory created successfully.\n");

    return OK;
}


void log_message(LogLevel level, const char* format, ...) {    
    if (level == LOG_DEBUG && !DEBUG_LOGS_ALLOWED)
        return;
    else if (level == LOG_INFO && !INFO_LOGS_ALLOWED)
        return;
    else if (level == LOG_WARNING && !WARNING_LOGS_ALLOWED)
        return;
    else if (level == LOG_ERROR && !ERROR_LOGS_ALLOWED)
        return;
    else if (level == LOG_STEPS && !STEPS_LOGS_ALLOWED)
        return;

    FILE *log_file;
    switch (level) {
        case LOG_STEPS:
            log_file = fopen(STEPS_LOG_FILE, "a");
            if (log_file == NULL) {
                perror("Error opening steps log file in log_message()\n");
                return;
            }
            break;

        default:
            log_file = fopen(MAIN_LOG_FILE, "a");
            if (log_file == NULL) {
                perror("Error opening main log file in log_message()\n");
                return;
            }
            break;
    }

    
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char time_str[50];
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", timeinfo);

    va_list args;
    va_start(args, format);

    const char *log_levels_in_file[] = {
        "[DEBUG]",
        "[INFO]",
        "[WARNING]",
        "[ERROR]",
        "[STEPS]"
    };

    fprintf(log_file, "%s %s [pid %d] ", time_str, log_levels_in_file[level], getpid());
    vfprintf(log_file, format, args);
    fclose(log_file);

    va_end(args);
}