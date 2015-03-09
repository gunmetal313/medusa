from arch import ArchConvertion
from helper import *

import re
import compiler
import ast
import string


class Avr8ArchConvertion(ArchConvertion):
    def __init__(self, arch):
        ArchConvertion.__init__(self, arch)
        self.all_mnemo = set()
        self.id_mapper = {
            'r0': 'AVR8_Reg0', 'r1': 'AVR8_Reg1', 'r2': 'AVR8_Reg2', 'r3': 'AVR8_Reg3',
            'r4': 'AVR8_Reg4', 'r5': 'AVR8_Reg5', 'r6': 'AVR8_Reg6', 'r7': 'AVR8_Reg7',
            'r8': 'AVR8_Reg8', 'r9': 'AVR8_Reg9', 'r10': 'AVR8_Reg10', 'r11': 'AVR8_Reg11',
            'r12': 'AVR8_Reg12', 'r13': 'AVR8_Reg13', 'r14': 'AVR8_Reg14', 'r15': 'AVR8_Reg15',
            'r16': 'AVR8_Reg16', 'r17': 'AVR8_Reg15'

        }

        all_instructions = self.arch['instruction']

        for insn in all_instructions:
            insn['encoding'] = compiler.ast.flatten(insn['encoding'])
            self._AVR8_VerifyInstruction(insn)
            self.all_mnemo.add(self._AVR8_GetMnemonic(insn).capitalize())


    def _AVR8_VerifyInstruction(self, insn):
        enc = insn['encoding']
        if len(enc) != 16:
            raise Exception('Invalid instruction "%s", encoding: %s, length: %d' % (
                insn['format'], insn['encoding'], len(insn['encoding'])))

    def _AVR8_GetMnemonic(self, insn):
        fmt = insn['format']
        res = ''
        for c in fmt:
            if not c in string.ascii_letters + string.digits:
                break
            res += c
        return res

    def _AVR8_GenerateMethodName(self, insn):
        mnem = self._AVR8_GetMnemonic(insn)
        mask = self._AVR8_GetMask(insn)
        value = self._AVR8_GetValue(insn)
        return 'Instruction_%s_%08x_%08x' % (mnem, mask, value)

    def _AVR8_GetSize(self, insn):
        return len(insn['encoding'])

    def _AVR8_GetValue(self, insn):
        enc = insn['encoding']
        res = 0x0
        off = 0x0
        for bit in enc[::-1]:
            if bit in [1, '(1)']:
                res |= (1 << off)
            off += 1
        return res

    def _AVR8_GetMask(self, insn):
        enc = insn['encoding']
        res = 0x0
        off = 0x0
        for bit in enc[::-1]:
            if bit in [0, 1, '(0)', '(1)']:
                res |= (1 << off)
            off += 1
        return res

    def _AVR8_GenerateMethodPrototype(self, insn, in_class=False):
        mnem = self._ARM_GetMnemonic(insn)
        meth_fmt = 'bool %s(BinaryStream const& rBinStrm, TOffset Offset, u32 Opcode, Instruction& rInsn)'
        if in_class == False:
            meth_fmt = 'bool %sArchitecture::%%s(BinaryStream const& rBinStrm, TOffset Offset, u32 Opcode, Instruction& rInsn)' % self.GetArchName()

        return meth_fmt % self._ARM_GenerateMethodName(insn)


    def GenerateHeader(self):
        res = ''
        res += 'static char const *m_Mnemonic[%#x];\n' % (len(self.all_mnemo) + 1)
        for insn in sorted(self.arch['instruction'], key=lambda a: self._AVR8_GetMnemonic(a)):
            res += self._AVR8_GenerateMethodPrototype(insn, True) + ';\n'