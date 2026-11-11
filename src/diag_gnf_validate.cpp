// diag_gnf_validate -- offline GNF layout validation and corpus manifest.
//
// Reads only fixed headers and the first 1 MiB of the representatives chosen
// for the manifest. Validates the declared payload against the mip chain and
// the 64 KiB tile packing computed from the header, and reports files whose
// decode would read past the container. Nothing is decoded or executed.
//
// Usage: diag_gnf_validate <raiz-do-dump> [manifesto.csv]

#include "assets/asset_census.h"
#include "assets/gnf_format.h"
#include "assets/gnf_layout.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr size_t kHeaderBytes = 96;
constexpr size_t kHashBytes = 1024u * 1024u;

struct LayoutKey {
    uint32_t formatId = 0;
    uint32_t tileMode = 0;
    uint32_t version = 0;
    uint32_t mipLevels = 0;

    bool operator<(const LayoutKey& other) const {
        return std::tie(formatId, tileMode, version, mipLevels) <
               std::tie(other.formatId, other.tileMode, other.version,
                        other.mipLevels);
    }
};

struct Representative {
    std::string path;
    uint64_t size = 0;
    AstroPort::AssetHeaderInfo info;
    AstroPort::GnfLayout layout;
};

struct FormatStats {
    uint64_t files = 0;
    uint64_t exact = 0;
    uint64_t larger = 0;
    uint64_t smaller = 0;
    uint64_t unpaddedExact = 0;
    uint64_t unpaddedLarger = 0;
    uint64_t unpaddedSmaller = 0;
    uint64_t alignedExact = 0;
    uint64_t alignedLarger = 0;
    uint64_t alignedSmaller = 0;
    uint64_t roundedExact = 0;
    uint64_t roundedLarger = 0;
    uint64_t roundedSmaller = 0;
    uint64_t pastEof = 0;
    uint64_t tiledPastEof = 0;
};

