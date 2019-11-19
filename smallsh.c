/* OSU CS344 Project 3 - smallsh.c
 * Author: Xavier Hollingsworth
 * Date: 02/24/2019
 * Description: This program creates a shell with 3 built-in commands: cd, status, exit. Every other command 
 * is execvp'ed. 
 * */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_ARGUMENTS 512
#define MAX_CHARACTERS 2048
#define BUFFER 1028


/* Define bool */
typedef enum {false, true} bool;



/* Global variables */
bool foregroundOnly = false;
bool foregroundProcess = false;
int pid;

/* Functions */


/* Name: catchInterrupt
 * Desc: Catches CTRL-C
 * */

void catchInterrupt(int signalNumber)
{
    printf("\nterminated by signal %d\n", signalNumber);
}


/* Name: catchSIGSTP
 * Desc: Catches CTRL-Z
 * */
void catchSIGSTP(int signalNumber)
{
  if (foregroundOnly)
  {
    // Dispplay exiting foregroud only mode
    char *message = "\nExiting foreground-only mdoe\n";
    write(STDOUT_FILENO, message, 31);
    // Change foregroundOnly flag
    foregroundOnly = false;
  }
  else 
  {
    // Print message to terminal about foreground-only mode
    char *message = "\nEntering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, message, 50);
    // Change foregroundOnly flag
    foregroundOnly = true;
  }
}



/* Name: printStatus
 * Desc: Prints either the exit status or the terminating signal of the last foreground process ran by my shell
 * */
void printStatus(int status)
{
  if (WIFEXITED(status))
  {
    // Retrieve exit status and print out to screen 
    int exitStatus = WEXITSTATUS(status);
    printf("exit value %d\n", exitStatus);
  }
  else if (WIFSIGNALED(status))
  {
    // Retrieve termination signal and print out to screen
    int signal = WTERMSIG(status);
    printf("terminated by signal %d\n", signal);
  }
}


/* Name: expand
 * Desc: Function to replace $$ with the the process ID
 * */
char* expand(char* input)
{
  // Initialize variables
  int length = strlen(input) + 1;
  int pid = getpid();
  int n;
  int size = sizeof(pid);
  length += size;
  char* strpid = malloc(sizeof(size));
  int pos1;
  bool expansion = false;
  char *newstr = malloc(sizeof(length+1));
  
  // Convert decimal to string
  sprintf(strpid, "%d", pid);

  // Loop through string looking for $
  for (n = 0; n < length; n++)
  {
    if(input[n] == '$' && input[n+1] == '$')
    {
      // Set first $ poistion
      pos1 = n;
      // Set expansion flag
      expansion = true;
    }
  }

  // IF $$ was present in the string then replace it with the pid
  if (expansion)
  {
    for (n = 0; n < pos1; n++)
    {
      newstr[n] = input[n];
    }
    // Replace pid where $$ previously was
    strcat(newstr, strpid);
    for (n = (pos1 + size + 1); n < length; n++)
    {
      newstr[n] = input[n-2];
    }
    return newstr;
  }

  // Retrun original string if no $$ was found
  else
  {
    return input; 
  }
}




