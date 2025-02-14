#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#define MAX_HISTORY 100 // Число команд (с конца), которые будут записаны в файл
#define HISTORY_FILE "command_history.txt" // Название файла для хранения истории
#define MAX_ARGS 10 // Максимальное кол-во аргументов

void sighup(int signum) {
    printf("Configuration reloaded\n");
}

int main() {
    signal(SIGHUP, sighup);
    printf("PID: %d\n", getpid());


    char input[100]; // Введенная строка с клавиатуры

    char history[MAX_HISTORY][100];
    int history_index = 0;
    int commands_stored = 0;

    while (1) {
        printf("> ");

        fflush(stdout);

        if (fgets(input, 100, stdin) == NULL) {
            break;
        }

        unsigned long n = strlen(input);
        input[n - 1] = '\0';

        strcpy(history[history_index % MAX_HISTORY], input);
        history_index++;
        if (commands_stored < MAX_HISTORY) {
            commands_stored++;
        }

        if (strcmp(input, "exit") == 0 || strcmp(input, "\\q") == 0) {
            break;
        }
        if (strncmp(input, "echo", 4) == 0) {
            printf("%s\n", input + 5);
            continue;
        }
        if (strncmp(input, "\\e", 2) == 0) {
            char * var_name = input + 3; // Получаем имя переменной окружения
            char * value = getenv(var_name); // Получаем значение, если еге нет, то вернет NULL
            if (value != NULL) {
                printf("%s\n", value);
            }
            else {
                printf("Error: environment variable '%s' not found\n", var_name);
            }
            continue;
        }
        if (strncmp(input, "!", 1) == 0) {
            char * binary_file_name = input + 1; // Получаем имя двоичного файла
            char * args[MAX_ARGS]; // Массив для аргументов
            int arg_count = 0; // Кол-во аргументов

            // Разделяем строку на аргументы
            char * token = strtok(binary_file_name, " ");
            while (token != NULL && arg_count < MAX_ARGS - 1) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL; // Маркер конца

            pid_t pid = fork();
            if (pid < 0) {
                perror("Error with fork");
                continue;
            }

            if (pid == 0) {
                execvp(args[0], args);
                perror("Error with exec");
                exit(1);
            }

            int status;
            waitpid(pid, & status, 0); // Ожидаем завершения дочернего процесса
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("Error: Failed to run binary file %s\n", args[0]);
            }

            continue;
        }
        if (strncmp(input, "\\l", 2) == 0) {
            char * device = input + 3;

            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                continue;
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                continue;
            }

            if (pid == 0) {
                close(pipefd[0]); //  Закрываем конец пайпа для чтения

                // Перенаправляем стандартный вывод в пайп
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                char * args[] = {"diskutil", "list", device, NULL};

                execvp(args[0], args);
                perror("exec");
                exit(1);
            }
            else {
                close(pipefd[1]); // Закрываем конец пайпа для записи

                int status;
                waitpid(pid, &status, 0);

                char buffer[256];
                ssize_t bytesRead;
                while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[bytesRead] = '\0'; // Добавляем нулевой символ для корректного вывода
                    printf("%s", buffer); // Выводим данные
                }

                close(pipefd[0]); // Закрываем конец пайпа для чтения

                if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    printf("Error: Failed to retrieve information for device %s\n", device);
                }
            }

            continue;
        }
        if (strcmp(input, "\\cron") == 0) { // Обработка команды \cron
            const char * vfs_dir = "/tmp/vfs";
            const char * tasks_file = "/tmp/vfs/tasks";

            // Создаем виртуальную файловую систему (каталог /tmp/vfs)
            if (access(vfs_dir, F_OK) == -1) {
                if (mkdir(vfs_dir, 0755) != 0) {
                    perror("Error creating VFS directory");
                    continue;
                }
                printf("VFS mounted at %s\n", vfs_dir);
            }
            else {
                printf("VFS already mounted at %s\n", vfs_dir);
            }

            // Открываем файл для записи списка задач
            FILE * fp = fopen(tasks_file, "w");
            if (fp == NULL) {
                perror("Error creating tasks file in VFS");
                continue;
            }

            // Получаем список задач из crontab
            FILE * crontab_output = popen("crontab -l", "r");
            if (crontab_output == NULL) {
                perror("Error reading crontab");
                fclose(fp);
                continue;
            }

            // Читаем вывод команды crontab -l и записываем в файл
            char line[256];
            while (fgets(line, sizeof(line), crontab_output) != NULL) {
                fprintf(fp, "%s", line);
            }

            pclose(crontab_output);
            fclose(fp);

            printf("Tasks saved to %s\n", tasks_file);
            continue;
        }
        printf("%s: command not found\n", input);
    }
    FILE * fp = fopen(HISTORY_FILE, "w");
    for (int i = 0; i < commands_stored; i++) {
        fprintf(fp, "%s\n", history[i]);
    }
    fclose(fp);

    return 0;
}