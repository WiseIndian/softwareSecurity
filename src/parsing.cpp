#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

#include <parsing.hpp>



namespace Parsing
{

    bool isPrintable(char ch) {
        return std::isprint(static_cast<unsigned char>(ch));
    }
    std::vector<size_t> get_all_occurences(std::string s, char sub) {
        std::string str;
        std::vector<size_t> positions; // holds all the positions that sub occurs within str
        size_t l = s.size();
    	for ( size_t i = 0; i < l;i++) {
    		if(s[i] == sub)  {
    			positions.push_back(i);
    		}
    	}
        return positions;
    }
    void replace_spaces(std::string& line, char rand_char) {
        std::vector<size_t> positions = get_all_occurences(line, '\"');
    	std::vector<size_t> space_positions = get_all_occurences(line, ' ');
        size_t l = positions.size();
        for (size_t i = 0; i < l;i += 2) {
            std::string sub = line.substr(positions[i], positions[i+1]);
    		for (size_t pos: space_positions) {
    			if (pos > positions[i] && positions[i+1] > pos) {
    				line[pos] = rand_char;
    			} 
    		}
        }
    }

    void put_spaces(std::string& line, char delimiter) {
        size_t l = line.size();
        for (size_t i = 0; i < l;i++) {
            if(line[i] == delimiter) {
                line[i] = space; 
            }
        }
    }

    BadPathException::BadPathException(std::string desc) {
        this->desc = desc;
    }

    BadPathException::~BadPathException()
    {
    }

    std::string BadPathException::getDesc() {
        return this->desc;
    }

    std::string get_relative_path(std::string base, std::string fp) {
        size_t found = fp.find(base);
        if (found != 0) {
            return "";
        }
        int l = base.size();
        return fp.substr(l, fp.size());
    }
    std::vector<std::string> split_string(std::string s, char delim) {
        std::vector<std::string> vec;
        replace_spaces(s, delimiter);
        std::stringstream ss(s);
        std::string token;
        while(std::getline(ss, token, delim)) {
            if (token.empty()) {
                continue;
            }
            put_spaces(token, delimiter);
            vec.push_back(token);
        }
        return vec;
    }
    std::string join_vector(std::vector<std::string> v, std::string join) {
        std::stringstream ss;
        for(size_t i = 0; i < v.size(); ++i)
        {
          if(i != 0)
            ss << join;
          ss << v[i];
        }
        std::string s = ss.str();
        return s;
    }

    std::string resolve_path(std::string base, std::string currentDir, std::string path) {
        std::vector<std::string> baseVec = split_string(base, slash);
        std::vector<std::string> currentDirVec = split_string(currentDir, slash);
        std::vector<std::string> pathVec = split_string(path, slash);
        for (std::string dir: pathVec) { 
            if (currentDirVec.empty()) {
                break;
            }
            if (dir == this_dir) {
                continue;
            }
            if (dir == parent_dir) {
                currentDirVec.pop_back();
                continue;
            }
            currentDirVec.push_back(dir);
        }
        std::string newCurrentDir = join_vector(currentDirVec, join_path);
        std::cout << "New current dir: " << newCurrentDir << std::endl;
        size_t found = newCurrentDir.find(base);
        if (found != 0) {
            std::cout << "Error thrown " << std::endl;
            BadPathException e{badPath};
            throw e;
        }
        return newCurrentDir;
    }
}
