//
// Created by Ricardo Jacobi on 18/11/22.
//

#include "globals.h"
#include "riscv.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#define MEM_SIZE 4096


int32_t mem[MEM_SIZE];
int DATA_SEGMENT_START;
//
// Initial values for registers
//

void init() {
    pc = 0;
    ri = 0;
    stop_prg = false;
    sp = 0x3ffc;
    gp = 0x1800;
    build_dic();
}

void fetch() {
    ri = lw(pc, 0);
    pc = pc + 4;
}

void decode() {
    int32_t tmp;

    opcode = ri & 0x7F;
    rs2 = (ri >> 20) & 0x1F;
    rs1 = (ri >> 15) & 0x1F;
    rd = (ri >> 7) & 0x1F;
    shamt = (ri >> 20) & 0x1F;
    funct3 = (ri >> 12) & 0x7;
    funct7 = (ri >> 25);
    imm12_i = ((int32_t) ri) >> 20;
    tmp = get_field(ri, 7, 0x1f);
    imm12_s = set_field(imm12_i, 0, 0x1f, tmp);
    imm13 = imm12_s;
    imm13 = set_bit(imm13, 11, imm12_s & 1);
    imm13 = imm13 & ~1;
    imm20_u = ri & (~0xFFF);
// mais aborrecido...
    imm21 = (int32_t) ri >> 11;            // estende sinal
    tmp = get_field(ri, 12, 0xFF);        // le campo 19:12
    imm21 = set_field(imm21, 12, 0xFF, tmp);    // escreve campo em imm21
    tmp = get_bit(ri, 20);                // le o bit 11 em ri(20)
    imm21 = set_bit(imm21, 11, tmp);            // posiciona bit 11
    tmp = get_field(ri, 21, 0x3FF);
    imm21 = set_field(imm21, 1, 0x3FF, tmp);
    imm21 = imm21 & ~1;                    // zero bit 0
    ins_code = get_instr_code(opcode, funct3, funct7);
}

int32_t lw(uint32_t address, int32_t kte){

    uint32_t endereco;
    int y;
    endereco = address + kte;
    if((endereco%4)==0){
        y = (endereco/4);
        return mem[y];
    }else{
        printf("ERROR");
        return 0;
    }

}

int32_t lb(uint32_t address, int32_t kte){

    int8_t *x;
    x = (int8_t *)mem;
    return ((int32_t)(*(x + (address + kte))));

}

int32_t lbu(uint32_t address, int32_t kte){

    int8_t *x;
    x = (int8_t *)mem;
    return ((int32_t)((*(x + (address + kte))) & 0x000000ff));
}

void sw(uint32_t address, int32_t kte, int32_t dado){

    uint32_t endereco = address + kte;
    if((endereco%4)==0){

        endereco = endereco/4;
        mem[endereco] = dado;

    }else{
        printf("ERROR");
    }

}

void sb(uint32_t address, int32_t kte, int8_t dado){

    int8_t* mem_byte = (int8_t*)mem;
    uint32_t endereco = (address + kte);
    mem_byte[endereco] = dado;

}







void debug_decode() {
    int32_t tmp = get_imm32(get_i_format(opcode, funct3, funct7));

    printf("Inst = %s\nPC = %x \nimm32 = %d / %x\nrs1 = %s : rs2 = %s : rd = %s\n",
           instr_str[ins_code],
           pc-4,
           tmp, tmp, reg_str[rs1], reg_str[rs2], reg_str[rd]
           );
}

void dump_breg() {
    for (int i = 0; i < 32; i++) {
        printf("BREG[%s] = \t%8d \t\t\t%8x\n", instr_str[i], regs[i], regs[i]);
    }
}

void dump_asm(int start_ins, int end_ins) {
    struct instruction_context_st ic;

    for (int i = start_ins; i <= end_ins; i++) {
        fetch();
        //cout  << "mem[" << i << "] =  " << x << endl;
        decode();
        debug_decode();
    }
}

void dump_mem(int start_byte, int end_byte, char format) {
    switch (format) {
        case 'h':
        case 'H':
            for (uint32_t i = start_byte; i <= end_byte; i += 4)
                printf("%08x \t%08x\n", i, lw(i, 0));
            break;
        case 'd':
        case 'D':
            for (int i = start_byte; i <= end_byte; i += 4)
                printf("%x \t%d\n", i, lw(i, 0));
            break;
        default:
            break;
    }
}

