5th oct: Started working on the Time-Travel Debugger project. I went through the project requirements and started working on the server-side implementation, beginning with Task 0.

6th oct: Completed Task 0 – Validation, which handles checking the structure of the source program. In task 0 we had to complete 4 functions and basically get the first word and second words by distinguishing between spaces and along with that we also had to keep validity checks for example func and func_end as nested functions are not allowed.   After completing Task 0, I started working on Task 1 ,will try to get it done by tommorrow and pick up the pace to finish
the project on time.


7th oct:  Today, I worked on setting up Ubuntu through WSL so I could compile and run my project on Linux. I also checked the compilation using g++ and started working on Task 1. Tomorrow, I plan to continue with the resolver and understand how it reads the instructions and handles function calls.


9th oct: Today, I continued working on the resolver and looked into how the program stores and reads information about functions. I also started looking at the functions needed for executing the program properly. Tomorrow, I plan to work on the stack and timeline parts of the project.

10th oct:  Today, I worked on the Stack class and fixed some of its functions, including isempty, depth, and snapshotinto. These functions are important for keeping track of the program's state. Tomorrow, I plan to continue with the execution functions.


11th oct: Today, I worked on the core execution functions for handling instructions like set, add, func, and call. Tomorrow, I plan to work on writing the snapshot outputs.

12th oct: Today, I worked on the snapshot functions to write the stack state and frames into session.tdbg. I also added #ifndef _WIN32 guards around sys/socket.h and unistd.h so the code stays clean for Windows compilers. I finished up the resolve.bin output logic and polished the rest of the binary reading functions.

13th oct: I double-checked all execution paths, stack operations, and output structures in server.cpp.
