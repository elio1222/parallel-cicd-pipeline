#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <unordered_map>

struct Prefix {
    std::string label;
    std::string text;
    int hash;
};

int compute_hash(const std::vector<char>& prefix) {
    int hashed = 0;
    int index = 0;
    for (int p = prefix.size() -1 ; p >= 0; p--) {
        hashed += prefix[index] * std::pow(3, p);
        std::cout << prefix[index] << "3 ^" << p << std::endl;
        index++;
    }
    return hashed;
}

Prefix make_prefix(const std::string& label, const std::string& text) {
    Prefix p;
    int hashed = compute_hash(std::vector<char>(text.begin(), text.end()));
    p.label = label;
    p.text = text;
    p.hash = hashed;
    return p;
};

std::vector<Prefix> prefixes = {
    make_prefix("Personal access token", "pat_"),
    make_prefix("Personal access token", "tok_"),
    make_prefix("GitHub token", "ghp_"),
    make_prefix("AWS access key", "AKIA"),

};

std::unordered_map<int, const Prefix*> build_index(const std::vector<Prefix>& prefixes) {
    std::unordered_map<int, const Prefix*> index;
    for (const Prefix& p : prefixes) {
        index.emplace(p.hash, &p); // no duplicates, first one rules
    }

    return index;
}

void print_prefix(std::vector<char>& prefix) {
    for (int i = 0; i < prefix.size(); i++) {
        std::cout << prefix[i] << std::endl;
    }
}

void write_findings(std::ofstream& file, int&  hash_sum) {
    file << "prefix found in <file_name>, line: <number> " << hash_sum << std::endl;
    file.close();
}

bool verify(std::vector<unsigned char>& buf, int start, int& window_size, const std::string& text) {
    std::vector<unsigned char> chars(window_size);
    for (int i = 0; i < window_size; i++) {
        chars[i] = buf[start + i];
    }
    std::string possible_match(chars.begin(), chars.end());
    
    if (possible_match == text) {
        return true;
    }

    return false;
}

int sliding_window(std::vector<unsigned char>& buf, int window_size) {
    std::cout << "sliding window implementation." << std::endl;
    std::ofstream out("findings.txt");

    std::unordered_map<int, const Prefix*> index = build_index(prefixes);


    if (buf.size() < window_size) {
        return 1;
    }

    int window_hash = 0;
    int exponent = 3;
    // first window prefix hash (size 4)
    for (int i = 0; i < window_size; i++) {
        window_hash += static_cast<int>(buf[i]) * std::pow(3, exponent);
        exponent--;
    }

    std::cout << "first window prefix hash: " << window_hash << std::endl;

    auto it = index.find(window_hash);
    if (it != index.end()) {
        const Prefix* p = it->second;
        if (verify(buf, 0, window_size, p->text)) {
            write_findings(out, window_hash);
        }
    }

    for (int i = window_size; i < buf.size(); i++) {
        window_hash = ((window_hash - static_cast<int>(buf[i - window_size]) * 27) * 3) + static_cast<int>(buf[i]);

        auto it = index.find(window_hash);
        int start = i - window_size + 1;
        if (it != index.end()) {
            const Prefix* p = it->second;
            if (verify(buf, start, window_size, p->text)) {
                write_findings(out, window_hash);
            }
        }
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

    int target_hash = compute_hash(prefix);
    std::cout << "hashed prefix: " << target_hash << std::endl;

    std::vector<unsigned char> buf = read_file();

    sliding_window(buf, 4);

    return 0;
}