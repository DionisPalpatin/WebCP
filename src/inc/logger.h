#ifndef __LOGGER_H__
#define __LOGGER_H__


#include <stdio.h>
#include <stdarg.h>
#include <time.h>


typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_STEPS
} LogLevel;


#define LOG_LEVEL   LOG_STEPS
#define LOG_DIR     "./logs"
#define MAIN_LOG_FILE    "./logs/log_main.txt"
#define STEPS_LOG_FILE    "./logs/log_steps.txt"


int open_logs_files(void);
void log_message(LogLevel level, const char* format, ...);


#endif