uint64_t Fnv1a64(const uint8_t* bytes, size_t size) {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
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

const char* FormatName(uint32_t formatId) {
    const AstroPort::GnfFormatInfo format = AstroPort::LookupGnfFormat(formatId);
    return format.name;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Uso: diag_gnf_validate <raiz-do-dump> [manifesto.csv]\n";
        return 1;
    }

    const fs::path root(argv[1]);
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        std::cerr << "Diretorio invalido: " << argv[1] << "\n";
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    uint64_t gnfFiles = 0;
    uint64_t invalidHeaders = 0;
    uint64_t unmappedFormats = 0;
    uint64_t totalPayload = 0;
    uint64_t totalExpected = 0;
    uint64_t totalExpectedLinear = 0;
    std::map<uint32_t, FormatStats> byFormat;
    std::map<LayoutKey, Representative> representatives;
    std::array<uint8_t, kHeaderBytes> header{};

    const auto options = fs::directory_options::skip_permission_denied;
    for (fs::recursive_directory_iterator it(root, options, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const fs::path path = it->path();
        const uint64_t size = it->file_size(ec);
        if (ec || size < 4) {
            ec.clear();
            continue;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) continue;
        const std::streamsize wanted = static_cast<std::streamsize>(
            std::min<uint64_t>(size, kHeaderBytes));
        file.read(reinterpret_cast<char*>(header.data()), wanted);
        const size_t got = static_cast<size_t>(file.gcount());

        AstroPort::AssetHeaderInfo info;
        if (!AstroPort::ClassifyAssetHeader(header.data(), got, size, info) ||
            info.kind != AstroPort::AssetKind::Gnf) {
            continue;
        }
        ++gnfFiles;
        if (!info.valid) {
            ++invalidHeaders;
            continue;
        }

        const AstroPort::GnfFormatInfo format =
            AstroPort::LookupGnfFormat(info.formatId);
        if (!format.known) {
            ++unmappedFormats;
            continue;
        }

        const bool tiled = info.tileMode != 0;
        const AstroPort::GnfLayout layout = AstroPort::ComputeGnfLayout(
            info.width, info.height, info.mipLevels, format.blockBytes, tiled);

        FormatStats& stats = byFormat[info.formatId];
        ++stats.files;
        if (info.payloadSize == layout.tiledBytes) {
            ++stats.exact;
        } else if (info.payloadSize > layout.tiledBytes) {
            ++stats.larger;
        } else {
            ++stats.smaller;
        }
        if (info.payloadSize == layout.unpaddedBytes) {
            ++stats.unpaddedExact;
        } else if (info.payloadSize > layout.unpaddedBytes) {
            ++stats.unpaddedLarger;
        } else {
            ++stats.unpaddedSmaller;
        }
        if (info.payloadSize == layout.alignedBytes) {
            ++stats.alignedExact;
        } else if (info.payloadSize > layout.alignedBytes) {
            ++stats.alignedLarger;
        } else {
            ++stats.alignedSmaller;
        }
        if (info.payloadSize == layout.roundedTotalBytes) {
            ++stats.roundedExact;
        } else if (info.payloadSize > layout.roundedTotalBytes) {
            ++stats.roundedLarger;
        } else {
            ++stats.roundedSmaller;
        }
        if (static_cast<uint64_t>(info.dataOffset) + info.payloadSize > size) {
            ++stats.pastEof;
        }
        if (static_cast<uint64_t>(info.dataOffset) + layout.tiledBytes > size) {
            ++stats.tiledPastEof;
        }
        totalPayload += info.payloadSize;
        totalExpected += layout.tiledBytes;
        totalExpectedLinear += layout.unpaddedBytes;

        const LayoutKey key{info.formatId, info.tileMode, info.gnfVersion,
                            info.mipLevels};
        if (representatives.find(key) == representatives.end()) {
            representatives.emplace(
                key, Representative{ToUtf8(path), size, info, layout});
        }
    }

    std::ofstream manifest;
    if (argc == 3) {
        manifest.open(argv[2], std::ios::binary | std::ios::trunc);
        if (!manifest) {
            std::cerr << "Nao foi possivel criar o manifesto: " << argv[2] << "\n";
            return 1;
        }
        manifest << "path,size,format_id,format,tile_mode,version,mips,width,"
                    "height,payload,expected_linear,expected_rounded,"
                    "expected_tiled,fnv1a64_first_mib\n";
    }

    uint64_t representativeCount = 0;
    if (manifest.is_open()) {
        std::vector<uint8_t> buffer(kHashBytes);
        for (const auto& [key, rep] : representatives) {
            std::ifstream file(fs::path(rep.path), std::ios::binary);
            const uint64_t toHash = std::min<uint64_t>(rep.size, kHashBytes);
            uint64_t hash = 0;
            if (file) {
                file.read(reinterpret_cast<char*>(buffer.data()),
                          static_cast<std::streamsize>(toHash));
                hash = Fnv1a64(buffer.data(), static_cast<size_t>(file.gcount()));
            }
            manifest << CsvEscape(rep.path) << ',' << rep.size << ','
                     << rep.info.formatId << ',' << CsvEscape(FormatName(key.formatId))
                     << ',' << rep.info.tileMode << ',' << rep.info.gnfVersion << ','
                     << rep.info.mipLevels << ',' << rep.info.width << ','
                     << rep.info.height << ',' << rep.info.payloadSize << ','
                     << rep.layout.unpaddedBytes << ','
                     << rep.layout.roundedTotalBytes << ','
                     << rep.layout.tiledBytes << ",0x" << std::hex << hash
                     << std::dec << '\n';
            ++representativeCount;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "=== Validacao de layout GNF ===\n";
    std::cout << "Raiz                  : " << ToUtf8(root) << "\n";
    std::cout << "GNF encontrados       : " << gnfFiles << "\n";
    std::cout << "Cabecalhos invalidos  : " << invalidHeaders << "\n";
    std::cout << "Formatos nao mapeados : " << unmappedFormats << "\n";
    std::cout << "Payload declarado     : " << totalPayload << " bytes\n";
    std::cout << "Payload esperado      : " << totalExpected << " bytes\n";
    std::cout << "Esperado linear       : " << totalExpectedLinear << " bytes\n";
    std::cout << "Diferenca             : "
              << static_cast<int64_t>(totalPayload) -
                     static_cast<int64_t>(totalExpected)
              << " bytes\n";
    std::cout << "Tempo                 : " << std::fixed << std::setprecision(1)
              << seconds << " s\n\n";

    std::cout << "Por formato (arquivos | payload==esperado | payload>esperado | "
                 "payload<esperado | payload passa do EOF | esperado passa do EOF):\n";
    for (const auto& [formatId, stats] : byFormat) {
        std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0')
                  << formatId << std::dec << std::setfill(' ') << " "
                  << std::setw(22) << std::left << FormatName(formatId)
                  << std::right << std::setw(7) << stats.files << " | "
                  << std::setw(7) << stats.exact << " | " << std::setw(7)
                  << stats.larger << " | " << std::setw(7) << stats.smaller
                  << " | " << std::setw(7) << stats.pastEof << " | "
                  << std::setw(7) << stats.tiledPastEof << "\n";
    }

    std::cout << "\nHipotese linear (payload == soma dos mips sem padding):\n";
    for (const auto& [formatId, stats] : byFormat) {
        std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0')
                  << formatId << std::dec << std::setfill(' ') << " exact="
                  << stats.unpaddedExact << " maior=" << stats.unpaddedLarger
                  << " menor=" << stats.unpaddedSmaller << "\n";
    }

    std::cout << "\nHipotese alinhada (cada mip comeca em fronteira de 64 KiB):\n";
    for (const auto& [formatId, stats] : byFormat) {
        std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0')
                  << formatId << std::dec << std::setfill(' ') << " exact="
                  << stats.alignedExact << " maior=" << stats.alignedLarger
                  << " menor=" << stats.alignedSmaller << "\n";
    }

    std::cout << "\nHipotese total arredondado (mips contiguos, payload em 64 KiB):\n";
    for (const auto& [formatId, stats] : byFormat) {
        std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0')
                  << formatId << std::dec << std::setfill(' ') << " exact="
                  << stats.roundedExact << " maior=" << stats.roundedLarger
                  << " menor=" << stats.roundedSmaller << "\n";
    }

    if (manifest.is_open()) {
        std::cout << "\nManifesto de corpus: " << argv[2] << " ("
                  << representativeCount
                  << " representantes, um por formato/tile/versao/mips)\n";
    }
    return 0;
}
