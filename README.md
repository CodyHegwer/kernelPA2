# kernelPA2
Basically implementing multi-threading in a program.

Makefile included, (however an executable 'multi-lookup' is already provided).
I didn't include a multi-lookup.h, oops.

To use the program:
./multi-lookup <# parsing threads> <# conversion threads> \<parsing log\> \<output file\> \[\<datafile\> ...\]
  
Right now the program forces you to use 1 parsing thread and 1 conversion thread.
Parsing log doesn't matter at this point, the argument won't be used by the program.
Output file must be included, could really be anything, potentially wont work if you can't access the file.
Data files are any text files holding domain names you wish to find IP addresses for. Only the first file will be used at this time.

Example input:
./multi-lookup 0 0 notused output.txt names1.txt

If less than 5 parameters are provided, the program won't execute and terminate early.
