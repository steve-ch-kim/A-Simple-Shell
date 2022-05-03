#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>

#define MAXLINE 80
#define MAXARGS 80
#define MAXJOBS 5
#define BACKGROUND '&'
#define _POSIX_SOURCE

struct process
{
    int cmd;
    int jid;
    int pid;
    char argu[MAXARGS];
    int status;
    // -1: terminated, 0: stopped, 1: bg/running, 2: fg/running
};

struct process main_array[5];
int main_size = 0;
int quitter = 0;
int give_jid = 1;

int to_do_redirection(char *argv[MAXARGS]) {
    int count = 0;
    int i = 0;

    while (argv[i] != NULL) {
        if (!strcmp(argv[i], ">") || !strcmp(argv[i], "<") || !strcmp(argv[i], ">>")) {
            count++;
        }

        i++;
    }

    return count;
}

void update_jid(int index) {
    give_jid--;

    for (int x = index; x < main_size; x++)
    {
        main_array[x].jid--;
    }
}

void sigchld_handler(int sig) {
    pid_t child_pid;
    int status;

    while ((child_pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
            for (int i = 0; i < main_size; i++) {
                if (main_array[i].pid == child_pid) {
                    main_array[i].status = 0;
                }
            }

            kill(child_pid, SIGTSTP);
        } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
            int counter = 0;

            for (int i = 0; i < main_size; i++) {
                if (main_array[i].pid == child_pid) {
                    main_array[i].status = -1;
                    main_array[i].jid = -1;
                    counter = i;
                }
            }
            
            update_jid(counter);
            kill(child_pid, SIGINT);
        } else {
            fprintf(stdout, "%s\n", "waitpid error");
        }
    }
}

void io_redirect(char *argv[MAXARGS]) {
    int i = 0;
    
    while (argv[i] != NULL) {
        mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; 

        if (!strcmp(argv[i], "<")) {     
            int y = i; 
            int flag = 0;

            while (argv[y] != NULL) {
                if (!strcmp(argv[y], ">")) {
                    flag++;
                    break;
                }

                y++;
            }

            if (flag) {
                int inFileID = open(argv[i + 1], O_RDONLY, mode);
                dup2(inFileID, STDIN_FILENO);
                int outFileID =  open(argv[y + 1], O_CREAT | O_WRONLY | O_TRUNC, mode); 
                dup2(outFileID, STDOUT_FILENO);                         

                char *command[MAXLINE];
                command[0] = argv[0];
                command[1] = NULL;
                
                if (execv(command[0], command) < 0) {
                    printf("error\n");
                }   
            } else {
                int inFileID = open(argv[i + 1], O_RDONLY, mode);
                dup2(inFileID, STDIN_FILENO);                             
                close(inFileID);

                char *command[MAXLINE];
                command[0] = argv[0];
                command[1] = NULL;
                
                if (execv(command[0], command) < 0) {
                    printf("error\n");
                }   
            }    
        }                                
        else if (!strcmp(argv[i], ">")) { 
            int outFileID =  open(argv[i + 1], O_CREAT | O_WRONLY | O_TRUNC, mode); 
            dup2(outFileID, STDOUT_FILENO);   
            close(outFileID);  

            char *command[MAXLINE];
            command[0] = argv[0];
            

            if (i == 1) {
                command[1] = NULL;

                if (execvp(command[0], command) < 0 )
                    printf("error\n");
            } else {
                command[1] = argv[i - 1];
                command[2] = NULL;

                if (execvp(command[0], command)) {
                    printf("error\n");
                }
            }    
        }
        else if (!strcmp(argv[i], ">>")) {      
            int outFileID =  open(argv[i + 1], O_CREAT | O_APPEND | O_WRONLY, mode);  
            dup2(outFileID, STDOUT_FILENO);      
            close(outFileID);
            
            char *command[MAXLINE];
            command[0] = argv[0];

            if (i == 1) {
                command[1] = NULL;
            } else {
                command[1] = argv[i - 1];
                command[2] = NULL;
            }    

            if (execvp(command[0], command) < 0) {
                if (execv(command[0], command) < 0 )
                    printf("error\n");
            }       
        }

        i++;                                 
    }                                       
}

