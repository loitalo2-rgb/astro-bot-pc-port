// diag_census -- offline asset census (read-only).
//
// Walks a directory tree, reads only the fixed header of each file and
// classifies it with the project's own header parsers (GNF, JXMD, SELF, ELF64,
// SPIR-V). Nothing is decoded, decompressed, mapped or executed; no byte of the
// game's data leaves the process.
//
// Usage: diag_census <raiz-do-dump> [censo.csv]
//
// The optional CSV lists one row per recognized file:
//   path,size,kind,valid,detail
// where detail is format-id/size for GNF or chunk counts for JXMD.

#include "assets/asset_census.h"
#include "assets/gnf_format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr size_t kHeaderBytes = 96;

struct KindStats {
    uint64_t count = 0;
    uint64_t bytes = 0;
    uint64_t invalid = 0;
};

struct GnfStats {
    std::map<uint32_t, uint64_t> formats;
    std::map<uint32_t, uint64_t> versions;
    std::map<uint32_t, uint64_t> tileModes;
    std::map<uint32_t, uint64_t> mipCounts;
    uint64_t minWidth = UINT64_MAX, maxWidth = 0;
    uint64_t minHeight = UINT64_MAX, maxHeight = 0;
    uint64_t payloadBytes = 0;
    uint64_t bc4 = 0;
};

struct JxmdStats {
    std::map<std::string, uint64_t> firstChunks;
    std::map<uint32_t, uint64_t> strides;
    uint64_t elements = 0;
};

std::string KindName(AstroPort::AssetKind kind) {
    switch (kind) {
        case AstroPort::AssetKind::Gnf: return "GNF";
        case AstroPort::AssetKind::Jxmd: return "JXMD";
        case AstroPort::AssetKind::SelfContainer: return "SELF";
        case AstroPort::AssetKind::Elf64: return "ELF64";
        case AstroPort::AssetKind::Spirv: return "SPIRV";
        default: return "desconhecido";
    }
}

std::string FormatLabel(AstroPort::AssetKind kind,
                        const AstroPort::AssetHeaderInfo& info) {
    char buffer[128];
    if (kind == AstroPort::AssetKind::Gnf) {
        std::snprintf(buffer, sizeof(buffer), "v%u fmt=0x%04X %ux%u mips=%u tile=%u",
                      info.gnfVersion, info.formatId, info.width, info.height,
                      info.mipLevels, info.tileMode);
        return buffer;
    }
    if (kind == AstroPort::AssetKind::Jxmd) {
        std::snprintf(buffer, sizeof(buffer), "chunk=%s size=0x%X count=%u stride=%u",
                      info.magic.c_str(), info.chunkSize, info.chunkCount,
                      info.chunkStride);
        return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "%s", info.magic.c_str());
    return buffer;
}

