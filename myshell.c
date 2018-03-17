/****************************************************************
 * Name: Keawa Rozet                                            *
 * Class: CSC 415                                               *
 * Due Date: 3/16/2108                                          *
 * Description :  Writting a simple bash shell program          *
 * 	        	  that will execute simple commands. The main     *
 *                goal of the assignment is working with        *
 *                fork, pipes and exec system calls.            *
 ****************************************************************/

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sys/wait.h"
#include <fcntl.h>
#include <errno.h>

#define BUFFERSIZE 256
#define PROMPT "@ myEggShell "
#define PARROWS " -> "
#define PROMPTSIZE sizeof(PROMPT)
#define ARGVMAX 64
#define PIPECNTMAX 10
#define FLAGSMAX 10
#define FLAG_OUT 1
#define FLAG_OUT_APPEND 2
#define FLAG_IN 3
#define clear() printf("\033[H\033[J")

int getArguments(char* userInput, char** myargv, int* pipeArgCounter, int* myargc, int* flagArray);
int getUserInput(char* line, int* resetBufferSize);
void reset(char* buffer, int* pipeArgCounter, int*flagArray, int* resetBufferSize, int* myargc, int* flag);
void handlePipedCommands(char** myargv, int* pipeArgCounter);
int handleBuiltinCommands(char** commands, int* myargc);
void handleSystemCommands(char** commands, int* myargc, int* flag);
void printCurrentDirectory();
void introPrompt();
void printPrompt();
void printHelp();

int main(int* argc, char** argv) {
  char userInput[BUFFERSIZE];
  char* myargv[ARGVMAX];
  int pipeArgCounter[PIPECNTMAX] = {0};
  int flagArray[FLAGSMAX] = {0};
  int resetBufferSize = 0;
  int myargc = -1;
  int noInput = 0;
  int flag = 0;
  introPrompt();

  while (1) {
    printPrompt();
    noInput = getUserInput(userInput, &resetBufferSize);
    //user did not type a command
    if (noInput)
      continue;
    //echo command
    //printf("%s\n", userInput);

    getArguments(userInput, myargv, pipeArgCounter, &myargc, flagArray);

    //there is at least one piped argument
    if (pipeArgCounter[0] > 0) {
      handlePipedCommands(myargv, pipeArgCounter);
    }
    //there is at least 1 flag
    else if (flagArray[0] > 0) {
        flag = flagArray[0];
        handleSystemCommands(myargv, &myargc, &flag);
    }
    //no flags, no pipes
    else if (handleBuiltinCommands(myargv, &myargc) == 0) {
      handleSystemCommands(myargv, &myargc, &flag);
    }

    reset(userInput, pipeArgCounter, flagArray, &resetBufferSize, &myargc, &flag);
  }
  return 0;
}

void reset(char* buffer, int* pipeArgCounter, int* flagArray, int* resetBufferSize, int* myargc, int* flag) {
  if (*resetBufferSize > BUFFERSIZE)
    *resetBufferSize = BUFFERSIZE;
  memset(buffer, '\0', *resetBufferSize);
  memset(pipeArgCounter, 0, PIPECNTMAX);
  memset(flagArray, 0, FLAGSMAX);
  *myargc = -1;
  *resetBufferSize = 0;
  *flag = 0;
}

//gets the terminal line from the user
int getUserInput(char* line, int* resetBufferSize) {
  char* buffer;
  buffer = readline(PARROWS);

  if ((*resetBufferSize = strlen(buffer)) != 0) {
    strcpy(line, buffer);
    add_history(buffer);

    free(buffer);
    return 0;
  }

  free(buffer);
  return 1;
}

