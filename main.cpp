#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

void print_prefix(std::vector<char>& prefix) {
    for (int i = 0; i < prefix.size(); i++) {
        std::cout << prefix[i] << std::endl;
    }
}

bool check_match(int& prefix_hash, int& key) {
    if (prefix_hash == key) {
        return true;
    } else {
        return false;
    }
}

void write_findings(std::ofstream& file, int&  hash_sum, int& key) {
    if (check_match(hash_sum, key)) {
        file << "prefix found in <file_name>, line: <number> " << hash_sum << std::endl;
        file.close();

    }
}

int hash_prefix(std::vector<char>& prefix) {
    int hashed = 0;
    int index = 0;
    for (int p = prefix.size() -1 ; p >= 0; p--) {
        hashed += prefix[index] * std::pow(3, p);
        std::cout << prefix[index] << "3 ^" << p << std::endl;
        index++;
    }
    return hashed;
}

int sliding_window(int& key, std::vector<unsigned char>& buf, int window_size) {
    std::cout << "sliding window implementation." << std::endl;
    std::ofstream out("findings.txt");

    if (buf.size() < window_size) {
        return 1;
    }

    int hash_sum = 0;
    int base = 3;
    // first window prefix hash (size 4)
    for (int i = 0; i < window_size; i++) {
        hash_sum += static_cast<int>(buf[i]) * std::pow(3, base);
        base--;
    }

    std::cout << "first window prefix hash: " << hash_sum << std::endl;

    write_findings(out, hash_sum, key);

    for (int i = window_size; i < buf.size(); i++) {
        hash_sum = ((hash_sum - static_cast<int>(buf[i - window_size]) * 27) * 3) + static_cast<int>(buf[i]);
        write_findings(out, hash_sum, key);
    }

    std::ifstream in("findings.txt");
    if (in.peek() == std::ifstream::traits_type::eof()) {
        std::cout << "file is empty" << std::endl;
        out << "nothing found." << std::endl;
    }

    out.close();
    in.close();


    return 0;
}

std::vector<unsigned char> read_file() {
    std::ifstream file("testdata/small_repo/client.py", std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std:: cerr << "error opening file." << std::endl;
    }

    std::streamsize size = file.tellg(); // file size in bytes
    file.seekg(0); // starts file to 0 again

    std::vector<unsigned char> buf(size);
    file.read(reinterpret_cast<char*>(buf.data()), size); // buf.data() is a direct pointer to the memory

    for (int i = 0; i < size; i++) {
        std::cout << "index: " << i << " byte: " << static_cast<int>(buf[i]) << " char:" << buf[i] << std::endl;        
    }

    std::cout << "size of file: " << size << std::endl;

    return buf;

}

int main() {

    int base = 'g' * 27;
    std::cout << base << std::endl;

    std::vector<char> prefix = {'g', 'h', 'p', '_'};

    int key = hash_prefix(prefix);
    std::cout << "hashed prefix: " << key << std::endl;

    std::vector<unsigned char> buf = read_file();

    sliding_window(key, buf, 4);

    return 0;
}