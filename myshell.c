#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>


#define MAX_INPUT_SIZE 1024
#define MAX_PATH_SIZE 1024
#define MAX_ALIAS_SIZE 256

// Data structure to store aliases
typedef struct {
    char name[MAX_ALIAS_SIZE];
    char value[MAX_INPUT_SIZE];
} Alias;

Alias aliases[MAX_ALIAS_SIZE];

int alias_count = 0;
char *output_file;
// Function declarations
void print_prompt();
void execute_command(char *command);
void handle_alias(char *command);
void display_user_info();
void save_aliases();
void load_aliases();
void reverse_string(char *str);
void reverse_last_line(char *filename);
int load_reverse_flag();
void save_reverse_flag(int flag);
void save_filename(char *filename);
char* retrieve_filename();

int main() {
    load_aliases();  // Load saved aliases

    char input[MAX_INPUT_SIZE];

    while (1) {
        print_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            exit(EXIT_FAILURE);
        }

        // Remove newline character from input
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        if (strcmp(input, "bello") == 0) {
            display_user_info();
            continue;
        }

        if (strncmp(input, "alias", 5) == 0) {
            handle_alias(input);
            save_aliases();  // Save aliases after modification
            continue;
        }

        execute_command(input);
    }

    return 0;
}

void print_prompt() {
    //similar to whoami code in ps
    char * username = getenv("USER");
    char host[MAX_PATH_SIZE];
    gethostname(host, sizeof(host));
    char *cwd = getcwd(NULL, 0);

    printf("%s@%s %s --- ", username, host, cwd);

    free(cwd);
}

void execute_command(char *command) {

    //background check

    int background = 0;

    // Check for background execution (&) at the end of the command
    if (strlen(command) > 0 && command[strlen(command) - 1] == '&') {
        background = 1;
        command[strlen(command) - 1] = '\0'; // Remove the '&' symbol
    }


    pid_t pid = fork();

    if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process

        // Check for output redirection
        char *output_redirect = strstr(command, ">>>");
        if (output_redirect != NULL) {
            //printf("found needle\n");
            save_reverse_flag(1);
            //printf("found needle %d\n",reverse_flag);
            *output_redirect = '\0'; // Null terminate before ">>"
            output_file = output_redirect + 3; // Points to the file name
            save_filename(output_file);
            // Remove leading and trailing spaces from the file name
            while (*output_file == ' ' || *output_file == '\t') {
                output_file++;
            }
            char *end = output_file + strlen(output_file) - 1;
            while (*end == ' ' || *end == '\t') {
                *end = '\0';
                end--;
            }

            // Output redirection with append (>>)
            int fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
                perror("Error opening output file");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);

        }

        output_redirect = strstr(command, ">>");
        if (output_redirect != NULL) {
            *output_redirect = '\0'; // Null terminate before ">>"
            output_file = output_redirect + 2; // Points to the file name
            // Remove leading and trailing spaces from the file name
            while (*output_file == ' ' || *output_file == '\t') {
                output_file++;
            }
            char *end = output_file + strlen(output_file) - 1;
            while (*end == ' ' || *end == '\t') {
                *end = '\0';
                end--;
            }

            // Output redirection with append (>>)
            int fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
                perror("Error opening output file");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else {
            // Check for regular output redirection (>), if not using append (>>)
            output_redirect = strstr(command, ">");
            if (output_redirect != NULL) {
                *output_redirect = '\0'; // Null terminate before ">"
                output_file = output_redirect + 1; // Points to the file name
                // Remove leading and trailing spaces from the file name
                while (*output_file == ' ' || *output_file == '\t') {
                    output_file++;
                }
                char *end = output_file + strlen(output_file) - 1;
                while (*end == ' ' || *end == '\t') {
                    *end = '\0';
                    end--;
                }

                // Output redirection without append (>)
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }

        // Check if the command is an alias
        for (int i = 0; i < alias_count; ++i) {
            if (strcmp(aliases[i].name, command) == 0) {
                // Tokenize the alias value and execute
                char *token = strtok(aliases[i].value, " \t");
                char *args[MAX_INPUT_SIZE];
                int arg_count = 0;

                while (token != NULL) {
                    args[arg_count++] = token;
                    token = strtok(NULL, " \t");
                }

                args[arg_count] = NULL;

                // Execute the alias with its arguments
                execvp(args[0], args);

                // If execvp fails
                perror("Command execution failed");
                exit(EXIT_FAILURE);
            }
        }

        // Not an alias

        // Tokenize the command and execute
        char *token = strtok(command, " \t");
        char *args[MAX_INPUT_SIZE];
        int arg_count = 0;

        while (token != NULL) {
            args[arg_count++] = token;
            token = strtok(NULL, " \t");
        }

        args[arg_count] = NULL;

        // Execute the command with its arguments


        if (background) {
            // Detach from the terminal to allow the parent to continue
            setsid();
        }
        execvp(args[0], args);

        // If execvp fails
        perror("Command execution failed");
        exit(EXIT_FAILURE);

    } else {
        // Parent process
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
            int reverse_flag = load_reverse_flag();
            if (reverse_flag) {
                printf("reversing\n");
                output_file = retrieve_filename();
                reverse_last_line(output_file);
                save_reverse_flag(0);
            }
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code != 0) {
                    fprintf(stderr, "Command execution failed with exit code %d\n", exit_code);
                }
            } else if (WIFSIGNALED(status)) {
                int signal_number = WTERMSIG(status);
                fprintf(stderr, "Command terminated by signal %d\n", signal_number);
            }
        } else {
            // Background process
            printf("Background process started with PID %d\n", pid);
        }
    }
}

