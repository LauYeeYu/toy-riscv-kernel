#include "system.h"
#include "ulib.h"

#define MAX_LINE_LENGTH 4096

enum builtin {
    EXIT,
    POWEROFF,
    NON_BUILTIN
};

const char *parameter_delimiters = " \t\r\n";

char tmp_env[4096];
char path[4096];
char *program_envp[4096];
char argv_buffer[4096];
char *tmp_argv[4096];

enum builtin program_type(char *program) {
    if (strcmp(program, "exit") == 0) {
        return EXIT;
    } else if (strcmp(program, "poweroff") == 0) {
        return POWEROFF;
    }
    return NON_BUILTIN;
}

int execute_line(char *line) {
    char **argv = tmp_argv;
    int argc = 0;
    argv[0] = NULL;
    path[0] = '\0';
    char **old_envp = program_envp; // for restoring envp
    while ((*old_envp) !=  NULL) {
        old_envp++;
    }

    // parse
    for (char *parameter = strtok(line, parameter_delimiters);
         parameter != NULL;
         parameter = strtok(NULL, parameter_delimiters)) {
        if (path[0] == '\0') { // program name
            switch (program_type(parameter)) {
                case EXIT:
                    exit(0);
                case POWEROFF:
                    return power_off();
                case NON_BUILTIN:
                    break;
            }
            path[0] = '/';
            strcpy(path + 1, parameter, strlen(parameter));
        }
        argv[argc] = parameter;
        ++argc;
        argv[argc] = NULL;
    }

    // execute
    if (path[0] != '\0') {
        int pid = fork();
        if (pid == 0) {
            exec(path, argv, program_envp);
            printf("execve failed!\n");
            exit(-1);
        } else {
            int exit_code;
            wait(&exit_code);
            return exit_code;
        }
    }
    return 0;
}

int main() {
    int i = 0;
    /*while (envp[i] != NULL) {
        program_envp[i] = envp[i];
        ++i;
    }*/
    program_envp[i] = NULL;
    char line[MAX_LINE_LENGTH];
    print_string("Welcome to sh!\n# ");
    while (true) {
        if (read_until(line, MAX_LINE_LENGTH, '\r') != 0) {
            printf("Too many characters in a line!\n#");
            continue;
        }
        put_char('\n');

        int exit_code = execute_line(line);
        if (exit_code != 0) {
            printf("%d # ", exit_code);
        } else {
            printf("# ");
        }
    }
    return 0;
}
