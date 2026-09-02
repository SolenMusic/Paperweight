#include <paperweight/pwlib.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

struct Options {
    std::filesystem::path output;
    std::filesystem::path header;
    std::string symbol{"paperweight_materials"};
    bool allowRle{true};
    std::vector<std::filesystem::path> inputs;
};

void usage()
{
    std::cerr
        << "Usage: paperweight_pack [--raw] [--cpp-header FILE] [--symbol NAME] "
           "-o LIBRARY.pwlib INPUT...\n"
        << "Each input may be a .pmat file or a folder searched recursively.\n";
}

bool validSymbol(std::string_view symbol)
{
    if (symbol.empty() ||
        (std::isalpha(static_cast<unsigned char>(symbol.front())) == 0 &&
         symbol.front() != '_')) {
        return false;
    }
    return std::all_of(symbol.begin() + 1, symbol.end(), [](char character) {
        const auto value = static_cast<unsigned char>(character);
        return std::isalnum(value) != 0 || character == '_';
    });
}

std::variant<Options, std::string> parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--raw") {
            options.allowRle = false;
        } else if (argument == "-o" || argument == "--output") {
            if (++index >= argc) return std::string{"missing output path"};
            options.output = argv[index];
        } else if (argument == "--cpp-header") {
            if (++index >= argc) return std::string{"missing C/C++ header path"};
            options.header = argv[index];
        } else if (argument == "--symbol") {
            if (++index >= argc) return std::string{"missing byte-array symbol"};
            options.symbol = argv[index];
        } else if (!argument.empty() && argument.front() == '-') {
            return "unknown option: " + std::string(argument);
        } else {
            options.inputs.emplace_back(argument);
        }
    }
    if (options.output.empty()) return std::string{"an output .pwlib path is required"};
    if (options.inputs.empty()) return std::string{"at least one input file or folder is required"};
    if (!validSymbol(options.symbol)) return std::string{"byte-array symbol is not a C identifier"};
    return options;
}

bool isPmat(const std::filesystem::path& path)
{
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
    return extension == ".pmat";
}

std::variant<std::vector<paperweight::MaterialLibrarySource>, std::string> readSources(
    const Options& options)
{
    std::vector<std::pair<std::filesystem::path, std::string>> files;
    for (const auto& input : options.inputs) {
        std::error_code error;
        if (std::filesystem::is_directory(input, error)) {
            std::vector<std::filesystem::path> found;
            for (std::filesystem::recursive_directory_iterator iterator(input, error), end;
                 !error && iterator != end; iterator.increment(error)) {
                if (iterator->is_regular_file(error) && !error && isPmat(iterator->path())) {
                    found.push_back(iterator->path());
                }
            }
            if (error) return "cannot enumerate " + input.string() + ": " + error.message();
            std::sort(found.begin(), found.end());
            for (const auto& path : found) {
                const auto relative = std::filesystem::relative(path, input, error);
                if (error) return "cannot form relative path for " + path.string();
                files.emplace_back(path, relative.generic_string());
            }
        } else if (!error && std::filesystem::is_regular_file(input, error) && isPmat(input)) {
            files.emplace_back(input, input.filename().generic_string());
        } else {
            return "input is not a readable .pmat file or folder: " + input.string();
        }
    }
    if (files.empty()) return std::string{"no .pmat files were found"};

    std::vector<paperweight::MaterialLibrarySource> sources;
    sources.reserve(files.size());
    for (const auto& [path, displayPath] : files) {
        std::ifstream input(path, std::ios::binary);
        if (!input) return "cannot read " + path.string();
        std::string text(
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{});
        if (input.bad()) return "cannot finish reading " + path.string();
        sources.push_back({displayPath, std::move(text)});
    }
    return sources;
}

bool writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

bool writeHeader(
    const std::filesystem::path& path,
    std::string_view symbol,
    std::span<const std::uint8_t> bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    std::string guard;
    guard.reserve(symbol.size() + 10);
    for (const auto character : symbol) {
        guard.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
    }
    guard += "_PWLIB_H";
    output << "#ifndef " << guard << "\n#define " << guard
           << "\n\n#include <stddef.h>\n\n"
           << "static const unsigned char " << symbol << "[] = {";
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index % 12 == 0) output << "\n    ";
        output << "0x" << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned>(bytes[index]) << std::dec;
        if (index + 1 != bytes.size()) output << ", ";
    }
    output << "\n};\n\nstatic const size_t " << symbol << "_size = sizeof(" << symbol
           << ");\n\n#endif\n";
    return output.good();
}

} // namespace

int main(int argc, char** argv)
{
    const auto parsed = parseOptions(argc, argv);
    if (const auto* message = std::get_if<std::string>(&parsed)) {
        usage();
        std::cerr << "paperweight_pack: " << *message << '\n';
        return 2;
    }
    const auto& options = std::get<Options>(parsed);
    const auto sourceResult = readSources(options);
    if (const auto* message = std::get_if<std::string>(&sourceResult)) {
        std::cerr << "paperweight_pack: " << *message << '\n';
        return 1;
    }
    const auto& sources = std::get<std::vector<paperweight::MaterialLibrarySource>>(sourceResult);
    const auto packed = paperweight::packPwlib(sources, {options.allowRle});
    if (const auto* packError = std::get_if<paperweight::PwlibError>(&packed)) {
        std::cerr << "paperweight_pack: " << packError->message << '\n';
        return 1;
    }
    const auto& bytes = std::get<std::vector<std::uint8_t>>(packed);
    const auto opened = paperweight::readPwlib(bytes);
    const auto* library = std::get_if<paperweight::PackedMaterialLibrary>(&opened);
    if (library == nullptr) {
        std::cerr << "paperweight_pack: internal verification of the pack failed\n";
        return 1;
    }
    if (!writeBytes(options.output, bytes)) {
        std::cerr << "paperweight_pack: cannot write " << options.output << '\n';
        return 1;
    }
    if (!options.header.empty() && !writeHeader(options.header, options.symbol, bytes)) {
        std::cerr << "paperweight_pack: cannot write " << options.header << '\n';
        return 1;
    }
    const auto rleCount = std::count_if(
        library->entries().begin(), library->entries().end(), [](const auto& entry) {
            return entry.storageMode == paperweight::PwlibStorageMode::rle;
        });
    std::cout << "Packed " << library->entries().size() << " material(s) into "
              << bytes.size() << " bytes (" << rleCount << " RLE, "
              << library->entries().size() - static_cast<std::size_t>(rleCount)
              << " raw).\n";
    return 0;
}
