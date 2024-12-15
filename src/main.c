#include "./inc/server.h"
#include "./inc/myerrors.h"


int main(void) {
    int exit_code = server();
    printf("exit status: %d, %s\n", exit_code, error_to_string(exit_code));

    return 0;
}