/*
* 6502 Emulator project
* Started Aug 3rd, 2022
* Using most of Dave Poo's online tutorial along with Bisqwit's NES emulator code
* https://www.youtube.com/watch?v=qJgsuQoy9bc&list=WL
* https://www.youtube.com/watch?v=y71lli8MS8s
* https://web.archive.org/web/20210803072316/http://www.obelisk.me.uk/6502/reference.html#LDA
* https://sta.c64.org/cbm64mem.html
*/

#include <iostream>
#include <vector>
#include <stdint.h>
#include <signal.h>
#include <cmath>
#include <assert.h>
#include <algorithm>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

/* Bisqwit's implementation
using u32 = uint_least32_t;
using u16 = uint_least16_t;
using u8 = uint_least8_t;
using s8 = int_least8_t;

template <unsigned bitno, unsigned nbits=1, typename T=u8>
struct RegBit
{
	T data;
	enum { mask = (1u << nbits) - 1u };
	template <typename T2>
	RegBit& operator=(T2 value)
	{
		data = (data & ~(mask << bitno)) | ((nbits > 1 ? value & mask : !!value) << bitno);
		return *this;
	}
};*/

/*
namespace INSTRUCTIONS
{
	namespace LDA_INSTRUCTIONS
	{
		static constexpr u8 INS_LDA_IMMEDIATE = 0xA9; // The Immediate Load Accumulator Instruction for the 6502 is the opcode "$A9"
		static constexpr u8 INS_LDA_ZP = 0xA5; // The Zero Page Intruction for the 6502 is the opcode "$A5"
		static constexpr u8 INS_LDA_ZPX = 0xB5; // The Zero Page,X Intruction for the 6502 is the opcode "$B5"

	}
	namespace JSR_INSTRUCTIONS
	{
		static constexpr u8 INS_JSR_ABSOLUTE = 0x20;
	}
}*/

struct
{
	static constexpr u8 INS_LDA_IMMEDIATE = 0xA9; // The Immediate Load Accumulator Instruction for the 6502 is the opcode "$A9"
	static constexpr u8 INS_LDA_ZP = 0xA5; // The Zero Page Intruction for the 6502 is the opcode "$A5"
	static constexpr u8 INS_LDA_ZPX = 0xB5; // The Zero Page,X Intruction for the 6502 is the opcode "$B5"

	static constexpr u8 INS_JSR_ABSOLUTE = 0x20;
} INSTRUCTIONS;

struct Memory
{
	static constexpr u32 MAX_MEMORY = 1024 * 64;
	u8 Data[MAX_MEMORY]{};

	void initialize() {
		std::for_each(std::begin(Data), std::end(Data), [=] (int i) mutable {Data[i] = 0; });
	}

	// Read/Get byte
	template <typename T = u32>
	inline u8 operator[](T address) const noexcept(noexcept(address > -1)) {
		assert(address < MAX_MEMORY);
		return Data[address];
	}

	// Write/Set byte
	template <typename T = u32>
	inline u8& operator[](T address) noexcept(noexcept(address > -1)) {
		assert(address < MAX_MEMORY);
		return Data[address];
	}

	// Will be used to write more than 8 bits
	void writeBytes(u32& cycles, u16 value, u32 address)
	{
		Data[address] = value & 0xff;
		Data[address + 1] = (value >> 8);
		cycles -= 2;
	}
};

struct CPU
{
	u16 PC; // Program Counter
	u8 SP, Acc, X, Y; // Registers: Stack Pointer, Accumulator, Index Register X, Index Register Y

	// Each flag is a single bit so a bit-field makes sense
	struct {
		u8 C : 1; // Carry
		u8 Z : 1; // Zero
		u8 I : 1; // Interupt
		u8 D : 1; // Decimal (Not needed but still exists)
		u8 B : 1; // Break
		u8 O : 1; // Overflow
		u8 N : 1; // Negative

	} PS; // Processor Status with the flags in a bitfield

	// Simulate a CPU power cycle
	void reset(Memory& memory) {
		PC = 0xFFFC;
		SP = 0x0100;
		Acc = X = Y = 0;

		PS.C = PS.Z = PS.I = PS.D = PS.B = PS.O = PS.N = 0;

		memory.initialize();
	}
	
	// Will fetch the next instruction byte from the program counter
	u8 fetchByte(u32& cycles, Memory& memory) {
		u8 data = memory[PC];
		PC++;
		cycles--;
		return data;
	}

	// Will read the next instruction byte from an address without incrementing it
	u8 readByte(u32& cycles, u8& address, Memory& memory) {
		u8 data = memory[address];
		cycles--;
		return data;
	}

	u16 fetchAbsoluteAddressing(u32& cycles, Memory& memory) // Used for absolute addressing where instructions contain a 16 bit address to identify target location
	{
		u16 data = memory[PC]; // Will get the first 8 bits
		PC++;

		data |= (memory[PC] << 8); // Shift upwards by 8 bits (memory[PC] * 2^8)
		PC++;
		cycles -= 2; // Decrement two cycles
		return data;
	}

	void LDASetStatus() {
		PS.Z = (Acc == 0); // Set the respective Zero Flag
		PS.N = (Acc & 0b10000000) > 0;// Set the respective Negative Flag
	}

	void execute(u32 cycles , Memory& memory)
	{
		while (cycles > 0) 
		{
			u8 instruction = fetchByte(cycles, memory);
			u8 value{};
			switch (instruction)
			{
				// The Load Accumulator loads a byte of memory into the accumulator and sets the zero and negative flags (PS) respectively
			case INSTRUCTIONS.INS_LDA_IMMEDIATE:
				{
					value = fetchByte(cycles, memory); //Load the value from fetchByte which will be the second cycle from the immediate addressing more into value
					Acc = value; // And then load value into the accumulator
					LDASetStatus();
					break;
				}
			case INSTRUCTIONS.INS_LDA_ZP:
				{
					u8 zeroPageAddress = fetchByte(cycles, memory);
					zeroPageAddress += X;
					cycles--;
					Acc = readByte(cycles, zeroPageAddress, memory);
					LDASetStatus();
					break;
				}
			case INSTRUCTIONS.INS_LDA_ZPX:
				{
					u8 zeroPageAddress = fetchByte(cycles, memory);
					Acc = readByte(cycles, zeroPageAddress, memory);
					LDASetStatus();
					break;
				}
			case INSTRUCTIONS.INS_JSR_ABSOLUTE:
				{
					u16 subRtnAddr = fetchAbsoluteAddressing(cycles, memory);
					memory.writeBytes(cycles, SP, PC - 1);
					memory[SP] = PC - 1;
					cycles--;
					PC = subRtnAddr;
					cycles--;
					SP++;
					break;
				}
				default:
				{
					std::cout << "Instruction not handled: " << instruction << '\n';
					break;
				}
			}
		}
	}
};

int main()
{
	Memory memory;
	CPU cpu;

	cpu.reset(memory);

	memory[0xFFFC] = INSTRUCTIONS.INS_LDA_ZP;
	memory[0xFFFD] = 0x42;
	memory[0x0042] = 0x84;

	cpu.execute(3, memory);

	return 0;
}
