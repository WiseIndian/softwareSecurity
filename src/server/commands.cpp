#include <exception>
#include <iostream>
#include <vector>

#include <server/commands.hpp>
#include <server/pathvalidate.hpp>
#include <server/systemcmd.hpp>
#include <parsing.hpp>
#include <server/commandParsing.hpp>
#include <server/conf.hpp>
#include <server/fileFetching.hpp>

namespace command
{
    //Commands that do not require authentication
    void ping(conn& conn, std::string host) {
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
        }
        try {
            std::string pingRetValue = SystemCommands::ping(host);
            if (pingRetValue.empty()) {
                std::string ret = "ping: " + host + ": Name or service not known";
                conn.send_message(ret);
            }
            else {
                conn.send_message(pingRetValue);
            }
        }
        catch(std::runtime_error& e) {
            conn.send_error(e.what());
        }
    }
    //Authentication commands
    void login(conn& conn, std::string username) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (isBeingAuthenticated || isLoggedIn) {
                conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
                conn.setUser("");
                conn.send_message(AuthenticationMessages::incorrectCommandSequence);
                return;
        }
        std::string confPath = getConfFilepath();
        bool userExists = checkIfUserExists(username, confPath);
        if (!userExists) {
            conn.send_error(AuthenticationMessages::userDoesNotExist);
            return;
        }
        conn.setUser(username);
        conn.setLoginStatus(AuthenticationMessages::authenticatingStatus);
        conn.send_message();
    }
    void pass(conn& conn, std::string pw) {
        std::string confPath = getConfFilepath();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::incorrectCommandSequence);
            return;
        }
        std::string username = conn.getUser();
        bool correctPasswordForUser = checkIfUserPasswordExists(username, pw, confPath);
        if (!correctPasswordForUser) {
            conn.send_error(AuthenticationMessages::incorrectPassword);
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.setUser("");
            return;
        }
        //The authentication was sucessful if we make it this far.
        conn.setLoginStatus(AuthenticationMessages::loggedIn);
        conn.setLogin();
        conn.send_message();
    }
    void logout(conn& conn) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        conn.clearRead();
        conn.clearLogin();
        conn.currentDir = "";
        conn.send_message();
        std::cout << AuthenticationMessages::logutMessage << '\n';
    }
    //Directory traversal commands
    void cd(conn& conn, std::string dir) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        std::string absoluteDir = conn.getCurrentDir(dir);
        //std::cout << "Absolutedir: " << absoluteDir << std::endl;
        std::string base = conn.getBase();
        std::string oldCurrentDir = conn.currentDir;
        try {
            std::string newPath = Parsing::resolve_path(base, conn.getCurrentDir("") , dir);
            std::string relativePath = Parsing::get_relative_path(base, newPath);
            if (pathvalidate::isDir(newPath)) {
                bool isBeingDeleted = conn.isBeingDeleted(newPath);
                if (isBeingDeleted) {
                    conn.send_error(Parsing::entryDoesNotExist);
                    return;
                }
                //Only update the tables if everything was successful, i.e. it is a valid directory
                conn.removeFileAsRead(oldCurrentDir);
                conn.addFileAsRead(relativePath);
                conn.currentDir = relativePath;
                conn.send_message();
            }
            else {
                if (pathvalidate::exists(newPath)) {
                    conn.send_error("This is not a directory");
                }
                else {
                    conn.send_error(relativePath + " does not exist");
                }
            }
        }
        catch(Parsing::BadPathException e) {
            conn.send_error(e.getDesc());
        }
    }
    void ls(conn& conn) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        std::string currDir = conn.getCurrentDir("");
        std::string cmd = CommandConstants::ls;
        std::string lsOutput = SystemCommands::command_with_output(cmd, conn.getCurrentDir(""));
        conn.send_message(lsOutput);
    }
    //Modify directory command
    void mkdir(conn& conn, std::string newDirName) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        std::string base = conn.getBase();
        std::string currentDir = conn.getCurrentDir("");
        std::string resolved;
        try {
            resolved = Parsing::resolve_path(base, currentDir, newDirName);
            bool isBeingDeleted = conn.isBeingDeleted(resolved);
            if (isBeingDeleted) {
                conn.send_error(Parsing::entryDoesNotExist);
                return;
            }
            conn.addFileAsRead(resolved);
            SystemCommands::mkdir(CommandConstants::mkdir, resolved);
            conn.send_message();
        }
        catch(Parsing::BadPathException e) {
            conn.send_error(e.getDesc());
        }
        conn.removeFileAsRead(resolved);
    }
    void rm(conn& conn, std::string filename) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        std::string base = conn.getBase();
        std::string currentDir = conn.getCurrentDir("");
        std::string resolved;
        try {
            resolved = Parsing::resolve_path(base, currentDir, filename);
            bool isBeingRead = conn.isBeingRead(resolved);
            if (isBeingRead) {
                conn.send_error(Parsing::entryInUse);
                return;
            }
            conn.addFileAsDeleted(resolved);
            SystemCommands::rm(CommandConstants::rm, resolved);
            conn.send_message();
        }
        catch(Parsing::BadPathException e) {
            conn.send_error(e.getDesc());
        }
        //The entry should have been deleted and is now removed from the synch data structure
        conn.removeFileAsDeleted(resolved);
    }

    //File specific commands
    void get(conn& conn, std::string filename) {
        conn = conn;    // supress compiler warnings
        filename = filename;    // supress compiler warnings
        conn.send_message();
        // TODO
    }
    void put(conn& conn, std::string filename, unsigned int fileSize) {
        conn = conn;    // supress compiler warnings
        filename = filename;    // supress compiler warnings
        fileSize = fileSize;    // supress compiler warnings
        conn.send_message();
        // TODO
    }

    //Misc commands
    void date(conn& conn) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        std::string cmd = CommandConstants::date;
        //Pass in the empty string since date is not used on a directory
        std::string dateOutput = SystemCommands::command_with_output(cmd, "");
        conn.send_message(dateOutput);

    }
    void grep(conn& conn, std::string pattern) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        std::string base = conn.getBase();
        std::string currentDir = conn.getCurrentDir("");
        std::string resolved = Parsing::resolve_path(base, currentDir, "");
        std::vector<std::string> files = FileFetching::fetch_all_files_from_dir(resolved);
        std::vector<std::string> candidateFiles;
        for (std::string file: files) {
            bool match = SystemCommands::grep(file, pattern);
            if (match) {
                candidateFiles.push_back(file);
            }
        }
        std::string ret = Parsing::join_vector(candidateFiles, Parsing::new_line);
        conn.send_message(ret);
    }
    void w(conn& conn) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        conn.send_message(conn.getAllLoggedInUsers());
    }
    void whoami(conn& conn) {
        bool isLoggedIn = conn.isLoggedIn();
        bool isBeingAuthenticated = conn.isBeingAuthenticated();
        if (!isLoggedIn || isBeingAuthenticated) {
            conn.setUser("");
            conn.setLoginStatus(AuthenticationMessages::notLoggedIn);
            conn.send_error(AuthenticationMessages::mustBeLoggedIn);
            return;
        }
        //This is really stupid but I did it for consistency
        conn.send_message(conn.getUser());
    }

    // return true on exit
    bool run_command(conn& conn, std::string commandLine) {
        try {
            std::vector<std::string> splitBySpace = Parsing::split_string(commandLine, Parsing::space);
            if (splitBySpace.empty()) {
                conn.send_error("Could not parse the command");
                return false;
            }
            std::string commandName = splitBySpace[0];
            bool hasRightArguments = Parsing::hasRightNumberOfArguments(splitBySpace);
            if (!hasRightArguments) {
                conn.send_error("Not the right argument total");
                return false;
            }
            if (commandName == "rm") {
                rm(conn, splitBySpace[1]);
            } else if (commandName == "cd") {
                cd(conn, splitBySpace[1]);
            } else if (commandName == "ls") {
                ls(conn);
            } else if (commandName == "mkdir") {
                mkdir(conn, splitBySpace[1]);
            } else if (commandName == "get") {
                get(conn, splitBySpace[1]);
            } else if (commandName == "put") {
                get(conn, splitBySpace[1]);
            } else if (commandName == "w") {
                w(conn);
            } else if (commandName == "whoami") {
                whoami(conn);
            } else if (commandName == "date") {
                date(conn);
            } else if (commandName == "ping") {
                ping(conn, splitBySpace[1]);
            } else if (commandName == "login") {
                login(conn, splitBySpace[1]);
            } else if (commandName == "pass") {
                pass(conn, splitBySpace[1]);
            } else if (commandName == "exit") {
                conn.send_message();
                return true;
            } else if (commandName == "logout") {
                logout(conn);
            } else if (commandName == "grep") {
                grep(conn, splitBySpace[1]);
            }
        }
        catch(Parsing::CommandArgumentsException e) {
            conn.send_error(e.getDesc());
        }
        catch(Parsing::CommandNotFoundException e) {
            conn.send_error(e.getDesc());
        }

        return false;
    }
} // command
