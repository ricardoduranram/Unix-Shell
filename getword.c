
 /*
 * Ricardo Duran
 *
 * Synopsis  - Takes input from the input stream, returns -1 if we reached the 
 * end-of-file or the word is logout, and returns 0 if we encounter a new line.
 * Otherwise, the number of characters per word is returned and the word is 
 * stored in an array.
 *
 * Objective - Illustrates the basic concept of a lexical analyzer.
 *
 * INPUT: The pointer in where the characters are going to be stored.
 * OUTPUT: -1, 0, or the number of characters in the word.
 *
 * The isLogout() function is called from the getword() function. Its job
 * is to return true if the word happen to be "logout". 	 
 * 
 */

/* Include Files */
#include <stdio.h>
#include "getword.h" 
#include <stdbool.h>

bool isLogout(char buffer[]);
bool pipeline = false;



int iochar;
const int newLine = 10;                                          
const int space = 32;
char *out = "logout";
/*Carries a counter of consecutives meta characters in a single word.*/
int metaCounter[255];	

int getword( char storage[] )
{
    int counter = 0;
    metaCounter[0] = 0; 	/* In this program this array is only used to count '>'*/
    int previous = '\0';

    while(true) {
	
	/*The program does not let the user to overflows the storage*/
	if(counter > 253) {
		storage[counter] = '\0';
		return counter;
	}

	iochar = getchar();
/*
*  If EOF or a new line is not found then add the character in storage or process meta characters.
*/
	if ((iochar == EOF) || (iochar == newLine)) {

		/*
		*Calls the isLogout function and return -1 if the word
		* logout is found.
		*/
		if ((counter == 6) && (isLogout(storage))) {
			storage[counter] = '\0';
			ungetc(iochar,stdin);
			return -1;
		}
		/*
		* Return the number of characters if there are. Otherwise
		* return -1 or 0
		*/
		if (counter != 0) {
			if (iochar == newLine) {
				ungetc(iochar,stdin);
			}
			storage[counter] = '\0';
			return counter;	
		}
		else {
			if(iochar == EOF) {
				storage[0] = '\0';
				return -1;
			}
			else if(iochar == newLine) {
				storage[0] = '\0';
				return 0;
			}
		}
	}

	else {
		/* Before analysing meta characters, if the previous input was
		the '\' symbol then we treat the current character as normal
		*/
		if(previous == '\\')	{
			storage[counter] = iochar;
			previous = '\0';
			counter++;
			continue;
		}
		/* Analyse the current character and its conditions.
		In this cases the character could be a meta character, a space,
		or a normal character.	
		 */	
		switch(iochar)	{
			case '\\':
				if(previous == '\0')	{
					previous = '\\';
					break;	
				}
				
				storage[counter] = '\0';
				ungetc('\\',stdin);
				return counter;
			case '>':
				if(previous == '\0')	{
					if(counter != 0)	{
						ungetc('>',stdin);
						storage[counter] = '\0';
						return counter;
					}
				}
				if(metaCounter[0] > 1)	{
					ungetc('>',stdin);
					storage[counter] = '\0';
					return counter;
				}

				metaCounter[0]++;
				storage[counter++] = iochar;
				previous = '>';
				break;
			case '|':
				pipeline = true;
			case '<':
				if(counter != 0)	{
					storage[counter] = '\0';
					ungetc(iochar, stdin);
					return counter;
				}
				
				storage[counter++] = iochar;
				storage[counter] = '\0';
				return counter;
			case '&':
				if(previous == '>')	{
					storage[counter] = '&';
					storage[++counter] = '\0';
					return counter;
				}
				if(counter != 0)	{
					storage[counter] = '\0';
					ungetc('&',stdin);
					return counter;
				}
				
				
				storage[counter] = '&';
				storage[++counter] = '\0';
				return counter;
			case 32:
				if(counter != 0)	{
					if((counter==6) && (isLogout(storage))) {
						storage[counter] = '\0';
						return -1;	
					}
					storage[counter] = '\0';
					return counter;	
				}
				break;
			default:
				if(previous != '\0') {
					storage[counter] = '\0';
					ungetc(iochar,stdin);
					return counter;	
				}
				storage[counter++] = iochar;    
				break; 
		}
	}            
  }
}

/*
* Checks to see if the word found is "logout" and return true if it is.
*/
bool isLogout(char buffer[]) {
	int i;
	for(i = 0; i < 6; i++) {
		if(buffer[i] != out[i])
			return false;
	}
	return true;
}
