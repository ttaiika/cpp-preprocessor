#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories,
    const path& name_file, const unsigned int number_line) {
    bool found = false;
    for (auto& file : include_directories) {
        if (!exists(path(file / name_file))) {
            continue;
        }
        found = true;
        Preprocess(file / name_file, out_file, include_directories);
        break;
    }
    if (!found) {
        cout << "unknown include file "s << name_file.string() << " at file "s << in_file.string()
            << " at line "s << number_line << endl;
    }
    return found;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    static regex include_file(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");    //файл
    smatch m_file;
    static regex include_library(R"/(\s*#\s*include\s*<([^>]*)>\s*)/"); //библиотека
    smatch m_library;

    ifstream reader(in_file);
    ofstream writer(out_file, ios::out | ios::app);
    string line;
    unsigned int number_line = 0;
    while (getline(reader, line)) {
        ++number_line;
        ifstream reader_file;
        if (regex_match(line, m_file, include_file)) {
            path name_file = string(m_file[1]);
            path parent_path = in_file.parent_path();
            path path_found = parent_path / name_file;
            reader_file.open(path_found);
            if (reader_file.is_open()) {
                Preprocess(path_found, out_file, include_directories);
                reader_file.close();
            }
            else {
                bool found = Preprocess(in_file, out_file, include_directories, name_file, number_line);
                if (!found) return false;
            }
        }
        else if (regex_match(line, m_library, include_library)) {
            path name_file = string(m_library[1]);
            bool found = Preprocess(in_file, out_file, include_directories, name_file, number_line);
            if (!found) return false;
        }
        else {
            writer << line << endl;
        }
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"sv;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"sv;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"sv;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"sv;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"sv;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"sv;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"sv;
    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}