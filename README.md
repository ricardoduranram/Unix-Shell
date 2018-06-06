# Purpose
Emulate the behavior of a real shell.

 The program can handle output and input redirection such as:
  	">", "<", ">&", ">>", ">>&", and "|".
  There are other meta-characters such as :
  "#" - When the program gets a file as an argument it treats this 
  		meta-characters as the starting of a comment line and thus, ignores it.
  & - If the '&' is at the end of a command it is treated as a background request.
 # Built-ins
  The program also contains some built-ins such as:
  cd - uses chdir 
  !! - Gets the previous saved command and executes it.
  !n - where n is an integer. It fetches the proper command corresponding to that
  		number in history.
 # Robustness
  p2 handles error handling without exiting or crashing providing a robust environment. 
 It also handles a variety of command combinations that gives the same result.
 
  In general p2 calls parse which in turn calls getword. Parse orders and gives meaning to
  words retrieved from getword. It also distributes the command to its proper place being some
  a program name and others, meta-characters. After parsing the the code process the command
  and executes it by forking p2 and exec with the given program from the command by the user.
  
  INPUT: A file that contains the commands to be executed.
  OUTPUT: The result of the command if any.
