#include <mutex>
#include <iostream>
#include <parsing.hpp>
#include <UserReadTable.hpp>

bool hasKey(std::map<std::string,std::set<std::string>> m, std::string key) {
    if (m.find(key) != m.end()) {
        return false;
    }
    return true;
}

UserReadTable::UserReadTable() {
    //Does not require initialization
}
UserReadTable::~UserReadTable() {}
void UserReadTable::addFile(std::string filename, std::string user) {
    this->mtx.lock();
    std::vector<std::string> split_filename = Parsing::split_string(filename, Parsing::slash);
    size_t l = split_filename.size();
    for (int i = 0; i < l;i++) {
        std::string current_path = Parsing::join_vector(split_filename, Parsing::join_path);
        if (!hasKey(this->FilesBeingRead, current_path)) {
            std::set<std::string> s;
            this->FilesBeingRead[current_path] = s;
        }
        std::set<std::string> readers = this->FilesBeingRead[current_path];
        readers.insert(user);
        split_filename.pop_back();
    }
    this->mtx.unlock();
}
void UserReadTable::removeFile(std::string filename, std::string user) {
    this->mtx.lock();
    std::vector<std::string> split_filename = Parsing::split_string(filename, Parsing::slash);
    size_t l = split_filename.size();
    for (int i = 0; i < l;i++) {
        std::string current_path = Parsing::join_vector(split_filename, Parsing::join_path);
        if (!hasKey(this->FilesBeingRead, current_path)) {
            //Something is wack
            continue;
        }
        std::set<std::string> readers = this->FilesBeingRead[current_path];
        readers.erase(user);
        split_filename.pop_back();
    }
    this->mtx.unlock();
}
bool UserReadTable::isBeingRead(std::string filename) {
    if (!hasKey(this->FilesBeingRead, filename)) {
        return true;
    }
    return false; // TODO
    // std::set  readers = this->FilesBeingRead[filename];
    // return readers.find(filename) != readers.end();
}