void handle_alias(char *command) {
    char *alias_name = strtok(command + 6, "=");
    char *alias_value = strtok(NULL, "\0");

    if (alias_name != NULL && alias_value != NULL) {
        // Remove leading and trailing spaces from alias_name and alias_value
        while (*alias_name == ' ' || *alias_name == '\t') {
            alias_name++;
        }

        char *end = alias_name + strlen(alias_name) - 1;
        while (*end == ' ' || *end == '\t') {
            *end = '\0';
            end--;
        }

        while (*alias_value == ' ' || *alias_value == '\t') {
            alias_value++;
        }

        end = alias_value + strlen(alias_value) - 1;
        while (*end == ' ' || *end == '\t') {
            *end = '\0';
            end--;
        }

        // Handle quotes around the alias value
        if (alias_value[0] == '"' && alias_value[strlen(alias_value) - 1] == '"') {
            // Remove quotes
            memmove(alias_value, alias_value + 1, strlen(alias_value) - 2);
            alias_value[strlen(alias_value) - 2] = '\0';
        }

        // Check if the alias already exists
        for (int i = 0; i < alias_count; ++i) {
            if (strcmp(aliases[i].name, alias_name) == 0) {
                // Update existing alias
                strcpy(aliases[i].value, alias_value);
                printf("Alias '%s' updated.\n", alias_name);
                return;
            }
        }

        // Check if there is space for a new alias
        if (alias_count < MAX_ALIAS_SIZE) {
            // Create a new alias
            strcpy(aliases[alias_count].name, alias_name);
            strcpy(aliases[alias_count].value, alias_value);
            alias_count++;
            //printf("Alias '%s' created.\n", alias_name);
        } else {
            fprintf(stderr, "Maximum number of aliases reached.\n");
        }
    } else {
        fprintf(stderr, "Invalid alias syntax.\n");
    }
}


void save_aliases() {
    FILE *file = fopen("aliases.txt", "w");
    if (file == NULL) {
        perror("Error opening aliases file for writing");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < alias_count; ++i) {
        fprintf(file, "alias %s = \"%s\"\n", aliases[i].name, aliases[i].value);
    }

    fclose(file);
}

void load_aliases() {
    FILE *file = fopen("aliases.txt", "r");
    if (file == NULL) {
        // No saved aliases file
        return;
    }

    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the line
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "alias", 5) == 0) {
            handle_alias(line);
        }
    }

    fclose(file);
}

