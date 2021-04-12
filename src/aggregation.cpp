#include <string>
#include <iostream>
#include <vector>
#include <filesystem>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>

class aggregator {
public:
    explicit aggregator(std::string directory);

    [[nodiscard]] auto find_files() -> bool;
    [[nodiscard]] auto directory() const -> const std::string&;
    [[nodiscard]] auto save(std::string_view filename = "aggregate") -> bool;

    void fill();


private:
    std::map<std::int32_t, std::uint32_t> m_entries {};
    std::string m_directory {};
    double m_distance {};
    std::uint32_t m_bin_width {};
    std::uint32_t m_n {};
    std::uint32_t m_uptime {};

    std::vector<std::string> m_input_files {};
};

void print_help();

auto main(int argc, const char* argv[]) -> int
{
    if (argc != 2) {
        std::cerr << "Wrong number of arguments.\n";
        print_help();
        return 1;
    }

    std::string last_directory {};
    for(const auto& p: std::filesystem::recursive_directory_iterator(argv[1])) {
        if (!p.is_regular_file()) {
            continue;
        }
        if (p.path().extension() != ".hist") {
            continue;
        }
        auto current_directory = p.path().parent_path();
        if (last_directory == current_directory) {
            continue;
        }

        aggregator agg{current_directory};

        if (!agg.find_files()) {
            std::cerr << "Could not find any histograms in the specified directory '" << current_directory << "'\n";
            return 1;
        }

        agg.fill();
        if (!agg.save()) {
            std::cerr << "Could not save data.\n";
            return 1;
        }

        last_directory = current_directory;
    }


}


void print_help()
{
    std::cerr << "aggregation searches a directory for histograms and aggregates them into a single histogram file.\nUsage: aggregation <directory>\n";
}


aggregator::aggregator(std::string directory)
    : m_directory { std::move(directory) }
{}

auto aggregator::find_files() -> bool
{
    for (const auto& entry: std::filesystem::directory_iterator(m_directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto& path { entry.path() };
        if (path.extension() != ".meta") {
            continue;
        }
        const std::string stem { path.stem() };

        if (!std::filesystem::exists(m_directory + "/" + stem + ".hist")) {
        }
        m_input_files.emplace_back(stem);
    }
    return !m_input_files.empty();
}

auto aggregator::directory() const -> const std::string&
{
    return m_directory;
}


void aggregator::fill()
{
    for (const auto& file: m_input_files) {
        std::string filename { m_directory +"/" + file };
        std::string filename_hist { filename +  ".hist" };
        std::string filename_meta { filename +  ".meta" };
        std::ifstream input_hist { filename_hist };

        for (std::string line {}; std::getline(input_hist, line); ) {
            std::vector<std::string> cont {};
            std::istringstream iss(line);
            std::copy(std::istream_iterator<std::string>{iss},
                      std::istream_iterator<std::string>{},
                      std::back_inserter(cont));

            if (cont.size() != 2) {
                continue;
            }
            std::int32_t bin { std::stoi(cont.at(0)) };
            std::uint32_t count { static_cast<std::uint32_t>(std::stoul(cont.at(1))) };

            m_entries[bin] += count;
        }
        input_hist.close();

        std::ifstream input_meta { filename_meta };
        for (std::string line {}; std::getline(input_meta, line);) {
            std::vector<std::string> cont {};
            std::istringstream iss(line);
            std::copy(std::istream_iterator<std::string>{iss},
                      std::istream_iterator<std::string>{},
                      std::back_inserter(cont));

            if (cont.size() != 3) {
                continue;
            }
            if (cont.at(0) == "distance") {
                m_distance = std::stod(cont.at(1));
            } else if (cont.at(0) == "total") {
                m_n += std::stoul(cont.at(1));
            } else if (cont.at(0) == "uptime") {
                m_uptime += std::stoul(cont.at(1));
            } else if (cont.at(0) == "bin_width") {
                m_bin_width += std::stoul(cont.at(1));
            }
        }
        input_meta.close();
    }
}

auto aggregator::save(std::string_view filename) -> bool
{
    std::string name { m_directory + "/" + std::string{filename} };
    std::string name_hist { name + ".hist" };
    std::string name_meta { name + ".meta" };
    if (std::filesystem::exists(name_hist)) {
        std::filesystem::remove(name_hist);
    }
    if (std::filesystem::exists(name_meta)) {
        std::filesystem::remove(name_meta);
    }
    std::ofstream output_hist { name_hist };
    for (const auto& [bin, count]: m_entries) {
        output_hist<<std::to_string(bin)<<' '<<std::to_string(count)<<'\n';
    }
    output_hist.close();
    std::ofstream output_meta { name_meta };
    output_meta
            <<"bin_width "<<std::to_string(m_bin_width)<<" ns\n"
            <<"distance "<<std::to_string(m_distance)<<" m\n"
            <<"total "<<std::to_string(m_n)<<" 1\n"
            <<"uptime "<<std::to_string(m_uptime)<<" min\n"
    ;
    std::cout << m_directory << ' ' << std::to_string(m_n) << ' ' << std::to_string(m_distance) << '\n';
    output_meta.close();
    return true;
}
