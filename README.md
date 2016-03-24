# threaded-appserver
A CLI-based multithreaded bank account server written in C.

This is a simple multithreaded server that handles bank account balance checks and transactions. The server works in a main thread which is operated on by multiple worker threads. The bank accounts are handled by a separate Bank.c file. After each transaction or balance check, a line is added to an output file. Up to 10 transactions can be processed at a time.

A second version of the server, appserver-coarse, is also included. This was an experiment to determine whether or not this situation called for fine-grained mutual exclusion locks or coarse-grained mutual exclusion locks. Due to the relatively low number of lock operations, the results are largely the same between the two versions, although the coarse version has a slight edge when run with a smaller number of worker threads.

###Syntax:
A makefile has been included - run make to compile the specified appserver.

Start: ./appserver <# bank accounts> <# worker threads> <output file>

Balance check: BAL <# account id>

Transaction: TRANS <#account id> <transaction amount> <#account 2 id> <transaction amount 2> ... <transaction amount 10>

Exit: END

To demonstrate the server's functionality, a perl script has been included which initializes a set number of bank accounts and then processes 300 separate transaction requests. The final balance of all accounts is then verified.

This was a 3-week lab project for my operating systems class, submitted in March 2016.