int builtin_command(char *argv[MAXARGS]) {
    if (!strcmp(argv[0], "quit")) {
        quitter = 1;

        return 1;
    } else if (!strcmp(argv[0], "jobs")) {
        for (int i = 0; i < main_size; i++) {
            if (main_array[i].status == 1 || main_array[i].status == 2) {
                printf("[%i] (%i) Running %s\n", main_array[i].jid, main_array[i].pid, main_array[i].argu);
            } else if (main_array[i].status == 0){
                printf("[%i] (%i) Stopped %s\n", main_array[i].jid, main_array[i].pid, main_array[i].argu);
            }
        }

        return 1;
    } else if (!strcmp(argv[0], "fg")) {
        for (int i = 0; i < main_size; i++) {
            if (atoi(argv[1]) == main_array[i].pid || (argv[1][1] - '0') == main_array[i].jid) {
                main_array[i].status = 2;
                kill(main_array[i].pid, SIGCONT);
                pause();
            }
        }

        return 1;
    } else if (!strcmp(argv[0], "bg")) {
        for (int i = 0; i < main_size; i++) {
            if (atoi(argv[1]) == main_array[i].pid || (argv[1][1] - '0') == main_array[i].jid) {
                main_array[i].status = 1;
                kill(main_array[i].pid, SIGCONT);
            }
        }

        return 1;
    } else if (!strcmp(argv[0], "cd")) {
        int i = 0;

        while (argv[i] != NULL) {
            i++;
        }
        
        chdir(argv[1]);
        
        return 1;
    } else if (!strcmp(argv[0], "kill")) {
        int counter = 0;

        for (int i = 0; i < main_size; i++) {
            if (atoi(argv[1]) == main_array[i].pid || (argv[1][1] - '0') == main_array[i].jid) {
                main_array[i].status = -1;
                counter = i;
                kill(main_array[i].pid, SIGINT);
            }
        }

        update_jid(counter);
        wait(NULL);
        
        return 1;
    } else {
        return 0;
    }
}

int parseline(char *cmdLine, char *argv[MAXARGS])
{
    int bg = 0;
    int size = 0;

    while (cmdLine[size] != '\0') {
        size++;
    }

    if (cmdLine[size - 2] == BACKGROUND) {
        bg = 1;
    }

    int i = 0;
    *argv = strtok(cmdLine, " \t\n");
    
    while (1) {
        if (argv[i] == NULL) {
            break;
        }
        if (i == MAXARGS - 1) {
            argv[i] = NULL;
            break;
        }

        argv[++i] = strtok(NULL, " \t\n");
    }
    
    return bg;
}

int eval(char *cmdLine) {
    char *argv[MAXARGS];
    int bg;             
    pid_t pid;
    int ret_val;
    
    bg = parseline(cmdLine, argv);

    if (!builtin_command(argv)) {  
        if ((pid = fork()) == 0) {  
            if (to_do_redirection(argv)) { 
                io_redirect(argv);
            } else if (execvp(argv[0], argv) < 0) {
                if (execv(argv[0], argv) < 0 )
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
            }
        }
        
        struct process curr_process;
        curr_process.pid = pid;
        curr_process.jid = give_jid;
        give_jid++;
        curr_process.cmd = bg;

        ret_val = pid;
        setpgid(0, 0);

        if (!bg) {   
            curr_process.status = 2;

            main_array[main_size] = curr_process;
            main_size++;
            
            pause();
        } else { 
            curr_process.status = 1;

            main_array[main_size] = curr_process;
            main_size++;

            pause();
            //printf("%d %s", pid, cmdLine);
        }
    } else {
        ret_val = -1;
    }

    return ret_val;
}

int main(void) {
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigchld_handler);
    signal(SIGTSTP, sigchld_handler);
    signal(SIGCONT, sigchld_handler);

    char cmdLine[MAXLINE];

    while(1)
    {
        printf("prompt> ");
        fgets(cmdLine, MAXLINE, stdin);
        
        if (feof(stdin))
        {
            exit(0);
        }

        char temp[MAXLINE];
        strcpy(temp, cmdLine);

        int built_in = eval(cmdLine);

        if (built_in > 0)
        {
          for (int i = 0; i < main_size; i++)
            {
              if (main_array[i].pid == built_in)
                {
                  strcpy(main_array[main_size - 1].argu, temp);
                }
            }
        }

        if (quitter) {
            break;
        }
    }

    return 0;
}
