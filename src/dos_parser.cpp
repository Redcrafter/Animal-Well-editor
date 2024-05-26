#include "dos_parser.hpp"

#include <cstring>
#include <fstream>
#include <span>
#include <vector>

// mostly adapted from the ImHex PE pattern made by WerWolv

struct DOSHeader {
    char signature[2];
    uint16_t extraPageSize;
    uint16_t numberOfPages;
    uint16_t relocations;
    uint16_t headerSizeInParagraphs;
    uint16_t minimumAllocatedParagraphs;
    uint16_t maximumAllocatedParagraphs;
    uint16_t initialSSValue;
    uint16_t initialRelativeSPValue;
    uint16_t checksum;
    uint16_t initialRelativeIPValue;
    uint16_t initialCSValue;
    uint16_t relocationsTablePointer;
    uint16_t overlayNumber;
    uint16_t reservedWords[4];
    uint16_t oemIdentifier;
    uint16_t oemInformation;
    uint16_t otherReservedWords[10];
    uint32_t coffHeaderPointer;
};
static_assert(sizeof(DOSHeader) == 64);

enum class ArchitectureType : uint16_t {
    Unknown = 0x00,
    ALPHAAXPOld = 0x183,
    ALPHAAXP = 0x184,
    ALPHAAXP64Bit = 0x284,
    AM33 = 0x1D3,
    AMD64 = 0x8664,
    ARM = 0x1C0,
    ARM64 = 0xAA64,
    ARMNT = 0x1C4,
    CLRPureMSIL = 0xC0EE,
    EBC = 0xEBC,
    I386 = 0x14C,
    I860 = 0x14D,
    IA64 = 0x200,
    LOONGARCH32 = 0x6232,
    LOONGARCH64 = 0x6264,
    M32R = 0x9041,
    MIPS16 = 0x266,
    MIPSFPU = 0x366,
    MIPSFPU16 = 0x466,
    MOTOROLA68000 = 0x268,
    POWERPC = 0x1F0,
    POWERPCFP = 0x1F1,
    POWERPC64 = 0x1F2,
    R3000 = 0x162,
    R4000 = 0x166,
    R10000 = 0x168,
    RISCV32 = 0x5032,
    RISCV64 = 0x5064,
    RISCV128 = 0x5128,
    SH3 = 0x1A2,
    SH3DSP = 0x1A3,
    SH4 = 0x1A6,
    SH5 = 0x1A8,
    THUMB = 0x1C2,
    WCEMIPSV2 = 0x169
};

struct Characteristics {
    bool baseRelocationsStripped : 1;
    bool executableImage : 1;
    bool lineNumbersStripped : 1;
    bool symbolsStripped : 1;
    bool aggressivelyTrimWorkingSet : 1;
    bool largeAddressAware : 1;
    bool : 1;  // padding
    bool bytesReversedLo : 1;
    bool machine32Bit : 1;
    bool debugInfoStripped : 1;
    bool removableRunFromSwap : 1;
    bool netRunFromSwap : 1;
    bool systemFile : 1;
    bool dll : 1;
    bool uniprocessorMachineOnly : 1;
    bool bytesReversedHi : 1;
};

enum class PEFormat : uint16_t {
    ROM = 0x107,
    PE32 = 0x10B,
    PE32Plus = 0x20B
};

enum class SubsystemType : uint16_t {
    Unknown = 0x00,
    Native = 0x01,
    WindowsGUI = 0x02,
    WindowsCUI = 0x03,
    OS2CUI = 0x05,
    POSIXCUI = 0x07,
    Windows9xNative = 0x08,
    WindowsCEGUI = 0x09,
    EFIApplication = 0x0A,
    EFIBootServiceDriver = 0x0B,
    EFIRuntimeDriver = 0x0C,
    EFIROM = 0x0D,
    Xbox = 0x0E,
    WindowsBootApplication = 0x10
};

struct DLLCharacteristics {
    bool callWhenLoaded : 1;
    bool callWhenThreadTerminates : 1;
    bool callWhenThreadStarts : 1;
    bool callWhenExiting : 1;
    bool padding : 1;
    bool highEntropyVA : 1;
    bool dynamicBase : 1;
    bool forceIntegrity : 1;
    bool nxCompatible : 1;
    bool noIsolation : 1;
    bool noSEH : 1;
    bool doNotBind : 1;
    bool appContainer : 1;
    bool isWDMDriver : 1;
    bool supportsControlFlowGuard : 1;
    bool terminalServerAware : 1;
};

struct LoaderFlags {
    bool prestartBreakpoint : 1;
    bool postloadingDebugger : 1;
    uint32_t padding : 30;
};

struct DataDirectory {
    uint32_t rva;
    uint32_t size;
};

