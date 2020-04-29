//
// Created by shevchenya on 28/03/2020.
//

#ifndef RISCV_SIM_TEST_EXECUTOR_H
#define RISCV_SIM_TEST_EXECUTOR_H

#include "Decoder.h"
#include "Executor.h"

void test_subtraction(InstructionPtr& instr, Executor& executor, bool& is_all_correct, int first, int second, Word result);

bool test_executor()
{
    bool is_all_correct = true;
    Decoder _decoder;
    Executor executor;
    Word instr;
    InstructionPtr instruction;

    instr = 1085605427; // 0100 0000 1011 0101 0000 0110 0011 0011
    instruction = _decoder.Decode(instr);

    // Tests
    test_subtraction(instruction, executor, is_all_correct, 124, 24, 100);
    test_subtraction(instruction, executor, is_all_correct, 124, 124, 0);
    test_subtraction(instruction, executor, is_all_correct, 10, 11, 4294967295);
    test_subtraction(instruction, executor, is_all_correct, 10, 5, 0);

    return is_all_correct;
}

void test_subtraction(InstructionPtr& instr, Executor& executor, bool& is_all_correct, int first, int second, Word result)
{
    instr->_src1Val = first;
    instr->_src2Val = second;
    executor.Execute(instr, 512);
    if (instr->_data != result)
        is_all_correct = false;
}
#endif //RISCV_SIM_TEST_EXECUTOR_H