/*
  gets the arguments from the userInput and also takes note of any pipes or flags found
  Flags: 0 = no flag, FLAG_OUT 1 = >, FlAG_OUT_APPEND 2 = >>, FLAG_IN 3 = <
*/
int getArguments(char* userInput, char** myargv, int* pipeArgCounter, int* myargc, int* flagArray) {
  char* token;
  int count = 1, pipes = 0, flagIndex = 0;

  while ((token = strtok_r(userInput, " ", &userInput)) != NULL) {
    //handles pipes
    if (strcmp(token, "|") == 0) {
      pipeArgCounter[pipes++] = count-1;
      count = 0;
      token = NULL;
    }
    //FLAG_OUT
    else if (strcmp(token, ">") == 0) {
      if(flagIndex >= 10)
        perror("Too many redirects.\n");
      else
        flagArray[flagIndex++] = FLAG_OUT;
    }
    //FLAG_OUT_APPEND
    else if (strcmp(token, ">>") == 0) {
      if(flagIndex >= 10)
        perror("Too many redirects.\n");
      else
        flagArray[flagIndex++] = FLAG_OUT_APPEND;
    }
    //FLAG_IN
    else if (strcmp(token, "<") == 0) {
      if(flagIndex >= 10)
        perror("Too many redirects.\n");
      else
        flagArray[flagIndex++] = FLAG_IN;
    }
    //NO_FLAG and no pipes

    myargv[++(*myargc)] = token;
    count++;
  }
  //null terminates after the last argument
  if (*myargc < ARGVMAX)
    myargv[(*myargc)+1] = '\0';
  //adds the last piped argument
  if (pipes > 0)
    pipeArgCounter[pipes++] = count-1;


  return 0;
}

void handlePipedCommands(char** myargv, int* pipeArgCounter) {
  int pipefd[2];
  int pipeCount = 0;
  pid_t pipe1, pipe2;

  //handles &
  int runBackground = 0;
  if (strcmp(myargv[pipeArgCounter[pipeCount]+pipeArgCounter[pipeCount+1]], "&") == 0) {
    runBackground = 1;
    myargv[pipeArgCounter[pipeCount]+pipeArgCounter[pipeCount+1]] = NULL;
  }

  if (pipe(pipefd) < 0) {
    perror("Pipe failed...\n");
  }

  pipe1 = fork();
  if (pipe1 < 0) {
    perror("Pipe fork failed...\n");
  }
  if (pipe1 == 0) {
    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) == -1)
      perror("dup2.1 failed.");
    close(pipefd[1]);

    if (execvp(myargv[0], myargv) < 0)
      perror(strerror(errno));
  }
  else {
    pipe2 = fork();

    if (pipe2 < 0)
      perror("fork2 failed...");

    if (pipe2 == 0) {
      close(pipefd[1]);
      if (dup2(pipefd[0], STDIN_FILENO) == -1)
        perror("dup2.2 failed.");
        close(pipefd[0]);

      if (execvp(myargv[(pipeArgCounter[pipeCount]+1)],
              (myargv+pipeArgCounter[pipeCount]+1)) < 0) {
        perror(strerror(errno));
      }
    }
    else{
      //closes the ends of the pipes
      close(pipefd[0]);
      close(pipefd[1]);
      //catches parent programs
      //allows for processes to run in the background
      if (runBackground == 0) {
      wait(NULL);
      wait(NULL);
      }
    }
  }
}

