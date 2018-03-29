/**
 * 
 * Project 01 simple unix shell 
 * 
 * Implements simple shell program which can be run in interactive mode & batch mode.
 * "cd" command, pipe line, input redirection and etc, are not supported.  
 * 
 * 
 * 
 * @author JoongHoon Kim
 * @since 2018-03-27
 * 
 */
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//ARG_MAX:Maximum length of argument to the exec functions including environment data.
//_POSIX_ARG_MAX is defined in limits.h and it is minimum acceptable value(According to The Open Group Specifications). 
#include <limits.h>
#define TRUE 1
#define FALSE 0
#define WHITE_SPACE_DELIM " \t\r\n\f\v"
#define CMD_DELIM ";"
#define BUFFER_SZ 65536
#define MAX_CHILDS 4096
int getcmd(char *buf, int nbuf, FILE* p_file)
{
    if(fgets(buf,nbuf,p_file))
        return 0;
    else
        return -1;
}

char** parsecmdtoargs(char *cmd)
{
    char* tempargv[_POSIX_ARG_MAX];
    char** new_argv;
    int argc = 0, i = 0;
    char* token = strtok(cmd, WHITE_SPACE_DELIM);
    while(token) {
        tempargv[argc++]=token;
        token = strtok(NULL, WHITE_SPACE_DELIM);
    }
    if(argc == 0)
        return NULL;
    new_argv = (char**)malloc( sizeof(char*) * (argc+1) );
    for (i =0; i< argc; ++i) {
        new_argv[i] = tempargv[i];
    }
    new_argv[argc] = NULL;
    return new_argv;
}


int main(int argc, char **argv)
{
    FILE * p_file = stdin;
    char buffer[BUFFER_SZ];
    int child_pids[MAX_CHILDS];
    int num_of_child = 0;
    int is_batch = FALSE;
    int has_end_sequence = FALSE;
    int i;
    if(argc >= 2) {
        p_file = fopen (argv[1] , "r");
        is_batch = TRUE;
        if (p_file == NULL) { 
            fprintf (stderr,"Error with opening file %s",argv[1]);
            return -1;
        }
    }

    {
        char* token;
        char** new_argv;
        do{
            int passed_chars = 0;
            if(!is_batch) {
                printf("prompt> ");
            }
            if(getcmd(buffer,BUFFER_SZ,p_file) != 0)
                return 1;
            if(is_batch)    
                printf("%s", buffer);
            buffer[strlen(buffer)+1] = '\0';
            token = strtok(buffer, CMD_DELIM);
            while(token) {
                char tokened_cmd[BUFFER_SZ];
                passed_chars += strlen(token)+1;
                strcpy(tokened_cmd,token);
                new_argv = parsecmdtoargs(tokened_cmd);
                if(new_argv == NULL)
                    break;
                if(strcmp(new_argv[0],"quit") == 0) {
                    has_end_sequence = TRUE;    
                    break;
                }
                int pid = fork();
                switch(pid) {
                    case -1:
                        printf("failed to create child process");
                        return -1;
                    case 0 :
                        if(execvp(new_argv[0],new_argv) == -1) {
                            fprintf(stderr, "error %s\n",strerror(errno));
                            return 1;
                        }
                        break;
                    default :
                        child_pids[num_of_child++] = pid;
                        break;
                }
                token = strtok(buffer+passed_chars, CMD_DELIM);
                free(new_argv);
            }
            
            for(i=0; i<num_of_child; i++) {
                waitpid(child_pids[i], NULL,0);
            }
            num_of_child = 0;
        }while(!has_end_sequence);

    }
    
    return 0;
}
