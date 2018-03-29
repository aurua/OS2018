/**
 * 
 * Project 01 simple unix shell 
 * 
 * Implements simple shell program which can be run in interactive mode & batch mode.
 * "cd" command, pipe line, input redirection and etc, are not supported.  
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
#define BUFFER_SZ 65535
#define MAX_CHILDS 4096
/**
 * Read single line of command input from "p_file" file descriptor, 
 *  @param[in]      buf char buffer which is used for saving command. 
 *  @param[in]      nbuf size of buffer -1
 *  @param[in]      p_file file descriptor which connected to desired input stream.
 *  @return         0 means something has been read
 *                  -1 means empty string.
 **/
int getcmd(char *buf, int nbuf, FILE* p_file)
{
    if(fgets(buf,nbuf,p_file))
        return 0;
    else
        return -1;
}

/**
 *  
 *  Parse single command to execvp's arguments.
 *  single command means it has no semicolon in that command.   
 *  @warning        It calls strtok with cmd parameter.
 *  @param[in]      cmd string of single command.
 *  @return         execvp's arguments or NULL pointer.
 **/
char** parsecmdtoargs(char *cmd)
{
    //temp variable to save argv.
    char* tempargv[_POSIX_ARG_MAX];
    //return variable should located in heap memory. 
    char** new_argv;
    int argc = 0, i = 0;
    char* token = strtok(cmd, WHITE_SPACE_DELIM);
    while(token) {
        tempargv[argc++]=token;
        token = strtok(NULL, WHITE_SPACE_DELIM);
    }
    //if cmd has only white spaces or empty, returns NULL.
    if(argc == 0)
        return NULL;
    //save temp argv to return variable.
    new_argv = (char**)malloc( sizeof(char*) * (argc+1) );
    for (i =0; i< argc; ++i) {
        new_argv[i] = tempargv[i];
    }
    new_argv[argc] = NULL;
    return new_argv;
}


int main(int argc, char **argv)
{
    //shell's input file descriptor, can be file or stdin.
    FILE * p_file = stdin;
    //buffer for inputs.
    char buffer[BUFFER_SZ+1];
    //saves forked process's pid.
    int child_pids[MAX_CHILDS];
    //the number of child that forked from parent.
    int num_of_child = 0;
    //determines shell to run as batch mode or interactive mode.
    int is_batch = FALSE;
    //in the case of quit command, it will turn to true.
    int has_end_sequence = FALSE;
    //i variable used for looping.
    int i;

    //if arguments are more than 2, it would be batch mode.
    if(argc >= 2) {
        //try to open file, if it fails then end with error message.
        p_file = fopen (argv[1] , "r");
        is_batch = TRUE;
        if (p_file == NULL) { 
            fprintf (stderr,"Error with opening file %s",argv[1]);
            return -1;
        }
    }

    {
        //this pointer will indicate string of single command.
        char* single_command;
        char** new_argv;
        do{
            //passed_chars is the number of characters that already processed from buffer.  
            int passed_chars = 0;

            if(!is_batch) {
                printf("prompt> ");
            }
            //read input from p_file and save to buffer.
            if(getcmd(buffer,BUFFER_SZ,p_file) != 0) {
                //if it reaches the end of file or get crrl+D, finish the shell.
                return 1;
            }
            
            if(is_batch) {    
                printf("%s", buffer);
            }

            //to deal with @l1(find @l1 below), extra null character is needed.
            buffer[strlen(buffer)+1] = '\0';
            //to get single command, strtok buffer with semicolon.
            single_command = strtok(buffer, CMD_DELIM);
            while(single_command) {
                //the length of single command and null characters are processed by strtok.
                passed_chars += strlen(single_command)+1;
                //tokenized single command will parsed to argv.
                new_argv = parsecmdtoargs(single_command);
                if(new_argv == NULL) {
                    break;
                }
                //if new_argv[0] equals to quit, it should be ended. 
                if(strcmp(new_argv[0],"quit") == 0) {
                    has_end_sequence = TRUE;    
                    break;
                }
                //fork the process
                // if it is parent then save child's pid to child_pids.
                // if it is child the process will be changed by execvp. 
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
                //@l1
                //strtok was called with new pointer(from parsecmdtoargs function call)
                //To tokenize the rest of buffer, strtok should be called with starting point to be parsed.  
                single_command = strtok(buffer+passed_chars, CMD_DELIM);
                free(new_argv);
            }
            //wait for children process to be ended.
            for(i=0; i<num_of_child; i++) {
                waitpid(child_pids[i], NULL,0);
            }
            num_of_child = 0;
        //if quit was called then end.
        }while(!has_end_sequence);

    }
    
    return 0;
}
