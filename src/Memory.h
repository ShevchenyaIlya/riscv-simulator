
#ifndef RISCV_SIM_DATAMEMORY_H
#define RISCV_SIM_DATAMEMORY_H

#include "Instruction.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <elf.h>
#include <cstring>
#include <vector>
#include <cassert>
#include <map>


//static constexpr size_t memSize = 4*1024*1024; // memory size in 4-byte words
static constexpr size_t memSize = 1024*1024; // memory size in 4-byte words

static constexpr size_t lineSizeBytes = 128;
static constexpr size_t lineSizeWords = lineSizeBytes / sizeof(Word);
using Line = std::array<Word, lineSizeWords>;
static constexpr size_t dataCacheBytes = 4096;
static constexpr size_t codeCacheBytes = 1024;
static Word ToWordAddr(Word ip) { return ip >> 2u; }
static Word ToLineAddr(Word ip) { return ip & ~(lineSizeBytes - 1); }
static Word ToLineOffset(Word ip) { return ToWordAddr(ip) & (lineSizeWords - 1); }

class MemoryStorage {
public:

    MemoryStorage()
    {
        _mem.resize(memSize);
    }

    bool LoadElf(const std::string &elf_filename) {
        std::ifstream elffile;
        elffile.open(elf_filename, std::ios::in | std::ios::binary);

        if (!elffile.is_open()) {
            std::cerr << "ERROR: load_elf: failed opening file \"" << elf_filename << "\"" << std::endl;
            return false;
        }

        elffile.seekg(0, elffile.end);
        size_t buf_sz = elffile.tellg();
        elffile.seekg(0, elffile.beg);

        // Read the entire file. If it doesn't fit in host memory, it won't fit in the risc-v processor
        std::vector<char> buf(buf_sz);
        elffile.read(buf.data(), buf_sz);

        if (!elffile) {
            std::cerr << "ERROR: load_elf: failed reading elf header" << std::endl;
            return false;
        }

        if (buf_sz < sizeof(Elf32_Ehdr)) {
            std::cerr << "ERROR: load_elf: file too small to be a valid elf file" << std::endl;
            return false;
        }

        // make sure the header matches elf32 or elf64
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *) buf.data();
        unsigned char* e_ident = ehdr->e_ident;
        if (e_ident[EI_MAG0] != ELFMAG0
            || e_ident[EI_MAG1] != ELFMAG1
            || e_ident[EI_MAG2] != ELFMAG2
            || e_ident[EI_MAG3] != ELFMAG3) {
            std::cerr << "ERROR: load_elf: file is not an elf file" << std::endl;
            return false;
        }

        if (e_ident[EI_CLASS] == ELFCLASS32) {
            // 32-bit ELF
            return this->LoadElfSpecific<Elf32_Ehdr, Elf32_Phdr>(buf.data(), buf_sz);
        } else if (e_ident[EI_CLASS] == ELFCLASS64) {
            // 64-bit ELF
            return this->LoadElfSpecific<Elf64_Ehdr, Elf64_Phdr>(buf.data(), buf_sz);
        } else {
            std::cerr << "ERROR: load_elf: file is neither 32-bit nor 64-bit" << std::endl;
            return false;
        }
    }

    Word Read(Word ip)
    {
        return _mem[ToWordAddr(ip)];
    }

    void Write(Word ip, Word data)
    {
        _mem[ToWordAddr(ip)] = data;
    }

private:
    template <typename Elf_Ehdr, typename Elf_Phdr>
    bool LoadElfSpecific(char *buf, size_t buf_sz) {
        // 64-bit ELF
        Elf_Ehdr *ehdr = (Elf_Ehdr*) buf;
        Elf_Phdr *phdr = (Elf_Phdr*) (buf + ehdr->e_phoff);
        if (buf_sz < ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf_Phdr)) {
            std::cerr << "ERROR: load_elf: file too small for expected number of program header tables" << std::endl;
            return false;
        }
        auto memptr = reinterpret_cast<char*>(_mem.data());
        // loop through program header tables
        for (int i = 0 ; i < ehdr->e_phnum ; i++) {
            if ((phdr[i].p_type == PT_LOAD) && (phdr[i].p_memsz > 0)) {
                if (phdr[i].p_memsz < phdr[i].p_filesz) {
                    std::cerr << "ERROR: load_elf: file size is larger than memory size" << std::endl;
                    return false;
                }
                if (phdr[i].p_filesz > 0) {
                    if (phdr[i].p_offset + phdr[i].p_filesz > buf_sz) {
                        std::cerr << "ERROR: load_elf: file section overflow" << std::endl;
                        return false;
                    }

                    // start of file section: buf + phdr[i].p_offset
                    // end of file section: buf + phdr[i].p_offset + phdr[i].p_filesz
                    // start of memory: phdr[i].p_paddr
                    std::memcpy(memptr + phdr[i].p_paddr, buf + phdr[i].p_offset, phdr[i].p_filesz);
                }
                if (phdr[i].p_memsz > phdr[i].p_filesz) {
                    // copy 0's to fill up remaining memory
                    size_t zeros_sz = phdr[i].p_memsz - phdr[i].p_filesz;
                    std::memset(memptr + phdr[i].p_paddr + phdr[i].p_filesz, 0, zeros_sz);
                }
            }
        }
        return true;
    }

    std::vector<Word> _mem;
};


