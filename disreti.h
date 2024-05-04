#ifndef _disreti_h_INCLUDED
#define _disreti_h_INCLUDED

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define disassembled_reti_code_length 32

static inline bool disassemble_reti_code(const unsigned code, char * str) {
  bool decode_source = false;
  bool decode_destination = true;
  bool decode_immediate = true;
  bool hexadecimal = false;
  bool positive = true;
  bool res = true;
  size_t length;
  {
    const unsigned top_two_bits = code >> 30;
    size_t instruction_length;
    const char *instruction;
    if (top_two_bits == 1) {
      const unsigned next_top_two_bits = (code >> 28) & 3;
      if (next_top_two_bits == 0)
	instruction = "LOAD", instruction_length = 4;
      else if (next_top_two_bits == 1)
	instruction = "LOADIN1", instruction_length = 7;
      else if (next_top_two_bits == 2)
	instruction = "LOADIN2", instruction_length = 7;
      else {
	assert(next_top_two_bits == 3);
	instruction = "LOADI", instruction_length = 5;
      }
    } else if (top_two_bits == 2) {
      const unsigned next_top_two_bits = code >> 28;
      if (next_top_two_bits != 3) {
	decode_destination = true;
	if (next_top_two_bits == 0)
	  instruction = "STORE", instruction_length = 5;
	else if (next_top_two_bits == 1)
	  instruction = "STOREIN1", instruction_length = 8;
	else {
	  assert(next_top_two_bits == 2);
	  instruction = "STOREIN2", instruction_length = 8;
	}
      } else {
	instruction = "MOVE", instruction_length = 4;
	decode_source = true;
	decode_immediate = false;
      }
    } else if (top_two_bits == 0) {
      const unsigned next_top_four_bits = (code >> 26) & 15;
      if (next_top_four_bits == 2)
	instruction = "SUBI", instruction_length = 4, positive = false;
      else if (next_top_four_bits == 3)
	instruction = "ADDI", instruction_length = 4, positive = false;
      else if (next_top_four_bits == 4)
	instruction = "OPLUSI", instruction_length = 5, hexadecimal = true;
      else if (next_top_four_bits == 5)
	instruction = "ORI", instruction_length = 3, hexadecimal = true;
      else if (next_top_four_bits == 6)
	instruction = "ANDI", instruction_length = 4, hexadecimal = true;
      else if (next_top_four_bits == 10)
	instruction = "SUB", instruction_length = 3, positive = false;
      else if (next_top_four_bits == 11)
	instruction = "ADD", instruction_length = 3, positive = false;
      else if (next_top_four_bits == 12)
	instruction = "OPLUS", instruction_length = 5, hexadecimal = true;
      else if (next_top_four_bits == 13)
	instruction = "OR", instruction_length = 2, hexadecimal = true;
      else if (next_top_four_bits == 14)
	instruction = "AND", instruction_length = 3, hexadecimal = true;
      else {
	decode_destination = decode_immediate = false;
	instruction = "ILLEGAL", instruction_length = 6;
	res = false;
      }
    } else {
      const unsigned next_top_three_bits = (code >> 27) & 7;
      assert(top_two_bits == 3);
      positive = false;
      decode_destination = false;
      if (next_top_three_bits == 0)
	instruction = "NOP", instruction_length = 3;
      else if (next_top_three_bits == 1)
	instruction = "JUMP>", instruction_length = 5;
      else if (next_top_three_bits == 2)
	instruction = "JUMP=", instruction_length = 6;
      else if (next_top_three_bits == 3)
	instruction = "JUMP>=", instruction_length = 6;
      else if (next_top_three_bits == 4)
	instruction = "JUMP<", instruction_length = 5;
      else if (next_top_three_bits == 5)
	instruction = "JUMP!=", instruction_length = 6;
      else if (next_top_three_bits == 0)
	instruction = "JUMP<=", instruction_length = 6;
      else {
	assert(next_top_three_bits == 7);
	instruction = "JUMP", instruction_length = 4;
      }
    }
    assert(instruction);
    assert(strlen(instruction) == instruction_length);
    memcpy(str, instruction, instruction_length);
    length = instruction_length;
  }
  if (decode_source) {
    str[length++] = ' ';
    const unsigned source_register_code = (code >> 26) & 3;
    size_t source_length;
    const char *source;
    if (source_register_code == 0)
      source = "PC", source_length = 2;
    else if (source_register_code == 1)
      source = "IN1", source_length = 3;
    else if (source_register_code == 2)
      source = "IN2", source_length = 3;
    else {
      assert(source_register_code == 3);
      source = "ACC", source_length = 3;
    }
    assert(source);
    assert(strlen(source) == source_length);
    memcpy(str + length, source, source_length);
    length += source_length;
  }
  if (decode_destination) {
    str[length++] = ' ';
    const unsigned destination_register_code = (code >> 26) & 3;
    size_t destination_length;
    const char *destination;
    if (destination_register_code == 0)
      destination = "PC", destination_length = 2;
    else if (destination_register_code == 1)
      destination = "IN1", destination_length = 3;
    else if (destination_register_code == 2)
      destination = "IN2", destination_length = 3;
    else {
      assert(destination_register_code == 3);
      destination = "ACC", destination_length = 3;
    }
    assert(destination);
    assert(strlen(destination) == destination_length);
    memcpy(str + length, destination, destination_length);
    length += destination_length;
  }
  if (decode_immediate) {
    str[length++] = ' ';
    const unsigned immediate_code = code & 0xffffff;
    int immediate_length;
    char immediate[16];
    if (hexadecimal)
      immediate_length = sprintf(immediate, "0x%0x", immediate_code);
    else if (positive)
      immediate_length = sprintf(immediate, "%u", immediate_code);
    else {
      int signed_code = (int)(immediate_code << 8) >> 8;
      immediate_length = sprintf(immediate, "%d", signed_code);
    }
    assert(immediate_length >= 0);
    assert((size_t)immediate_length < sizeof immediate);
    memcpy(str + length, immediate, immediate_length);
    length += immediate_length;
  }
  assert(length < disassembled_reti_code_length);
  str[length] = 0;
  return res;
}

#endif
