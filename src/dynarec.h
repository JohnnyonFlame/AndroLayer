#ifndef _DYNAREC_H_
#define _DYNAREC_H_

#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>

#include "dynarmic/interface/A64/a64.h"
#include "dynarmic/interface/A64/config.h"
#include "dynarmic/interface/exclusive_monitor.h"

#define DYNAREC_MEMBLK_SIZE (32 * 1024 * 1024)

class so_env final : public Dynarmic::A64::UserCallbacks {
public:
	std::uint64_t ticks_left = 0;
	std::uint8_t *memory = nullptr;
	std::uint64_t mem_size = 0;
	Dynarmic::A64::Jit *parent = nullptr;

	std::optional<std::uint32_t> MemoryReadCode(std::uint64_t vaddr);

	std::uint64_t getCyclesForInstruction(bool isThumb, std::uint32_t instruction) {
		(void)isThumb;
		(void)instruction;

		return 1;
	}

	std::uint8_t MemoryRead8(std::uint64_t vaddr) override {
		return *reinterpret_cast<uint8_t*>(vaddr);
	}

	std::uint16_t MemoryRead16(std::uint64_t vaddr) override {
		return *reinterpret_cast<uint16_t*>(vaddr);
	}

	std::uint32_t MemoryRead32(std::uint64_t vaddr) override {
		return *reinterpret_cast<uint32_t*>(vaddr);
	}

	std::uint64_t MemoryRead64(std::uint64_t vaddr) override {
		return *reinterpret_cast<uint64_t*>(vaddr);
	}
	
	Dynarmic::A64::Vector MemoryRead128(std::uint64_t vaddr) override {
		return Dynarmic::A64::Vector{
			*reinterpret_cast<uint64_t*>(vaddr),
			*reinterpret_cast<uint64_t*>(vaddr + 8)
		};
	}

	void MemoryWrite8(std::uint64_t vaddr, std::uint8_t value) override {
		*reinterpret_cast<uint8_t*>(vaddr) = value;
	}

	void MemoryWrite16(std::uint64_t vaddr, std::uint16_t value) override {
		*reinterpret_cast<uint16_t*>(vaddr) = value;
	}

	void MemoryWrite32(std::uint64_t vaddr, std::uint32_t value) override {
		*reinterpret_cast<uint32_t*>(vaddr) = value;
	}

	void MemoryWrite64(std::uint64_t vaddr, std::uint64_t value) override {
		*reinterpret_cast<uint64_t*>(vaddr) = value;
	}
	
	void MemoryWrite128(std::uint64_t vaddr, Dynarmic::A64::Vector value) override {
		*reinterpret_cast<uint64_t*>(vaddr) = value[0];
		*reinterpret_cast<uint64_t*>(vaddr + 8) = value[1];
	}
	
	bool MemoryWriteExclusive8(std::uint64_t vaddr, std::uint8_t value, [[maybe_unused]] std::uint8_t expected) override {
        MemoryWrite8(vaddr, value);
        return true;
    }
    bool MemoryWriteExclusive16(std::uint64_t vaddr, std::uint16_t value, [[maybe_unused]] std::uint16_t expected) override {
        MemoryWrite16(vaddr, value);
        return true;
    }
    bool MemoryWriteExclusive32(std::uint64_t vaddr, std::uint32_t value, [[maybe_unused]] std::uint32_t expected) override {
        MemoryWrite32(vaddr, value);
        return true;
    }
    bool MemoryWriteExclusive64(std::uint64_t vaddr, std::uint64_t value, [[maybe_unused]] std::uint64_t expected) override {
        MemoryWrite64(vaddr, value);
        return true;
    }
    bool MemoryWriteExclusive128(std::uint64_t vaddr, Dynarmic::A64::Vector value, [[maybe_unused]] Dynarmic::A64::Vector expected) override {
        MemoryWrite128(vaddr, value);
        return true;
    }

	void InterpreterFallback(std::uint64_t pc, size_t num_instructions) override {
		printf("Interpreter fallback: 0x%llx\n", pc);
	}

	void CallSVC(std::uint32_t swi) override;

	void ExceptionRaised(std::uint64_t pc, Dynarmic::A64::Exception exception) override {
		printf("Exception raised: 0x%llx\n", pc);
	}

	void AddTicks(std::uint64_t ticks) override {
		if (ticks > ticks_left) {
			ticks_left = 0;
			return;
		}
		ticks_left -= ticks;
	}

	std::uint64_t GetTicksRemaining() override {
		return ticks_left;
	}
	
	std::uint64_t GetCNTPCT() override {
		return 0x10000000000 - ticks_left;
	}
};

extern so_env so_dynarec_env;
extern Dynarmic::A64::Jit *so_dynarec;
extern Dynarmic::A64::UserConfig user_config;
extern Dynarmic::ExclusiveMonitor *so_monitor;

#endif