class IMem
{
public:
    IMem() = default;
    virtual ~IMem() = default;
    IMem(const IMem &) = delete;
    IMem(IMem &&) = delete;

    IMem& operator=(const IMem&) = delete;
    IMem& operator=(IMem&&) = delete;

    virtual void Request(Word ip) = 0;
    virtual std::optional<Word> Response() = 0;
    virtual void Request(InstructionPtr &instr) = 0;
    virtual bool Response(InstructionPtr &instr) = 0;
    virtual void Clock() = 0;
    virtual size_t getWaitCycles() = 0;
};

class UncachedMem : public IMem
{
public:
    explicit UncachedMem(MemoryStorage& amem)
            : _mem(amem)
    {

    }


    void Request(Word ip)
    {
        if (ip != _requestedIp) {
            _requestedIp = ip;
            _waitCycles = latency;
        }
    }

    std::optional<Word> Response()
    {
        if (_waitCycles > 0)
            return std::optional<Word>();
        return _mem.Read(_requestedIp);
    }

    void Request(InstructionPtr &instr)
    {
        if (instr->_type != IType::Ld && instr->_type != IType::St)
            return;

        Request(instr->_addr);
    }

    bool Response(InstructionPtr &instr)
    {
        if (instr->_type != IType::Ld && instr->_type != IType::St)
            return true;

        if (_waitCycles != 0)
            return false;

        if (instr->_type == IType::Ld)
            instr->_data = _mem.Read(instr->_addr);
        else if (instr->_type == IType::St)
            _mem.Write(instr->_addr, instr->_data);

        return true;
    }

    Word getWordToCache(Word ip)
    {
        return _mem.Read(ip);
    }


    void writeWordToUncache(Word ip, Word data)
    {
        _mem.Write(ip, data);
    }

    void Clock()
    {
        if (_waitCycles > 0)
            --_waitCycles;
    }

    size_t getWaitCycles()
    {
        return _waitCycles;
    }


private:
    // TODO: Change latency for 120
    static constexpr size_t latency = 1;
    Word _requestedIp = 0;
    size_t _waitCycles = 0;
    MemoryStorage& _mem;
};

// TODO: Create cache for data and for code that works for 1 and 3 ticks
// TODO: Refactor to make this code mode beautiful and functionally right
// TODO: Fix memory loading latency
class CachedMem
{
public:
    CachedMem(UncachedMem& uncachedMem): _mem(uncachedMem)
    {

    }

    void Request(Word ip)
    {
        if (ip != _memoryRequestIp) {
            _memoryRequestIp = ip;
            Word lineAddr = ToLineAddr(_memoryRequestIp);
            Word offset = ToLineOffset(_memoryRequestIp);
            bool inCache = false;
            for (int i = 0; i < codeCacheBytes / lineSizeBytes; ++i) {
                if (_codeMem[i].second == lineAddr) {
                    lineAddr = i;
                    inCache = true;
                    break;
                }
            }
            _requestedIp = lineAddr;
            _requestedOffset = offset;
            if (inCache) {
                _waitCycles = codeLatency;
                _miss = false;
            } else {
                _miss = true;
                _waitCycles = failLatency;
            }
        }
    }

