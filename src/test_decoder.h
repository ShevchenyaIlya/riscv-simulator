//
// Created by shevchenya on 28/03/2020.
//

#ifndef RISCV_SIM_TEST_DECODER_H
#define RISCV_SIM_TEST_DECODER_H

#include "Decoder.h"

bool test_decode()
{
    bool is_all_correct = true;
    Decoder _decoder;
    Word instr;
    InstructionPtr instruction;

    instr = 1085605427; // 0100 0000 1011 0101 0000 0110 0011 0011
    instruction = _decoder.Decode(instr);

    if (instruction->_dst != 12 || instruction->_src1 != 10 || instruction->_src2 != 11 ||
        instruction->_type != IType::Alu || instruction->_aluFunc != AluFunc::Sub)
        is_all_correct = false;

    instr = 1075872179; // 0100 0000 0010 0000 1000 0001 1011 0011
    instruction = _decoder.Decode(instr);

    if (instruction->_dst != 3 || instruction->_src1 != 1 || instruction->_src2 != 2 ||
        instruction->_type != IType::Alu || instruction->_aluFunc != AluFunc::Sub)
        is_all_correct = false;

    return is_all_correct;
}

#endif //RISCV_SIM_TEST_DECODER_H