/* Main Function */
int main()
{
  // Initialize variables
  char inputBuffer[MAX_CHARACTERS];
  char *strCopy = NULL;
  char *newString = NULL;
  char *args[MAX_ARGUMENTS];
  char *token;
  char *inputFileName = NULL;
  char *outputFileName = NULL;
  int spawnPid, status;
  int fileIn = -1;
  int fileOut = -1;
  int pid;
  bool foregroundCommandRunning = false;

  // Initialize sigaction structs
  struct sigaction sigint_int = {0}, sigint_stp = {0}, sigint_child = {0};

  // Setup actions and handlers for sigaction structs
  sigint_int.sa_handler = SIG_IGN;
  sigint_int.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sigint_int, NULL);

  sigint_stp.sa_handler = catchSIGSTP;
  sigint_stp.sa_flags = 0;
  sigaction(SIGTSTP, &sigint_stp, NULL);


  // Run shell until user exits
  do
  {
    // Reset background flag
    foregroundProcess = true; 


    // Reset variable to parse user entry
    int index = 0;

    // Clear stdout and print prompt
    fflush(stdout);
    printf(": ");

    // Clear input buffer
    memset(inputBuffer, '\0', sizeof(inputBuffer));

    // Get user input
    fgets(inputBuffer, MAX_CHARACTERS, stdin);
 
    // Copy user input to another string
    strCopy = expand(inputBuffer);

    // Parse user input
    token = strtok(strCopy, " \n");

    // Loop through input until it reaches the end
    while (token != NULL)
    {
      // User wants to run background process
      if (strcmp(token, "&") == 0)
      {
        foregroundProcess = false;
	if (foregroundOnly == true)
	{
	  ;
	}
	break;
      }
      else 
      {
        // Set foreground fkag
        foregroundCommandRunning = true;
      }

      // User wants to retrieve output from input file
      if (strcmp(token, "<") == 0)
      {
        // Parse input to next argument to get input file name
        token = strtok(NULL, " \n");
	// Duplicate string in token to inputFileName
	inputFileName = strdup(token);
	// Parse input for next argument
	token = strtok(NULL, " \n");
      }
      // User wants to output to output file 
      else if (strcmp(token, ">") == 0)
      {
        // Parse input to next argument to get output file name
	token = strtok(NULL, " \n");
	// Duplicate the string in token to outputFileName
	outputFileName = strdup(token);
	// Parse input for next argument
	token = strtok(NULL, " \n");
      }

      // It is an argument so save the token to the argument buffer
      else
      {
        // Duplicate the string in token to args
        args[index] = strdup(token);
	// Parse next word
	token = strtok(NULL, " \n");
	// Increment index to point to the next argument in user input
	index++;
      }
    }
    // Indicate end of arguments array
    args[index] = NULL;

    // If foreground command is running catch the signal
    if (foregroundCommandRunning)
    {
      sigint_int.sa_handler = catchInterrupt;
      sigaction(SIGINT, &sigint_int, NULL);
    }

    // User entered a comment
    if (strncmp(strCopy, "#", 1) == 0)
    {
      // Rerun the main while loop
      ;
    }

    // User entered a blank line
    else if (strCopy[0] == '\n' || strCopy[0] == '\0')
    {
      // Rerun the main while loop
      ;
    }

    // Change directory
    else if (strncmp(strCopy, "cd", 2) == 0)
    {
      // If there is no second argument return to home path
      if (args[1] == NULL)
      {
        // Display error message if HOME path not found
        if(chdir(getenv("HOME")) != 0)
	{
	  printf("-bash: %s: directory not found. \n", getenv("HOME"));
	  return 1;
	}
      }
      // Change to path input by user
      else
      {
        // Display error meeage if directory is not found
        if(chdir(args[1]) != 0)
	{
	  printf("-bash: %s: no such directory found\n", args[1]);
	}
      }
    }
    
    // Check either exit status or the terminating signal of the last foreground process ran by the shell
    else if (strncmp(strCopy, "status", 6) == 0)
    {
      printStatus(status);
    }

    // Exit the shell
    else if (strncmp(strCopy, "exit", 4) == 0)
    {
      exit(0);
    }
    
    // User entered something other than cd, exit or status
    else
    {
      // If foreground-only mode has been sent ignore it
      if (foregroundOnly == true && foregroundProcess == false)
      {
        ;
      }
      else
      {
        spawnPid = fork();
        switch(spawnPid)
        {
          // Fork Error
	  case -1:
            perror("Hull Breach! Error with fork().\n");
	    status = 1;
	    break;

          // Child Process
	  case 0:
	    // If User specified an input file
	    if(inputFileName != NULL)
	    {
	      // Open stdin file descriptor
	      fileIn = open(inputFileName, O_RDONLY);
	      // Error checking if input file was unable to be opened
	      if (fileIn == -1)
	      {
	        printf("smallsh: cannot open %s for input\n", inputFileName);
	        fflush(stdout);
	        exit(1);
	      }
	      // Point stdin to fileIn file descriptor
	      if (dup2(fileIn, 0) == -1)
	      {
	        // Display error message and exit
	        perror("dup2");
	        exit(1);
	      }
	      // Close new stdin file descriptor
	      close(fileIn);
	    }
	    
	    // If it is not a foreground process input file is dev/null
	    else if (!foregroundProcess)
	    {
	      fileIn = open("/dev/null", O_RDONLY);
	      // Display error message if it is not opened
	      if (fileIn == -1)
	      {
	        perror("open()");
	        exit(1);
	      }
	      
	      // Duplicate stdin to fileIn, display error and exit if it does not succeed
	      if (dup2(fileIn, 0) == -1)
	      {
	        perror("dup2");
	        exit(2);
	      }
	    }
          
	      // If there is an output file
	      if (outputFileName != NULL)
	      {
	        // Open outputfile
	        fileOut = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0744);
     
               // Display error message if one has occurred
	        if (fileOut == -1)
	        {
	          printf("smallsh: cannot open %s\n", outputFileName);
	          fflush(stdout);
	          exit(1);
	        }
	        // Duplicate stdout to fileOut, display error message and exit if it does not succeed
	        if (dup2(fileOut, 1) == -1)
	      {
	        perror("dup2");
	        exit(2);
	      }
	      // Close new stdout file descriptor
	      close(fileOut);
	     }
          

	    // Run the command - display an error message and exit if the command is not found
	    if (execvp(args[0], args) < 0)
	    {
	      printf("smallsh: %s: command not found\n", args[0]);
	      exit(1);
	    }
	
	  // Indicates parent process
	  default:
	    if (foregroundProcess)
	    {
	      waitpid(spawnPid, &status, 0);
	    }
	    else
	    {
	      printf("background pid is %d\n", spawnPid);
	      break;
	    }
         }
      }
    }

    // Cleanup character pointers
    free(inputFileName);
    inputFileName = NULL;
    free(outputFileName);
    outputFileName = NULL;
    
    // Cleanup input buffer
    int n;
    for (n = 0; args[n] != NULL; n++)
    {
      free(args[n]);
    }
    
    // Check if any process has completed
    spawnPid = waitpid(-1, &status, WNOHANG);
    while (spawnPid > 0)
    {   
      printf("background pid %d is done: ", spawnPid);
      printStatus(status);
      spawnPid = waitpid(-1, &status, WNOHANG);
    }


  } while(true);

  return 0;
}
