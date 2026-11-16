#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <filesystem>
#include <tuple>
#include <algorithm>


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
        // std::cout << prefix[index] << "3 ^" << p << std::endl;
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

bool verify(const std::vector<unsigned char>& buf, int start, int window_size, const std::string& text) {
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

void print_buf(std::vector<unsigned char>& buf) {
    for (int i = 0; i < buf.size(); i++) {
        std::cout << "index: " << i << " char: " << buf[i] << " byte: " << static_cast<int>(buf[i]) << std::endl; 
    }

    std::cout << "size of file: " << buf.size() << std::endl;
}

std::vector<unsigned char> read_file(const std::filesystem::path& path) {

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "error opening file" << std::endl;
    }

    std::streamsize size = file.tellg();
    file.seekg(0);
    
    std::vector<unsigned char> buf(size);
    file.read(reinterpret_cast<char*>(buf.data()), size);

    print_buf(buf);

    return buf;
}

struct Finding {
    std::filesystem::path file;
    size_t line;     // 1-based
    size_t column;   // 1-based
    std::string label;
};

std::vector<Finding> scan_buffer(const std::filesystem::path& file, const std::vector<unsigned char>& buf, int window_size) {

    std::vector<Finding> findings;

    std::unordered_map<int, const Prefix*> index = build_index(prefixes);

    if (buf.size() < window_size) {
        std::cerr << "error, buffer size is less than window size" << std::endl;
        return findings; // empty findings
    }

    // 3^(window_size - 1) = weight of the byte leaving the window
    int high_power = 1;
    for (int i = 0; i < window_size - 1; i++) {
        high_power *= 3;
    }

    // line tracking: filled in while we scan, so no second pass over the file
    size_t line = 1;         
    size_t line_start = 0;   

    int window_hash = 0;
    int exponent = window_size - 1;

    // first window prefix hash
    for (int i = 0; i < window_size; i++) {
        window_hash += static_cast<int>(buf[i]) * std::pow(3, exponent);
        exponent--;
        if (buf[i] == '\n') {
            line++;
            line_start = i + 1;
        }
    }

    auto it = index.find(window_hash);
    if (it != index.end()) {
        const Prefix* p = it->second;
        if (verify(buf, 0, window_size, p->text)) {
            findings.push_back({file, line, 0 - line_start + 1, p->label});
        }
    }

    for (int i = window_size; i < buf.size(); i++) {
        // a prefix never contains '\n', so a match starts and ends on the same line
        if (buf[i] == '\n') {
            line++;
            line_start = i + 1;
        }

        window_hash = ((window_hash - static_cast<int>(buf[i - window_size]) * high_power) * 3) + static_cast<int>(buf[i]);

        auto it = index.find(window_hash);
        int start = i - window_size + 1;
        if (it != index.end()) {
            const Prefix* p = it->second;
            if (verify(buf, start, window_size, p->text)) {
                findings.push_back({file, line, start - line_start + 1, p->label});
            }
        }
    }

    return findings;
}

struct FileMetaData {
    std::filesystem::path path;
    std::uintmax_t size;
};

std::vector<Finding> process_file(const std::filesystem::path& file) {

    // will call scan buffer after reading the file and building the buffer
    // scan_buffer(file, buf, window_size)
    // window size will be changed in the future for it to be dynamic to check prefixes that are 3 length

    std::vector<Finding> findings;
    std::vector<unsigned char> buf = read_file(file);
    int window_size = 4;

    return scan_buffer(file, buf, window_size);

}

std::vector<FileMetaData> collect_files(const std::filesystem::path& root) {

    if (!std::filesystem::exists(root)  || !std::filesystem::is_directory(root)) {
        std::cerr << "invalid directory" << std::endl;
    }
    std::vector<FileMetaData> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
            files.push_back({entry.path(), entry.file_size()});
        }
    }
    return files;
}

std::vector<Finding> secret_scanner(const std::vector<FileMetaData>& files) {

    std::vector<std::vector<Finding>> results(files.size());
    for (size_t i = 0; i < files.size(); i++) {
        results[i] = process_file(files[i].path);
    }

    std::vector<Finding> all;
    for (auto& r : results) {
        all.insert(all.end(), r.begin(), r.end());   // append this file's findings
    }

    return all;

}

bool write_findings(const std::vector<FileMetaData>& files, const std::vector<Finding>& findings) {
    std::ofstream out("findings.txt");
    if (!out.is_open()) {
        std::cerr << "error opening findings.txt" << std::endl;
        return false;
    }

    for (const FileMetaData& file : files) {
        bool clean = true;

        for (const Finding& f : findings) {
            if (f.file == file.path) {
                out << f.file.string() << ":" << f.line << ":" << f.column << "  " << f.label << "\n";
                clean = false;
            }
        }

        if (clean) {
            out << file.path.string() << ": clean\n";
        }
    }

    return true;
}

int main() {
    std::filesystem::path dirPath = "code/";
    std::vector<FileMetaData> files = collect_files(dirPath);
    std::vector<Finding> findings = secret_scanner(files);
    write_findings(files, findings);

    return 0;
}

// TODO
// better hash (uint64, no pow, handle collisions)
// prefixes with different lengths
// remove print_buf
// sort files, skip .git and binary
// read_file: stop if open fails
// exit code 1 if secret found
// check key length + entropy
// test repos with planted secrets
// timing
// openmp over files, then chunks
// scheduler