struct OptionalHeader {
    PEFormat magic;
    uint8_t majorLinkerVersion;
    uint8_t minorLinkerVersion;
    uint32_t sizeOfCode;
    uint32_t sizeOfInitializedData;
    uint32_t sizeOfUninitializedData;
    uint32_t addressOfEntryPoint;
    uint32_t baseOfCode;

    // magic == PEFormat::PE32Plus
    uint64_t imageBase;

    uint32_t virtualSectionAlignment;
    uint32_t rawSectionAlignment;
    uint16_t majorOperatingSystemVersion;
    uint16_t minorOperatingSystemVersion;
    uint16_t majorImageVersion;
    uint16_t minorImageVersion;
    uint16_t majorSubsystemVersion;
    uint16_t minorSubsystemVersion;
    uint32_t win32VersionValue;
    uint32_t sizeOfImage;
    uint32_t sizeOfHeaders;
    uint32_t checksum;
    SubsystemType subsystem;
    DLLCharacteristics dllCharacteristics;
    /*
        if (magic == PEFormat::PE32Plus) {
                u64 sizeOfStackReserve;
                u64 sizeOfStackCommit;
                u64 sizeOfHeapReserve;
                u64 sizeOfHeapCommit;
        }
        else {
                u32 sizeOfStackReserve;
                u32 sizeOfStackCommit;
                u32 sizeOfHeapReserve;
                u32 sizeOfHeapCommit;
        }
        LoaderFlags loaderFlags;
        u32 numberOfRVAsAndSizes [[hex::spec_name("numberOfRvaAndSizes")]];
        DataDirectory directories[numberOfRVAsAndSizes];*/
};

struct COFFHeader {
    char signature[4];
    ArchitectureType architecture;
    uint16_t numberOfSections;
    uint32_t timeDateStamp;
    uint32_t pointerToSymbolTable;
    uint32_t numberOfSymbols;
    uint16_t sizeOfOptionalHeader;
    Characteristics characteristics;

    // OptionalHeader optionalHeader;
};

struct SectionFlags {
    bool : 3;
    bool doNotPad : 1;
    bool : 1;
    bool containsCode : 1;
    bool containsInitializedData : 1;
    bool containsUninitializedData : 1;
    bool linkOther : 1;
    bool linkHasInformation : 1;
    bool : 1;
    bool linkRemove : 1;
    bool linkHasCOMDAT : 1;
    bool : 1;
    bool resetSpeculativeExceptions : 1;
    bool globalPointerRelocations : 1;
    bool purgeable : 1;
    bool is16Bit : 1;
    bool locked : 1;
    bool preloaded : 1;
    bool dataAlignment : 4;
    bool linkExtendedRelocations : 1;
    bool discardable : 1;
    bool notCached : 1;
    bool notPageable : 1;
    bool shared : 1;
    bool executed : 1;
    bool read : 1;
    bool writtenOn : 1;
};

struct SectionHeader {
    char name[8];
    uint32_t virtualSize;
    uint32_t rva;
    uint32_t sizeOfRawData;
    uint32_t ptrRawData;
    uint32_t ptrRelocations;
    uint32_t ptrLineNumbers;
    uint16_t numberOfRelocations;
    uint16_t numberOfLineNumbers;
    SectionFlags characteristics;
};

SegmentData getSegmentOffsets(std::span<char> data) {
    auto ptr = (uint8_t*)data.data();

    auto dos_header = (DOSHeader*)ptr;
    auto coff_header_ptr = (COFFHeader*)(ptr + dos_header->coffHeaderPointer);
    auto coff_optional_header = (OptionalHeader*)(ptr + dos_header->coffHeaderPointer + sizeof(COFFHeader));
    auto section_ptr = (SectionHeader*)(ptr + dos_header->coffHeaderPointer + sizeof(COFFHeader) + coff_header_ptr->sizeOfOptionalHeader);
    // assert(coff_header_ptr->optionalHeader.magic == PEFormat::PE32Plus);
    auto image_base = coff_optional_header->imageBase;

    std::span<uint8_t> dat;
    std::span<uint8_t> rdata;
    uint64_t rdata_offset = -1;

    for (size_t i = 0; i < coff_header_ptr->numberOfSections; i++) {
        auto& section = section_ptr[i];
        if (std::strcmp(section.name, ".data") == 0) {
            dat = std::span(ptr + section.ptrRawData, section.sizeOfRawData);
        } else if (std::strcmp(section.name, ".rdata") == 0) {
            rdata = std::span(ptr + section.ptrRawData, section.sizeOfRawData);
            rdata_offset = section.rva + image_base;
        }
    }

    return {
        dat,
        rdata_offset,
        rdata
    };
}