int load_mem(const char *fn, int start) {
    FILE *fptr;
    int *m_ptr = mem + (start >> 2);
    int size = 0;

    fptr = fopen(fn, "rb");
    if (!fptr) {
        printf("Arquivo nao encontrado!");
        return -1;
    } else {
        while (!feof(fptr)) {
            fread(m_ptr, 4, 1, fptr);
            m_ptr++;
            size++;
        }
        fclose(fptr);
    }
    return size;
}

int32_t get_imm32(enum FORMATS iformat) {
    switch (iformat) {
        case RType:     return 0;
        case IType:     return imm12_i;
        case SType:     return imm12_i;
        case SBType:    return imm13;
        case UType:     return imm20_u;
        case UJType:    return imm21;
        default:        return 0;
    }
}

enum FORMATS get_i_format(uint32_t opcode, uint32_t func3, uint32_t func7) {
    switch (opcode) {
        case 0x33 :
            return RType;
        case 0x03:
        case 0x13:
        case 0x67:
        case 0x73:
            return IType;
        case 0x23 :
            return SType;
        case 0x63 :
            return SBType;
        case 0x37 :
            return UType;
        case 0x6F:
        case 0x17 :
            return UJType;
        case 0x00:
            if (func3 == 0 && func7 == 0)
                return NOPType;
            else
                return NullFormat;
        default:
            printf("Undefined Format");
            return NullFormat;
            break;
    }
}

enum INSTRUCTIONS get_instr_code(uint32_t opcode, uint32_t funct3, uint32_t funct7) {
    switch (opcode) {
        case LUI:
            return I_lui;
        case AUIPC:
            return I_auipc;
        case BType:
            switch (funct3) {
                case BEQ3:
                    return I_beq;
                case BNE3:
                    return I_bne;
                case BLT3:
                    return I_blt;
                case BGE3:
                    return I_bge;
                case BLTU3:
                    return I_bltu;
                case BGEU3:
                    return I_bgeu;
                default:
                    break;
            }
            break;
        case ILType:
            switch (funct3) {
                case LB3:
                    return I_lb;
                case LH3:
                    return I_lh;
                case LW3:
                    return I_lw;
                case LBU3:
                    return I_lbu;
                default:
                    break;
            }
            break;
        case JAL:
            return I_jal;
        case JALR:
            return I_jalr;
        case StoreType:
            switch (funct3) {
                case SB3:
                    return I_sb;
                case SH3:
                    return I_sh;
                case SW3:
                    return I_sw;
                default:
                    break;
            }
            break;
        case ILAType:
            switch (funct3) {
                case ADDI3:
                    return I_addi;
                case ORI3:
                    return I_ori;
                case ANDI3:
                    return I_andi;
                case XORI3:
                    return I_xori;
                case SLTI3:
                    return I_slti;
                case SLTIU3:
                    return I_sltiu;
                case SLLI3:
                    return I_slli;
                case SRI3:
                    if (funct7 == SRLI7) return I_srli;
                    else return I_srai;
                default:
                    break;
            }
            break;
        case RegType:
            switch (funct3) {
                case ADDSUB3:
                    if (funct7 == SUB7) return I_sub;
                    else return I_add;
                case SLL3:
                    return I_sll;
                case SLT3:
                    return I_slt;
                case SLTU3:
                    return I_sltu;
                case XOR3:
                    return I_xor;
                case OR3:
                    return I_or;
                case AND3:
                    return I_and;
                case SR3:
                    if (funct7 == SRA7) return I_sra;
                    else return I_srl;
                default:
                    break;
            }
            break;
        case ECALL:
            return I_ecall;
        default:
            printf("\n\nInstrucao Invalida (PC = %08x RI = %08x)\n", pc, ri);
            break;
    }
    return I_nop;
}

