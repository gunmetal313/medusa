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


    def _AVR8_VerifyInstruction(self, insn):
        enc = insn['encoding']
        if len(enc) != 16:
            raise Exception('Invalid instruction "%s", encoding: %s, length: %d' % (
            insn['format'], insn['encoding'], len(insn['encoding'])))

    def _AVR8_GetMnemonic(self, insn):
        fmt = insn['format']
        res = ''
        for c in fmt:
            if not c in string.ascii_letters+string.digits:
                break
            res += c
        return res