void display_user_info() {
        // 1. Username
        char *username = getenv("USER");
        printf("1. Username: %s\n", username);

        // 2. Hostname
        char host[MAX_PATH_SIZE];
        gethostname(host, sizeof(host));
        printf("2. Hostname: %s\n", host);

        // 3. Last Executed Command
        // Note: You may need to maintain a global variable to store the last executed command.

        // 4. TTY
        char *tty = ttyname(STDIN_FILENO);
        printf("4. TTY: %s\n", tty);



        // 5. Current Shell Name (Linux-specific)
        //TODO


        // 6. Home Location
        char *home = getenv("HOME");
        printf("6. Home Location: %s\n", home);

        // 7. Current Time and Date
        time_t current_time;
        struct tm *time_info;

        time(&current_time);
        time_info = localtime(&current_time);

        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
        printf("7. Current Time and Date: %s\n", time_str);

        // 8. Current number of processes being executed
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        //printf("8. Current number of processes being executed: %ld\n", usage.ru_nprocs);
}

void reverse_string(char *str) {
    if (str == NULL) {
        return;
    }

    int length = strlen(str);
    int start = 0;
    int end = length - 1;

    while (start < end) {
        // Swap characters at start and end positions
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;

        start++;
        end--;
    }
}

void reverse_last_line(char *filename) {
    FILE *file = fopen(filename, "r+");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char currentLine[256];
    char lastLine[256] = ""; // Initialize to an empty string
    long int lastLinePosition = -1;

    while (fgets(currentLine, sizeof(currentLine), file) != NULL) {
        lastLinePosition = ftell(file) - strlen(currentLine); // Record the position of the last line
        strcpy(lastLine, currentLine);
    }

    fclose(file);

    // Check for empty file or a single line
    if (lastLinePosition == -1) {
        return;
    }

    // Remove the newline at the end of lastLine
    if (strlen(lastLine) > 0 && lastLine[strlen(lastLine) - 1] == '\n') {
        lastLine[strlen(lastLine) - 1] = '\0';
    }

    // Reverse the last line
    reverse_string(lastLine);

    // Open the file again for writing
    file = fopen(filename, "r+");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Move to the position of the last line
    fseek(file, lastLinePosition, SEEK_SET);

    // Overwrite the last line with its reversed version
    fprintf(file, "%s", lastLine);

    // Add a newline character if the original last line had one
    if (lastLine[strlen(lastLine) - 1] != '\n') {
        fprintf(file, "\n");
    }

    fclose(file);
}


// Function to save the reverse_flag to a file
void save_reverse_flag(int flag) {
    FILE *file = fopen("reverse_flag.txt", "w");
    if (file != NULL) {
        fprintf(file, "%d", flag);
        fclose(file);
    }
}

// Function to load the reverse_flag from a file
int load_reverse_flag() {
    int flag = 0;
    FILE *file = fopen("reverse_flag.txt", "r");
    if (file != NULL) {
        fscanf(file, "%d", &flag);
        fclose(file);
    }
    return flag;
}


void save_filename(char *filename) {
    FILE *file = fopen("filenames.txt", "w");
    if (file != NULL) {
        fprintf(file, "%s", filename);
        fclose(file);
    }
}

char* retrieve_filename() {
    char *filename = NULL; // Allocate memory for the pointer
    FILE *file = fopen("filenames.txt", "r");
    if (file != NULL) {
        // Allocate memory for the string
        filename = (char *)malloc(256); // You can adjust the size as needed
        if (filename == NULL) {
            perror("Memory allocation failed");
            fclose(file);
            return NULL;
        }

        // Use %255s to prevent buffer overflow (leaving one byte for the null terminator)
        if (fscanf(file, "%255s", filename) != 1) {
            // Handle fscanf failure
            perror("Error reading filename from file");
            free(filename); // Free allocated memory on failure
            filename = NULL;
        }

        fclose(file);
    }
    return filename;
}