void execute() {
    int32_t aux;

    regs[0] = 0;

    switch(ins_code){
        case I_lui:
            regs[rd] = imm20_u;
            break;
        case I_auipc:
            regs[rd] = imm20_u + pc;
            break;
        case I_beq:
            if(regs[rs1] == regs[rs2]){
                pc = pc + imm13;
            }
            break;
        case I_bne:
            if(regs[rs1] != regs[rs2]){
                pc = pc + imm13;
            }
            break;
        case I_blt:
            if(regs[rs1] < regs[rs2]){
                pc = pc + imm13;
            }
            break;
        case I_bge:
            if(regs[rs1] >= regs[rs2]){
                pc = pc + imm13;
            }
            break;
        case I_bltu:
            if((uint32_t)regs[rs1] < (uint32_t)regs[rs2]){
                pc = pc + imm13;
            }
            break;
        case I_bgeu:
            if((uint32_t)regs[rs1] >= (uint32_t)regs[rs2]){
                pc = pc + imm13;
            }
            break;
        case I_lb:
            regs[rd] = lb(regs[rs1], imm12_i);
            break;
        case I_lh:
            regs[rd] = lw(regs[rs1], imm12_i);
            break;
        case I_lw:
            regs[rd] = lw(regs[rs1], imm12_i);
            break;
        case I_lbu:
            regs[rd] = lbu(regs[rs1], imm12_i);
            break;
        case I_jal:
            regs[rd] = pc + 4;
            pc = pc + imm21;
            break;
        case I_jalr:
            regs[rd] = pc + 4;
            pc = (regs[rs1] + imm12_i) & ~1;
            break;
        case I_sb:
            sb(regs[rs1], imm12_s, (int8_t)regs[rs2]);
            break;
        case I_sh:
            sb(regs[rs1], imm12_s, (int16_t)regs[rs2]);
            break;
        case I_sw:
            sw(regs[rs1], imm12_s, (int8_t)regs[rs2]);
            break;
        case I_addi:
            regs[rd] = regs[rs1] + imm12_i;
            break;
        case I_ori:
            regs[rd] = regs[rs1] | imm12_i;
            break;
        case I_andi:
            regs[rd] = regs[rs1] & imm12_i;
            break;
        case I_xori:
            regs[rd] = regs[rs1] ^ imm12_i;
            break;
        case I_slti:
            regs[rd] = regs[rs1] < imm12_i;
            break;
        case I_sltiu:
            regs[rd] = (uint32_t)regs[rs1] < (uint32_t)imm12_i;
            break;
        case I_slli:
            regs[rd] = regs[rs1] << shamt;
            break;
        case I_srai:
            regs[rd] = regs[rs1] >> shamt;
            break;
        case I_srli:
            regs[rd] = (uint32_t)regs[rs1] >> shamt;
            break;
        case I_sub:
            regs[rd] = regs[rs1] - regs[rs2];
            break;
        case I_add:
            regs[rd] = regs[rs1] + regs[rs2];
            break;
        case I_sll:
            regs[rd] = regs[rs1] << regs[rs2];
            break;
        case I_slt:
            regs[rd] = regs[rs1] < regs[rs2];
            break;
        case I_sltu:
            regs[rd] = (uint32_t)regs[rs1] < (uint32_t)regs[rs2];
            break;
        case I_xor:
            regs[rd] = regs[rs1] ^ regs[rs2];
            break;
        case I_or:
            regs[rd] = regs[rs1] | regs[rs2];
            break;
        case I_and:
            regs[rd] = regs[rs1] & regs[rs2];
            break;
        case I_sra:
            regs[rd] = regs[rs1] >> regs[rs2];
            break;
        case I_srl:
            regs[rd] = (uint32_t)regs[rs1] >> regs[rs2];
            break;
        case I_ecall:
            switch(regs[A7]){

                case 1:
                    printf("%ld", regs[A0]);
                    break;
                case 4:
                    {
                    char *c;
                    c = (char*)mem + regs[A0];
                    printf("%s", c);
                    break;
                    }
                case 5:
                    printf("%ld", regs[A0]);
                    break;
                case 6:
                    printf("%ld", regs[A0]);
                    break;
                case 10:
                    stop_prg = true;
                    break;
                default:
                    break;
            }
            break;
        default:
            if(ri == 0){
                exit(0);
            }
            printf("Instrucao falha (PC = %08x RI = %08x)\n", pc, ri);
            break;
    }
}

void step() {
    fetch();
    decode();
    execute();
}

void run() {
    init();
    while ((pc < DATA_SEGMENT_START) && !stop_prg)
        step();
}

int main()
{

    init();

    DATA_SEGMENT_START = 2048;

    int size1 = load_mem("code", 0);

    size1 = load_mem("data", 0x2000);

    run();

    return 0;
}
