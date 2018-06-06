 /*
 * Ricardo Duran
 *
 *Purpose: of this file is to emulate the behavior of a real shell.
 *The program can handle output and input redirection such as:
 * 	">", "<", ">&", ">>", ">>&", and "|".
 * There are other meta-characters such as :
 * # - When the program gets a file as an argument it treats this 
 * 		meta-characters as the starting of a comment line and thus, ignores it.
 * & - If the '&' is at the end of a command it is treated as a background request.
 * 
 * The program also contains some built-ins such as:
 * cd - uses chdir 
 * !! - Gets the previous saved command and executes it.
 * !n - where n is an integer. It fetches the proper command corresponding to that
 * 		number in history.
 * 
 * p2 handles error handling very well without exiting or crashing. However, there could
 * be situations in which crashing is inevitable. It also handles a variety of command
 * combinations that gives the same result.
 * 
 * In general p2 calls parse which in turn calls getword. Parse orders and gives meaning to
 * words retrieved from getword. It also distributes the command to its proper place being some
 * a program name and others, meta-characters. After parsing the the code process the command
 * and executes it by forking p2 and exec with the given program from the command by the user.
 * 
 * INPUT: A file that contains the commands to be executed.
 * OUTPUT: The result of the command if any.
 */
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h> /*chdir*/
#include <stdlib.h> /*getenv*/
#include <sys/wait.h> /*wait(3c)*/
#include <sys/stat.h> /*stat(2)*/
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include "p2.h"
#define MAXSTORAGE 2550	//Big trivial size

/***File Flags **/
bool file_to_stdin;			//True when redirection from file.
bool stdout_to_file;		//True when redirection from file to stdout.
bool append_stdout_to_file;	
bool err_to_file;			//stdout and stderr redirected to a file.
bool append_err_to_file;

/**General Flags **/
extern bool pipeline;	//External variable from getword.
bool background;		//True when & is present at the end of command.	
bool PIPE;				//True if a pipe is going to take place.
bool history_command;	//True when a command is fetched from history.
bool reading_file;

int pid;
int n;				//number of letters returned from getword/strlen.
int counter;			//Prompt counter.
int numberOfCommands;	//Number of commands saved so far.

int input_fd;			//file directive for input file
int output_fd;			//file directive for output file
int lineLength;
int commandLine[255];
int savedCommands;
int fildes[2];

/**Storage for filenames**/
char *outfile;
char *errfile;
char *infile;
char* position;

char buffer[1000];		//Storge for the full command fetched from getword.
static char hist[MAXSTORAGE];	//Storage for saved commands.

struct stat sb;			//For stat subroutine.
struct Command cmd;		//Storage for a command and its arguments
struct Command cmd2;	//Storage for a command after a pipeline


struct Command{
	int indx;
	char **newargv;
};

