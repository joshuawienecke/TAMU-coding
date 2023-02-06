#include <iostream>
#include <stdio.h>
#include <stdlib.h>


#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <string>


#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    // restore i/o later
    int in = dup(STDIN_FILENO);
    int out = dup(STDOUT_FILENO);

    // for cd
    char buf[512];
    char buf2[512];  // so that prevDur and currDir dont point to same buf
    //char buf3[512];
    getcwd(buf, 512);
    getcwd(buf2, 512);
    char* currDir = buf;
    char* prevDir = buf2;

    vector<int> pid_list;
    vector<string> command_list;

    while (true) 
    {
        string input;
        time_t timer = time(0);
        tm* localTime = localtime(&timer);
        string months[] = {"Jan", "Feb", "Mar","Apr","May", "Jun", "Jul","Aug","Sep","Oct","Nov","Dec"};
        cout << YELLOW
        << (months[localTime->tm_mon]) << " " << localTime->tm_mday << " " 
        << localTime->tm_hour << ":" << localTime->tm_min << ":" << localTime->tm_sec << " "  
        << getenv("USER") << ":" << currDir << "$" << NC << " ";

        // check for zombies
        while (!pid_list.empty()) {
            if (waitpid(pid_list.back(), NULL, WNOHANG) == -1) {
                perror("wait() error");
            }
            else {
                pid_list.pop_back();
            }
        }

        /* up/down arrows implementation
        char c = getc(stdin);
        cout << "c: " << c << endl;
        if (c == 'a') { // esc
            cout << command_list.back();
            getc(stdin);
            getc(stdin);
            c = getc(stdin);
            if (c == 'A') {
                cout << command_list.back() << endl;
            }
            else if (c == 'B') {
                cout << command_list.back() << endl;
            }
        }
        else {
            cin.putback(c);
            getline(cin, input);
            command_list.push_back(input);
        }
        */

        getline(cin, input);

        if (input.empty()) {
            continue;
        }
        if (input == "exit") {
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }


        // split input into commands
        Tokenizer token(input);

        // CD 
        if (token.commands[0]->args[0] == "cd") {
            cout << "- directory change request -\n";
            if (token.commands[0]->args.size() == 1) {
                char* path = (char*)"/home/";
                prevDir = getcwd(buf2, 512);
                chdir(path);
                currDir = getcwd(buf, 512);
            } 
            else if (token.commands[0]->args.size() == 2) {
                char* path = (char*)token.commands[0]->args[1].c_str();
                if (token.commands[0]->args[1] == (char*)"-") {
                        char* tmp = prevDir;
                        chdir(prevDir);
                        prevDir = currDir;
                        currDir = tmp;
                }
                else {
                    prevDir = getcwd(buf2, 512);
                    chdir(path);
                    currDir = getcwd(buf, 512);
                }
            }
            else {
                cout << "invald cd command\n";
                exit(2);
            }
        }
 
        // execute each command, redirect if needed
        for (size_t i = 0; i < token.commands.size(); i++) {
            int fd[2];
            pipe(fd);
            int pid = fork();
            if (pid == 0) {      
                // FILE IO
                bool hasInfile = token.commands.at(i)->hasInput();
                bool hasOutfile = token.commands.at(i)->hasOutput();
                if (hasInfile || hasOutfile) 
                {
                    if (hasOutfile) {
                        auto outName = token.commands[i]->out_file.c_str();
                        cout << "- output redirected to file -\n";
                        cout << "outfile: " << outName << endl;
                        int outfd = open(outName, O_CREAT | O_WRONLY, 0777); // creates file
                        if (outfd == -1) {
                            cout << "fifo error (output)\n";
                            exit(2);
                        }
                        dup2(outfd, STDOUT_FILENO); // overwrites stdout with the file // output now goes to file
                        close(outfd);              // dont need anymore
                    }

                    if (hasInfile) {
                        auto inName = token.commands[i]->in_file.c_str();
                        cout << "- file directed to input -\n";
                        cout << "infile: " << inName << endl;
                        int infd = open(inName, O_RDONLY, 0777);
                        if (infd == -1) {
                            cout << "fifo error (input)\n";
                            exit(2);
                        }
                        dup2(infd, STDIN_FILENO); // overwrites stdin with the file // file will go to stdin (terminal)
                        close(infd);
                    }
                }

                // EXECVP
                if (i < token.commands.size()-1) {     // dont redirect last command - stay in stdout to send to terminal
                    dup2(fd[1], STDOUT_FILENO);        // direct output to the write end of pipe 
                    close(fd[0]);                     // close read end
                }

                vector<string> cmd_str = token.commands.at(i)->args;    
                vector<char*> cmd;                                  // converts string to char*
                for (size_t j = 0; j < cmd_str.size(); j++) {       // needed for execvp
                    cmd.push_back((char*)(cmd_str.at(j).c_str()));
                }
                cmd.push_back(NULL);
                // cout << "execvp: " << cmd[0] << endl;
                execvp(cmd[0], &cmd[0]);


            }

            else {  // in parent
                if (token.commands[i]->isBackground()) {
                    cout << "adding pid: " << pid << endl; 
                    pid_list.push_back(pid);
                }
                else {
                    dup2(fd[0], STDIN_FILENO); // direct parent's input to read end of pipe
                    close(fd[1]);              // close write end
                    if (i == token.commands.size()-1) {
                        cout << "waiting..." << endl;
                        wait(0);
                    }
                    cout << "done waiting" << endl;
                }
            }
        }
        // reset for parent
        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
    }
}





















// comand pipeline - output of one cmd becomes input of next
    // use command vector to create pipeline

// output redirect - change stdout to file
    // open file for writing as fd
    // use dup2 to redirect stdin(1) to fd

// input redirect - change stdin from file
    // open file for reading as fd
    // use dup2 to redirect stdin(0) from fd
    // files for redirect are in command class

// file descriptors are keys to input or output resources (could be a file, pipe, sterr)
// they are keys to things you can read/write to
// write(1) -> writes to stdout
// read(0)  -> reads stdin

// dup2(fd1, fd2)
// the file at fd2 is overwritten by fd1


// user prompt - date/time, username, ab path of cwd
    // get login() and get cwd()          
    
    
    
    
// // print out every command token-by-token on individual lines
// // prints to cerr to avoid influencing autograder
// for (auto cmd : tknr.commands) {
//     for (auto str : cmd->args) {
//         cerr << "|" << str << "| ";
//     }
//     if (cmd->hasInput()) {
//         cerr << "in< " << cmd->in_file << " ";
//     }
//     if (cmd->hasOutput()) {
//         cerr << "out> " << cmd->out_file << " ";
//     }
//     cerr << endl;
// }