void handleSystemCommands(char** commands, int* myargc, int* flag) {
  pid_t pid = fork();
  int fd, cur_in, cur_out;

  //handles &
  int runBackground = 0;
  if (strcmp(commands[*myargc], "&") == 0) {
    runBackground = 1;
    commands[*myargc] = NULL;
  }

  if (pid < 0) {
    perror("fork() failed...");
    exit(-1);
  }
  else if (pid == 0) {
    //handles flag
    if (*flag > 0) {
      commands[*myargc - 1] = NULL;

      switch (*flag) {
        //FLAG_OUT
        case 1:
          fd = open(commands[*myargc], O_CREAT | O_TRUNC | O_WRONLY, 0644);
				  cur_out = dup(STDOUT_FILENO);
				  dup2(fd, STDOUT_FILENO);
				  close(fd);
          break;
        //FlAG_OUT_APPEND
        case 2:
          fd = open(commands[*myargc], O_WRONLY| O_APPEND | O_CREAT, 0644);
          cur_out = dup(STDOUT_FILENO);
          dup2(fd, STDOUT_FILENO);
          close(fd);
          break;
        //FLAG_IN
        case 3:
          fd = open(commands[*myargc], O_CREAT | O_RDONLY, 0644);
          cur_in = dup(STDIN_FILENO);
          dup2(fd, STDIN_FILENO);
          close(fd);
          break;
        default:
          break;
      }
    }

    if (execvp(commands[0], commands) < 0)
      perror(strerror(errno));
  }
  else if (runBackground == 0) {
    waitpid(pid, NULL, 0);
    return;
  }
  else {
    return;
  }
}

int handleBuiltinCommands(char** commands, int* myargc) {
  int size = 4;
  int command = 0;
  char cwd[BUFFERSIZE];
  char* builtinCommands[4] = {"exit", "cd", "help", "pwd"};

  for (int i = 0; i < size; i++) {
    if (strcmp(commands[0], builtinCommands[i]) == 0) {
      command += i+1;
      break;
    }
  }

  switch (command) {
    case 1:
      printf("\nCome back EGG-gain anytime!\n\n");
      exit(0);
    case 2:
      chdir(commands[1]);
      return 1;
    case 3:
      printHelp();
      return 1;
    case 4:
      getcwd(cwd, sizeof(cwd));
      printf("%s\n", cwd);
      return 1;
    default:
      break;
  }
  return 0;
}

void printHelp() {
  printf("\n@ Simple Linux EggShell @\n\n");
  printf("Standard linux commands as well as '&' and '|' will work.\n\n");
  printf("cd\tChanges directory.\n");
  printf("cwd\tPath to current working directory.\n");
  printf("exit\tTo quit the shell.\n\n");
}

void printPrompt() {
  printf("%s", PROMPT);
  printCurrentDirectory();
}

void printCurrentDirectory() {
    char cwd[BUFFERSIZE];
    char realpath[BUFFERSIZE] = "~";
    char* pHome;
    getcwd(cwd, sizeof(cwd));

    //gets user directory
    if ((pHome = getenv("HOME")) != NULL) {
      //checks if user directory is in the working directory
      if (strstr(cwd, pHome) != NULL) {
        strcat(realpath, cwd+(strlen(pHome)));
        printf("%s", realpath);
      }
      else { printf("%s", cwd); }
    }
}

void introPrompt() {
  clear();
  printf("\n\n ,adPPYba,  ,adPPYb,d8  ,adPPYb,d8 \n");
  printf("a8P_____88 a8\"    `Y88 a8\"    `Y88  \n");
  printf("8PP\"\"\"\"\"\"\" 8b       88 8b       88  \n");
  printf("\"8b,   ,aa \"8a,   ,d88 \"8a,   ,d88  \n");
  printf(" `\"Ybbd8\"\'  `\"YbbdP\"Y8  `\"YbbdP\"Y8     .-~-.\n");
  printf("            aa,    ,88  aa,    ,88   .\'     \'.\n");
  printf("             \"Y8bbdP\"    \"Y8bbdP\"   /         \\\n");
  printf("\t\t            .-~-.  :           ;\n");
  printf("\t\t          .\'     \'.|           |\n");
  printf("\t\t         /         \\           :\n");
  printf("\t\t        :           ; .-~\"\"~-,/\n");
  printf("\t\t        |           /`        `\'.\n");
  printf("\t\t        :          |             \\\n");
  printf("\t\t         \\         |             /\n");
  printf("\t\t          `.     .\' \\          .\'\n");
  printf("\t\t            `~~~`    \'-.____.-\'\n\n\n\n");
  }
