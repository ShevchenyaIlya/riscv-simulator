#include "Cpu.h"
#include "Memory.h"
#include "BaseTypes.h"

#include <optional>


// First task. Instruction per tact: 0.007611794. Info stored in info.odt file.

int main()
{
    MemoryStorage mem ;
    mem.LoadElf("program");
    UncachedMem uncachedMem = UncachedMem (mem);
    std::unique_ptr<CachedMem> memModelPtr( new CachedMem(uncachedMem));
    Cpu cpu{*memModelPtr};
    cpu.Reset(0x200);

    int32_t print_int = 0;
    while (true)
    {
        cpu.Clock();
        memModelPtr->Clock();
        std::optional<CpuToHostData> msg = cpu.GetMessage();
        if (!msg)
            continue;

        auto type = msg.value().unpacked.type;
        auto data = msg.value().unpacked.data;

        if(type == CpuToHostType::ExitCode) {
            if(data == 0) {
                fprintf(stderr, "PASSED\n");
                return 0;
            } else {
                fprintf(stderr, "FAILED: exit code = %d\n", data);
                return data;
            }
            break;
        } else if(type == CpuToHostType::PrintChar) {
            fprintf(stderr, "%c", (char)data);
        } else if(type == CpuToHostType::PrintIntLow) {
            print_int = uint32_t(data);
        } else if(type == CpuToHostType::PrintIntHigh) {
            print_int |= uint32_t(data) << 16;
            fprintf(stderr, "%d", print_int);
        }
    }
}
