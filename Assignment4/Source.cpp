#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MaxBuffer 1024
#define MaxHistory 20
#define MaxArg 20
#define KGRN  "\x1B[32m"
#define KWHT  "\x1B[37m"

void PrintPrompt() {
    char path[MaxBuffer];
    if (getcwd(path, sizeof(path)) == NULL) {
        printf("Failure\n");
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    printf("%s%s> %s", KGRN, path, KWHT);
}

int GetArgCount(char* str) {
    int count = 0;
    bool prevSpace = true;
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] != ' ') {
            if (prevSpace) count++;
            prevSpace = false;
        }
        else {
            prevSpace = true;
        }
    }
    return count;
}

void TokenizeInput(char* input, char** args, int* argCount) {
    char* token = strtok(input, " \n");
    while (token != NULL) {
        args[*argCount] = token;
        (*argCount)++;
        token = strtok(NULL, " \n");
    }
}

void HandleRedirection(char** args, int* argCount) {
    pid_t pid = fork();
    if (pid == -1) {
        printf("Fork failed\n");
        return;
    }

    if (pid == 0) {
        // Child process
        int in = -1, out = -1;

        for (int i = 0; i < *argCount; i++) {
            if (strcmp(args[i], "<") == 0) {
                in = i + 1;

            }
            else if (strcmp(args[i], ">") == 0) {
                out = i + 1;

            }
        }
        if (in != -1) {
            int fd = open(args[in], O_RDONLY);
            if (fd == -1) {
                printf("Failure\n");
                perror(args[in]);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[in - 1] = NULL;
        }
        if (out != -1) {
            int fd = creat(args[out], 0644);
            if (fd == -1) {
                printf("Failure\n");
                perror(args[out]);
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[out - 1] = NULL;
        }

        if (execvp(args[0], args) == -1) {
            printf("Failure\n");
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    else {
        waitpid(pid, NULL, 0);
    }
}

void PipeCommand(char** args, int argCount, char** args2, int argCount2) {
    int pipefd[2];
    pid_t pid1, pid2;

    //printf("Piping: \n");
    //printf("arguement1(%d):", argCount);
    //for (int i = 0; i <= argCount; i++) {
    //    printf("%s ", args[i]);
    //}
    //printf("\narguement2(%d):", argCount2);
   // for (int i = 0; i <= argCount2; i++) {
    //    printf("%s ", args2[i]);
    //}
    //printf("\n");

    if (pipe(pipefd) == -1) {
        printf("Failure pipe 92\n");
        return;
    }

    pid1 = fork();

    if (pid1 == -1) {
        printf("Failure pipe 99\n");
        return;
    }

    if (pid1 == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        if (execvp(args[0], args) == -1) {
            printf("Failure pipe 108\n");
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    else {
        pid2 = fork();
        if (pid2 == -1) {
            printf("Failure pipe 116\n");
            return;
        }
        else if (pid2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(args2[0], args2) == -1) {
                printf("Failure pipe 124\n");
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        else {
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
    }
}

void ExecuteCommand(char** args, int argCount) {
    if (argCount == 0) return;

    //printf("Executing: ");
    //for (int i = 0; i <= argCount; i++) {
    //    printf("%s ", args[i]);
    //}
    //printf("\n");

    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(EXIT_SUCCESS);
    }

    if (strcmp(args[0], "history") == 0) {
        char command[MaxBuffer];
        FILE* fp = fopen(".shell_history", "r");
        if (fp != NULL) {
            while (fgets(command, sizeof(command), fp) != NULL) {
                printf("%s", command);
            }
            fclose(fp);
        }
        return;
    }

    if (strcmp(args[0], "pwd") == 0) {
        PrintPrompt();
        printf("\n");
        return;
    }


    char* args2[MaxArg];
    int argCount2 = 0;
    int pipeFlag = 0;
    int j = 0;


    for (int i = 0; i < argCount; i++) {
        if (strcmp(args[i], "|") == 0) {
            j = i + 1;

            while (j < argCount) {
                args2[argCount2] = args[j];
                argCount2++;
                j++;
            }

            args[i] = NULL;

            args2[argCount2] = NULL;

            pipeFlag = 1;

            break;
        }
    }

    if (pipeFlag == 1) {
        PipeCommand(args, argCount, args2, argCount2);
        return;
    }

    HandleRedirection(args, &argCount);
}

void WriteCommandToHistory(char* command) {
    FILE* fp = fopen(".shell_history", "a");
    if (fp != NULL) {
        fprintf(fp, "%s", command);
        fclose(fp);
    }
}

int main() {
    char input[MaxBuffer];
    char* args[MaxArg];
    int argCount = 0;

    printf("Welcome to Ryan Granquist's Custom Linux Shell\n");
    printf("  1: enter 'pwd' to print working directory\n");
    printf("  2: basic commands and arguements work\n");
    printf("  3: enter 'history' to view command history\n");
    printf("  4: < and > work for redirecting input and output. One at a time please\n");
    printf("  5: | operations work. Again, one at a time please, and using with redirects does not always work\n");
    printf("If their is any problem when you run this, message me on team or canvas with questions because it all works as the assignment requires\n");

    PrintPrompt();
    while (fgets(input, MaxBuffer, stdin) != NULL) {
        WriteCommandToHistory(input);

        int len = strlen(input);
        if (len == 0 || (len == 1 && input[0] == '\n')) {
            PrintPrompt();
            continue;
        }
        input[len - 1] = '\0';

        TokenizeInput(input, args, &argCount);

        ExecuteCommand(args, argCount);
        memset(input, 0, MaxBuffer);
        memset(args, 0, sizeof(args));
        argCount = 0;
        PrintPrompt();
    }

    printf("Closing Shell\n");
    return 0;
}