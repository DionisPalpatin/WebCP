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
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    LogLevel current_log_level = LOG_LEVEL;

    if (level > current_log_level) { return; }

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

    char time_str[50];
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", timeinfo);

    va_list args;
    va_start(args, format);

    fprintf(log_file, "%s ", time_str);

    switch (level) {
        case LOG_DEBUG:
            fprintf(log_file, "[DEBUG] [pid %d] ", getpid());
            break;
        case LOG_INFO:
            fprintf(log_file, "[INFO] [pid %d] ", getpid());
            break;
        case LOG_WARNING:
            fprintf(log_file, "[WARNING] [pid %d] ", getpid());
            break;
        case LOG_ERROR:
            fprintf(log_file, "[ERROR] [pid %d] ", getpid());
            break;
        case LOG_STEPS:
            fprintf(log_file, "[STEP] [pid %d] ", getpid());
            break;
    }

    vfprintf(log_file, format, args);

    va_end(args);
    fclose(log_file);
}