std::string ToUtf8(const fs::path& path) {
    const auto text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::string CsvEscape(const std::string& value) {
    bool needsQuotes = false;
    for (char c : value) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Uso: diag_census <raiz-do-dump> [censo.csv]\n";
        return 1;
    }

    const fs::path root(argv[1]);
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        std::cerr << "Diretorio invalido: " << argv[1] << "\n";
        return 1;
    }

    std::ofstream csv;
    if (argc == 3) {
        csv.open(argv[2], std::ios::binary | std::ios::trunc);
        if (!csv) {
            std::cerr << "Nao foi possivel criar o CSV: " << argv[2] << "\n";
            return 1;
        }
        csv << "path,size,kind,valid,detail\n";
    }

    const auto start = std::chrono::steady_clock::now();
    std::map<AstroPort::AssetKind, KindStats> kinds;
    std::map<std::string, KindStats> byExtension;
    GnfStats gnf;
    JxmdStats jxmd;

    uint64_t totalFiles = 0;
    uint64_t totalBytes = 0;
    uint64_t unreadable = 0;
    uint64_t lastReport = 0;
    std::array<uint8_t, kHeaderBytes> header{};

    const auto options = fs::directory_options::skip_permission_denied;
    for (fs::recursive_directory_iterator it(root, options, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            std::cerr << "[Censo] Aviso ao percorrer: " << ec.message() << "\n";
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const fs::path path = it->path();
        const uint64_t size = it->file_size(ec);
        if (ec) {
            ec.clear();
            ++unreadable;
            continue;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            ++unreadable;
            continue;
        }
        const std::streamsize wanted = static_cast<std::streamsize>(
            std::min<uint64_t>(size, kHeaderBytes));
        file.read(reinterpret_cast<char*>(header.data()), wanted);
        const size_t got = static_cast<size_t>(file.gcount());

        ++totalFiles;
        totalBytes += size;

        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension.empty()) extension = "(sem extensao)";
        KindStats& extStats = byExtension[extension];
        ++extStats.count;
        extStats.bytes += size;

        AstroPort::AssetHeaderInfo info;
        if (got >= 4 && AstroPort::ClassifyAssetHeader(header.data(), got, size,
                                                       info)) {
            KindStats& stats = kinds[info.kind];
            ++stats.count;
            stats.bytes += size;
            if (!info.valid) ++stats.invalid;

            if (info.kind == AstroPort::AssetKind::Gnf) {
                if (info.valid) {
                    ++gnf.formats[info.formatId];
                    ++gnf.versions[info.gnfVersion];
                    ++gnf.tileModes[info.tileMode];
                    ++gnf.mipCounts[info.mipLevels];
                    gnf.minWidth = std::min<uint64_t>(gnf.minWidth, info.width);
                    gnf.maxWidth = std::max<uint64_t>(gnf.maxWidth, info.width);
                    gnf.minHeight = std::min<uint64_t>(gnf.minHeight, info.height);
                    gnf.maxHeight = std::max<uint64_t>(gnf.maxHeight, info.height);
                    gnf.payloadBytes += info.payloadSize;
                    if (info.bytesPerBlock == 8) ++gnf.bc4;
                }
            } else if (info.kind == AstroPort::AssetKind::Jxmd) {
                if (info.valid) {
                    ++jxmd.firstChunks[info.magic];
                    ++jxmd.strides[info.chunkStride];
                    jxmd.elements += info.chunkCount;
                }
            }

            if (csv.is_open()) {
                csv << CsvEscape(ToUtf8(path)) << ',' << size << ','
                    << KindName(info.kind) << ',' << (info.valid ? 1 : 0) << ','
                    << CsvEscape(FormatLabel(info.kind, info)) << '\n';
            }
        }

        if (totalFiles - lastReport >= 25000) {
            lastReport = totalFiles;
            const auto now = std::chrono::steady_clock::now();
            const double seconds =
                std::chrono::duration<double>(now - start).count();
            std::cerr << "[Censo] " << totalFiles << " arquivos ("
                      << (totalBytes / (1024.0 * 1024.0 * 1024.0)) << " GB) em "
                      << static_cast<int>(seconds) << " s\n";
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "=== Censo offline de assets ===\n";
    std::cout << "Raiz            : " << ToUtf8(root) << "\n";
    std::cout << "Arquivos        : " << totalFiles << "\n";
    std::cout << "Bytes lidos     : " << totalBytes << " ("
              << std::fixed << std::setprecision(2)
              << (totalBytes / (1024.0 * 1024.0 * 1024.0)) << " GiB)\n";
    std::cout << "Ilegiveis       : " << unreadable << "\n";
    std::cout << "Tempo           : " << std::setprecision(1) << seconds << " s\n\n";

    std::cout << "Containers reconhecidos:\n";
    for (const auto& [kind, stats] : kinds) {
        std::cout << "  " << std::setw(6) << std::left << KindName(kind)
                  << std::right << std::setw(9) << stats.count << " arquivos  "
                  << std::setw(10) << std::setprecision(2)
                  << (stats.bytes / (1024.0 * 1024.0 * 1024.0)) << " GiB"
                  << (stats.invalid ? "  (invalidos: " +
                                          std::to_string(stats.invalid) + ")"
                                    : "")
                  << "\n";
    }

    if (kinds.count(AstroPort::AssetKind::Gnf) && gnf.maxWidth > 0) {
        std::cout << "\nGNF (texturas):\n";
        std::cout << "  dimensoes     : " << gnf.minWidth << "x" << gnf.minHeight
                  << " ate " << gnf.maxWidth << "x" << gnf.maxHeight << "\n";
        std::cout << "  payload total : " << (gnf.payloadBytes / (1024.0 * 1024.0))
                  << " MiB\n";
        std::cout << "  BC4 (8 B/bloco): " << gnf.bc4 << "\n";
        std::cout << "  formatos      :";
        for (const auto& [id, count] : gnf.formats) {
            std::cout << " 0x" << std::hex << std::setw(4) << std::setfill('0')
                      << id << std::dec << std::setfill(' ') << "=" << count;
        }
        std::cout << "\n  versoes       :";
        for (const auto& [version, count] : gnf.versions) {
            std::cout << " v" << version << "=" << count;
        }
        uint64_t unknownFormats = 0;
        for (const auto& [id, count] : gnf.formats) {
            if (!AstroPort::LookupGnfFormat(id).known) unknownFormats += count;
        }
        if (unknownFormats != 0) {
            std::cout << "\n  NAO MAPEADOS  : " << unknownFormats
                      << " textura(s) em formatos sem decodificador:";
            for (const auto& [id, count] : gnf.formats) {
                if (!AstroPort::LookupGnfFormat(id).known) {
                    std::cout << " 0x" << std::hex << std::setw(4)
                              << std::setfill('0') << id << std::dec
                              << std::setfill(' ') << "=" << count;
                }
            }
            std::cout << " (o loader recusa esses IDs em vez de adivinhar)";
        }
        std::cout << "\n  tile modes    :";
        for (const auto& [mode, count] : gnf.tileModes) {
            std::cout << " " << mode << "=" << count;
        }
        std::cout << "\n  mips          :";
        for (const auto& [mips, count] : gnf.mipCounts) {
            std::cout << " " << mips << "=" << count;
        }
        std::cout << "\n";
    }

    if (kinds.count(AstroPort::AssetKind::Jxmd) && !jxmd.firstChunks.empty()) {
        std::cout << "\nJXMD (malhas):\n";
        std::cout << "  primeiro chunk:";
        for (const auto& [magic, count] : jxmd.firstChunks) {
            std::cout << " " << magic << "=" << count;
        }
        std::cout << "\n  strides       :";
        for (const auto& [stride, count] : jxmd.strides) {
            std::cout << " " << stride << "=" << count;
        }
        std::cout << "\n  elementos     : " << jxmd.elements << " (soma de count)\n";
    }

    std::vector<std::pair<std::string, KindStats>> extensions(byExtension.begin(),
                                                             byExtension.end());
    std::sort(extensions.begin(), extensions.end(),
              [](const auto& a, const auto& b) { return a.second.count > b.second.count; });
    std::cout << "\nExtensoes mais comuns:\n";
    const size_t limit = std::min<size_t>(extensions.size(), 15);
    for (size_t i = 0; i < limit; ++i) {
        const auto& [extension, stats] = extensions[i];
        std::cout << "  " << std::setw(12) << std::left << extension << std::right
                  << std::setw(8) << stats.count << " arquivos  " << std::setw(9)
                  << std::setprecision(2)
                  << (stats.bytes / (1024.0 * 1024.0 * 1024.0)) << " GiB\n";
    }

    if (csv.is_open()) {
        std::cout << "\nCSV: " << argv[2] << "\n";
    }
    return 0;
}
