# Cody Hegwer - Programming Assignment 2 - Multi-threading
Essentially implementing multi-threading in a program.

Makefile included, (however an executable 'multi-lookup' is already provided).
I didn't include a multi-lookup.h, oops.

To use the program:
./multi-lookup <# parsing threads> <# conversion threads> \<parsing log\> \<output file\> \[\<datafile\> ...\]
  
Provide the program with a desired amount of parsing and conversion threads. Any number here <= to 0 will cause the program to exit.
Any number greater than 10 (for each type of thread) will automatically reduce the number of threads to 10.
Parsing log must be a valid file, or new file, to record how many files each parsing thread worked on. 
Output file must be included, will terminate if you can't access the file.
Data files are any text files holding domain names you wish to find IP addresses for. The program will read up to 10 files. 

Example input:
./multi-lookup 2 4 parselog.txt output.txt names1.txt names3.txt

If less than 5 parameters are provided, the program won't execute and terminate early.
