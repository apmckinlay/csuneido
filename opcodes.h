#ifndef OPCODES_H
#define OPCODES_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef FALSE
#undef TRUE

enum { UNCOND, POP_YES, POP_NO, CASE_YES, CASE_NO,
	ELSE_POP_YES, ELSE_POP_NO };
enum { SUB, SUB_SELF, LITERAL, AUTO, DYNAMIC, MEM, MEM_SELF, GLOBAL };
enum { FALSE = 0, TRUE, EMPTY_STRING, SELF, MINUS_ONE, ZERO, ONE };

enum
	{
	I_NOP = 0, I_RETURN, I_RETURN_NIL, I_POP, I_SUPER, I_THROW,
	I_LT, I_LTE, I_GT, I_GTE, I_NOT, I_BITNOT,
	I_EACH, I_DUP, I_BLOCK, I_PUSH_INT,

	I_PUSH_LITERAL = 0x10,
	I_PUSH_AUTO = 0x20, I_BOOL = 0x2f,
	I_EQ_AUTO = 0x30, I_EQ_AUTO_POP = 0x38,
	I_CALL_GLOBAL = 0x40, I_CALL_GLOBAL_POP = 0x48,
	I_CALL_MEM = 0x50, I_CALL_MEM_POP = 0x58,
	I_CALL_MEM_SELF = 0x60, I_CALL_MEM_SELF_POP = 0x68,
	I_PUSH = 0x70,
	I_PUSH_VALUE = 0x78,
	I_TRY = 0x7f,
	I_CALL = 0xa0,
	I_JUMP = 0xa8,
	I_CATCH = 0xaf,

	I_ADDEQ = 0x80, I_SUBEQ, I_CATEQ, I_MULEQ, I_DIVEQ, I_MODEQ,
	I_LSHIFTEQ, I_RSHIFTEQ, I_BITANDEQ, I_BITOREQ, I_BITXOREQ,
	I_EQ, I_PREINC, I_PREDEC, I_POSTINC, I_POSTDEC,

	I_ADD = 0xf0, I_SUB, I_CAT, I_MUL, I_DIV, I_MOD,
	I_LSHIFT, I_RSHIFT, I_BITAND, I_BITOR, I_BITXOR,
	I_IS, I_ISNT, I_MATCH, I_MATCHNOT, I_UMINUS
	};

extern const char* opcodes[];

#endif