int main(int argc, char* argv[])        {
	counter = 1;
	savedCommands = 0;
	reading_file = false;
	signal(SIGTERM,sighandler);
	
	/*p2 accepts a file as an argument. If the file exist and can be opened
	 * we treat the '#' as a comment line and the program ignores it.
	 * */
    if (argc > 1)	{
		if((input_fd =open(argv[1],O_RDONLY,S_IREAD | S_IWUSR)) < 0)	{
			perror("opening");
			exit(EXIT_FAILURE);
		} 
		else {
			reading_file  = true;
			if (dup2(input_fd,STDIN_FILENO) < 0)	{
				perror("Dup2");
			}
			close(input_fd);
		}
	}

	/*This infinite loop is responsible for keeping the program running until the 
	 * user decides to kill it or the program finish normally
	 */
	while (true)    {
		history_command = false;
		pipeline = false;
		if (!reading_file )
			printf("%%%d%% ",counter++);
	
		/*In case of a syntax error in parse the program reprompts*/
parse:	if (parse() < 0)	{
			counter--;
			continue;
		}
		
		/*If no program exist in the command then error.*/
		if (cmd.newargv[0] == NULL)	{
			fprintf(stderr,"syntax error\n");
			continue;
		}
		
		if (PIPE)	{
			if (cmd2.newargv[0] == NULL)	{
				fprintf(stderr,"Invalid piping\n");
				continue;
			}
		}
						/**** Background Process ****************/
		if (PIPE)	{
			if (strcmp(cmd2.newargv[cmd2.indx - 1], "&") == 0)	{
	 			cmd2.newargv[--cmd2.indx] = NULL;
				background = true;		
			}
		}
		else {
			if (strcmp(cmd.newargv[cmd.indx - 1], "&") == 0)	{
					cmd.newargv[--cmd.indx] = NULL;
					background = true;		
			}	
		}
		/******** Built-ins ***************/
		/* "!!" command */
		if (cmd.newargv[0][0] == '!')	{
                if (strcmp("!!", cmd.newargv[0]) == 0)   {
                    if (savedCommands > 1)  {

                        getCommand(buffer,hist + commandLine[savedCommands -2]);
                    }
                    else  {
                        getCommand(buffer, hist);
                    }
                    goto parse;
                }
                else if (isdigit(cmd.newargv[0][1])
                         && cmd.newargv[0][2] == '\0')  {
                    int number = (int) (cmd.newargv[0][1] - '0');
                    if (savedCommands < number)   {
                        fprintf(stderr,"%d: Event not found\n",number);
                        continue;
                    }
                    else if (number > 1)  {
                        getCommand(buffer,hist + commandLine[number -2]);
                    }
                    else  {
                        getCommand(buffer, hist);               
                    }
					goto parse;
                }
                else {
                    fprintf(stderr,"Event not found\n");
                    continue;
                }
		}
		else	{
            if (savedCommands == 0)   {
                    save(buffer,hist);
            }
            else
                save(buffer,hist + commandLine[savedCommands -1]);
		}
		
		/* cd command */
        if (strcmp("cd",cmd.newargv[0]) == 0)   {
			if (cmd.indx > 2)	{
				fprintf(stderr,"cd: To many arguments\n");
				continue;
			}
			if (cmd.newargv[1] == NULL)     {	//cd wit no arguments
				if ( chdir(getenv("HOME")) < 0 )	{
					perror("chdir ");	
					continue;				
				}
            }
         	 else if (chdir(cmd.newargv[1]) < 0)  { //go to the path given
                perror("chdir");
            }
			continue;
		}
		/************ File Error Handling********/
		/*Better to catch simple errors in the parent than in the child */
		if (file_to_stdin)	{
			/*File does not exist or is not a regular file */
			if ((stat(infile, &sb) < 0) || ((sb.st_mode & S_IFMT) != S_IFREG) )	{
				perror("Error on input file");
				continue;
			}
 
		}
		
		if (stdout_to_file)	{
			/*File should not exist already */
			if (stat(outfile, &sb) == 0)	{
				fprintf(stderr,"%s :File exist\n",outfile);
				continue;
			}
		}
		else if (append_stdout_to_file)	{
			/*File should exist already */
			if (stat(outfile, &sb) != 0)	{
				fprintf(stderr,"%s :File does not exist\n",outfile);
				continue;
			}
		}
		/*File should not exist already */
		if (err_to_file)	{
			if (stat(errfile, &sb) == 0)	{
				fprintf(stderr,"%s :File exist\n",errfile);
				continue;
			}
		}
		else if (append_err_to_file)	{
			/*File should exist already */
			if (stat(errfile, &sb) != 0)	{
				fprintf(stderr,"%s :File does not exist\n",errfile);
				continue;
			}
		}
		/******* Create Process *******/
		/*Ensure that children inherits empty buffers */
		fflush(stdout);
		fflush(stderr);
		if ((pid = fork()) < 0)	{
			perror("forking");
			continue;
		}
		if (pid == 0)	{
			if (file_to_stdin)	{
				/*Open file to be read by stdin */
				if((input_fd =open(infile,O_RDONLY)) < 0)	{
					perror("input file");
					exit(EXIT_FAILURE);
				}
				if (dup2(input_fd,STDIN_FILENO) < 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
					
				}
				close(input_fd);
			}
			else		{	/*Prevent child from competing for input on stdin */
				int devnull;
				if (devnull = open("/dev/null",O_RDONLY) < 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
			}
			
			/* If there is pipeline fork another child and make a pipe between them*/
			if (PIPE)	{
				pipe(fildes);
				int pid2;
				
				fflush(stdout);
				fflush(stderr);
				if ((pid2 = fork()) < 0)	{	//error piping
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				
				else if (pid2 == 0)	{			//child
					if (dup2(fildes[0],STDIN_FILENO)<0)	{
						perror("Exiting because");
						exit(EXIT_FAILURE);
					}
					close(fildes[0]);
					close(fildes[1]);
					cmd = cmd2;				//we can reused the code on the bottom by doing this.							
				}
				else {							//parent
					if (dup2(fildes[1],STDOUT_FILENO)< 0)	{
						perror("Exiting because");
						exit(EXIT_FAILURE);
					}
					close(fildes[0]);
					close(fildes[1]);
					if (execvp(cmd.newargv[0],cmd.newargv) < 0)	{
						fprintf(stderr,"Cannot exec %s\n", cmd.newargv[0]);
						exit(EXIT_FAILURE);
					}
					
				}
			}
			
			/*Open file to be written by stdout */
			if (stdout_to_file)	{
				if((output_fd = open(outfile,O_WRONLY | O_CREAT, 
					S_IRUSR | S_IWUSR)) < 0)	{
					perror("output file");
					exit(EXIT_FAILURE);
				}
				if (dup2(output_fd,STDOUT_FILENO) < 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				close(output_fd);
			}
			/*Append an existing output file */ 
			else if (append_stdout_to_file)	{
				if((output_fd = open(outfile,O_WRONLY | O_APPEND)) < 0)	{
					perror("Appending output file ");
					exit(EXIT_FAILURE);
				}
				if (dup2(output_fd,STDOUT_FILENO)< 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				close(output_fd);
			}
			/*Open file to be written by stdout and stderr */
			if (err_to_file)	{
				if((output_fd = open(errfile,O_WRONLY | O_CREAT, 
					S_IRUSR | S_IWUSR)) < 0)	{
					perror("err file");
					exit(EXIT_FAILURE);
				}
				if (dup2(output_fd,STDOUT_FILENO) < 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				if (dup2(output_fd,STDERR_FILENO)<0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				close(output_fd);
			}
			/*Append an existing file for stderr/stdout redirection.*/
			else if (append_err_to_file)	{
				if((output_fd = open(errfile,O_WRONLY | O_APPEND)) < 0)	{
					perror("Appending err file ");
					exit(EXIT_FAILURE);
				}
				if (dup2(output_fd,STDOUT_FILENO)< 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				if (dup2(output_fd,STDERR_FILENO)< 0)	{
					perror("Exiting because");
					exit(EXIT_FAILURE);
				}
				close(output_fd);
			}
			if (execvp(cmd.newargv[0],cmd.newargv) < 0)	{
				fprintf(stderr,"Cannot exec %s\n", cmd.newargv[0]);
				exit(EXIT_FAILURE);
			}
					
		}
		/*The parent waits for the child to die. If background
		 * the parent keeps executing and prints its pid to the console.*/
		if (pid > 0)	{
			if (!background)	{ 
				for (;;)	{
					if(pid == wait(NULL))	{
						break;
					}
				}
				continue;
			}
			else {
				printf("%s [%d]\n",cmd.newargv[0],pid);
				continue;
			}
		}
	}
	killpg(getpid(),SIGTERM);
	printf("p2 terminated.\n");
	return 0;
}

int parse()	{
	/*Reset variables*/
	background = false;
	file_to_stdin = false;
	stdout_to_file =false;
	err_to_file = false;
	append_stdout_to_file = false;
	append_err_to_file = false;
	PIPE = false;
	outfile = NULL;
	infile = NULL;
	errfile = NULL;
	cmd.indx = 0;
	cmd.newargv = create();

	cmd2.indx = 0;
	cmd2.newargv = create();

	position= buffer;
	bool loop = true;

if (!history_command)    {
    while (loop)    {
    
            if ((n = getword(position)) == -1)	{
                /*Empty buffer. EOF or Logout found.Terminate process. */
                if (buffer == position)	{
                    if (!reading_file )
                        printf("p2 terminated.\n");
                    exit(EXIT_SUCCESS);
                }
                /*logout is not first word then is part of the arguments*/
                else if (strcmp(position,"logout") == 0)	{
                    /*6 characters in the logout word plus 1 more for the null character*/
                    position +=7;
                    continue;
                }
                lineLength = position - buffer;
                break;
            }
            else if (n == 0)	{
                lineLength = position - buffer;

                /*Initial position is the same as currect position (empty buffer)
                *reissue the prompt
                */
                if (buffer == position)	{
					return (-1);
                }
                break;                                                              //get out the loop and reissue prompt
            }
            switch (*(position))   {
                /*If p2 is reading from argv[1] then treat this as a metacharacter.
                 * and ignore words after the '#' metacharacter*/
                case '#':
                    if ((*(position + 1) == '\0') && reading_file )	{	//add the condition that the comments comes from a file
                        for (;;)	{
                            if ((n = getword(position)) <= 0)	{
								loop = false;
								lineLength = position - buffer;
                                break;
                            }
                        }
                    }
                    break;
				case '|':
					/*Make sure the original command line contained a clear '|' pipe and not
					* a "\|" word(which is not treated as a metacharacter).*/
					if (pipeline )	{
						position += (n+1);
						continue;
					}
            }
            position += (n + 1);
        }
}
    position = buffer;
	while((position - buffer) < lineLength)	{
        n = strlen(position);

		/*Look for metacharacters*/
		switch(*(position)) {
			case '|':
				/*Make sure the original command line contained a clear '|' pipe and not
				 * a "\|" word(which is not treated as a metacharacter).*/
				if (pipeline)	{
					PIPE = true;
					position += (n+1);
					continue;
				}
				else if (history_command)	{
					PIPE = true;
					position += (n+1);
					continue;
				}
				goto jump;

			case '<':
				/*See what the next characters is. */
				switch(*(position + 1))	{
					case '\0':
						/*Multiple input files*/
						if (infile != NULL)	{
							fprintf(stderr,"Cannot redirect output to two places at once\n");
							return (-1);
						}
						position += (n + 1);
						/*Make sure there exist a next word (filename)*/
						if ((n = strlen(position)) == 0 )	{
							fprintf(stderr,"syntax error\n");
							return (-1);
						}
						infile = position;
						/*Make sure file exist and is regular*/
						file_to_stdin = true;
						position += (n+1);
						continue;
				}
			case '>':
				/*See what the next characters is. Four possible cases ">&", ">", ">>", and ">>&"*/
				switch(*(position + 1))	{
					case '\0':
						/*Multiple output files*/
						if (outfile != NULL)	{
							fprintf(stderr,"Cannot redirect output to two places at once\n");
							return (-1);
						}
						position += (n + 1);
						/*Make sure there exist a next word (filename)*/
						if ((n = strlen(position)) == 0 )	{
							fprintf(stderr,"syntax error\n");
							return (-1);
						}
						outfile = position;
						stdout_to_file = true;
						position += (n + 1);
						continue;
					/* >> or >>& meta-characters */
					case '>':
						/*Is >>& meta-characters */
						if ((*(position + 2) == '&') && (*(position + 3) == '\0'))	{	//add some error detection
							position += (n + 1);
							if ((n = strlen(position)) == 0 )	{
                                fprintf(stderr,"syntax error\n");
                                return (-1);
                            }
							append_err_to_file = true;
							errfile = position;
							position += (n + 1);

							continue;
						}
						/*Is >> meta-characters */
						else if (*(position + 2) == '\0')	{	// add some error detection
							position += (n + 1);
							if ((n = strlen(position)) == 0 )	{
                                fprintf(stderr,"syntax error\n");
                                return (-1);
                            }
							append_stdout_to_file = true;
							outfile = position;
							position += (n + 1);
							continue;
						}

					case '&':
						/*Is not a meta-characters, then is added as a normal word*/
						if (*(position + 2) != '\0')	{
							goto jump;
						}
						if (errfile != NULL)	{
							fprintf(stderr,"Cannot redirect output to two places at once\n");
							return (-1);
						}
						position += (n + 1);
						if ((n = strlen(position)) == 0 )	{
                                fprintf(stderr,"syntax error\n");
                                return (-1);
                        }
						errfile = position;
						err_to_file = true;
						position += (n + 1);
						continue;
				}
			//	case '&':
					/*Is not a meta-character. then is added as a normal word*/
				/*	if (*(position + 1) != '\0')	{
							goto jump;
					}
					else if ((position-buffer) == (lineLength - 2))	{	//Background
						position += (n + 1);
						background = true;
						continue;
					}*/
					
	jump:		default:
				/*If no pipe encountered yet the program and its arguments
				 * are inserted in cmd. Otherwise, they are inserted in cmd2. */
				if (PIPE)	{
					cmd2.newargv[cmd2.indx++] = position;
					break;
				}
				else {
					cmd.newargv[cmd.indx++] = position;
					break;
				}


		}

		position += (n + 1);
	}
	return 0;
}


void sighandler(int signum) {}

/*Creates space for the newargv*/
char** create()	{
	char **newargv = malloc(50 * sizeof(char*));
	return newargv;
}

/*This function gets a command previously saved in history and stores
 * it in a buffer so it can be further processed in parse*/
void getCommand (char * buffer, char *hist) {
	history_command = true;
	lineLength = 1;
    int i = 0;
    while(hist[i] != '\n')  {	//transfer a character at a time.
        buffer[i] = hist[i];
        i++;
        lineLength++;
    }
    buffer[i] = '\0';
}

/*This function saves  command from buffer into history. Updates
 * the number of commands saved so far. Each word in a command
 * is stored in history from buffer and are separated by a '\0'
 * character. After the full command is store a '\n' character
 * is placed so that the program can know when the commands finishes.*/
void save (char* buffer, char *hist)    {
    int pos = 0;	//position 
    int len = 0;	//number of characters in a word.
    int i;
    if(savedCommands == 0 ) {
        commandLine[savedCommands++] = lineLength;
    }
    else{
        int prev = commandLine[savedCommands - 1];
        commandLine[savedCommands++] = lineLength + prev;
    }

    while(pos < lineLength)   {
        len = strlen(buffer + pos);
        for (i = 0; i < len; i++)  {
            hist[pos + i] = buffer[pos + i];
        }
        pos += len;
        hist[pos++] = '\0';
    }
    hist[pos - 1] = '\n';
}