    std::optional<Word> Response(Word responseTime)
    {
        if (_waitCycles > 0)
            return std::optional<Word>();

        if (_miss)
        {
            Line memoryLine = { 0 };
            for (int i = 0; i < lineSizeWords; ++i) {
                memoryLine[i] = _mem.getWordToCache(_requestedIp + 4 * i);
            }

            // TODO: Change for function that return index in vector
            Word latestUsage = *min_element(_lastCodeUsage.begin(), _lastCodeUsage.end());
            Word index = 0;
            for (int j = 0; j < codeCacheBytes / lineSizeWords; ++j) {
                if (_lastCodeUsage[j] == latestUsage)
                {
                    index = j;
                    break;
                }
            }
            //Word index = *std::find(_lastCodeUsage.begin(), _lastCodeUsage.end(), latestUsage);

            if (latestUsage != 0)
            {
                for (int i = 0; i < lineSizeWords; ++i) {
                    _mem.writeWordToUncache(_codeMem[index].second + 4 * i, _codeMem[index].first[i]);
                }
            }
            _codeMem[index] = std::pair<Line, Word>(memoryLine, _requestedIp);
            _lastCodeUsage[index] = responseTime;

            return memoryLine[_requestedOffset];
        } else {
            _lastCodeUsage[_requestedIp] = responseTime;
            return _codeMem[_requestedIp].first[_requestedOffset];
        }
    }

    void Request(InstructionPtr &instr)
    {
        if (instr->_type != IType::Ld && instr->_type != IType::St)
            return;

        Word lineAddr = ToLineAddr(instr->_addr);
        Word offset = ToLineOffset(instr->_addr);
        bool inCache = false;
        for (int i = 0; i < dataCacheBytes / lineSizeBytes; ++i) {
            if (_dataMem[i].second == lineAddr) {
                lineAddr = i;
                inCache = true;
                break;
            }
        }
        _requestedIp = lineAddr;
        _requestedOffset = offset;
        if (inCache) {
            _waitCycles = dataLatency;
            _miss = false;
        } else {
            _miss = true;
            _waitCycles = failLatency;
        }
    }

    bool Response(InstructionPtr &instr, Word responseTime)
    {
        if (instr->_type != IType::Ld && instr->_type != IType::St)
            return true;

        if (_waitCycles != 0)
            return false;

        if (_miss)
        {
            Line memoryLine = { 0 };

            if (instr->_type == IType::St)
                _mem.writeWordToUncache(instr->_addr, instr->_data);

            for (int i = 0; i < lineSizeWords; ++i) {
                memoryLine[i] = _mem.getWordToCache(_requestedIp + 4 * i);
            }

            // TODO: Change for function that return index in vector
            Word latestUsage = *min_element(_lastDataUsage.begin(), _lastDataUsage.end());
            Word index = 0;
            for (int j = 0; j < dataCacheBytes / lineSizeWords; ++j) {
                if (_lastDataUsage[j] == latestUsage)
                {
                    index = j;
                    break;
                }
            }
            //Word index = *std::find(_lastCodeUsage.begin(), _lastCodeUsage.end(), latestUsage);

            if (latestUsage != 0)
            {
                for (int i = 0; i < lineSizeWords; ++i) {
                    _mem.writeWordToUncache(_dataMem[index].second + 4 * i, _dataMem[index].first[i]);
                }
            }
            _dataMem[index] = std::pair<Line, Word>(memoryLine, _requestedIp);
            _lastDataUsage[index] = responseTime;

            if (instr->_type == IType::Ld)
                instr->_data = memoryLine[_requestedOffset];
        }
        else
        {
            _lastDataUsage[_requestedIp] = responseTime;
            if (instr->_type == IType::Ld)
                instr->_data = _dataMem[_requestedIp].first[_requestedOffset];
            else if (instr->_type == IType::St)
                _dataMem[_requestedIp].first[_requestedOffset] = instr->_data;
        }

        return true;
    }

    void Clock()
    {
        if (_waitCycles > 0)
            --_waitCycles;
    }

    size_t getWaitCycles()
    {
        return _waitCycles;
    }
private:
    // TODO: Replace for 152
    static constexpr size_t failLatency = 152;
    static constexpr size_t codeLatency = 1;
    static constexpr size_t dataLatency = 3;

    Word _memoryRequestIp = 0;
    Word _requestedIp = 0;
    Word _requestedOffset = 0;
    size_t _waitCycles = 0;
    bool _miss = false;
    std::vector<std::pair<Line, Word>> _dataMem = std::vector<std::pair<Line, Word>>(dataCacheBytes/lineSizeBytes);;
    std::vector<std::pair<Line, Word>> _codeMem = std::vector<std::pair<Line, Word>>(codeCacheBytes/lineSizeBytes);;
    std::vector<Word> _lastDataUsage = std::vector<Word> (dataCacheBytes/lineSizeBytes);
    std::vector<Word> _lastCodeUsage = std::vector<Word> (codeCacheBytes/lineSizeBytes);
    UncachedMem& _mem;
};

#endif //RISCV_SIM_DATAMEMORY_H
