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

#include "opcodes.h"

const char* opcodes[256] =
	{
	"nop", "return", "return nil", "pop", "super", "throw",
	"<", "<=", ">", ">=", "!", "~", "each", "dup", "block", "push int",
	"push literal", "push literal", "push literal", "push literal",
	"push literal", "push literal", "push literal", "push literal",
	"push literal", "push literal", "push literal", "push literal",
	"push literal", "push literal", "push literal", "push literal",
	"push auto", "push auto", "push auto", "push auto",
	"push auto", "push auto", "push auto", "push auto",
	"push auto", "push auto", "push auto", "push auto",
	"push auto", "push auto", "push auto", "bool",
	"= auto", "= auto", "= auto", "= auto",
	"= auto", "= auto", "= auto", "= auto",
	"= auto pop", "= auto pop", "= auto pop", "= auto pop",
	"= auto pop", "= auto pop", "= auto pop", "= auto pop",
	"call global", "call global", "call global", "call global",
	"call global", "call global", "call global", "call global",
	"call global pop", "call global pop", "call global pop", "call global pop",
	"call global pop", "call global pop", "call global pop", "call global pop",
	"call mem", "call mem", "call mem", "call mem",
	"call mem", "call mem", "call mem", "call mem",
	"call mem pop", "call mem pop", "call mem pop", "call mem pop",
	"call mem pop", "call mem pop", "call mem pop", "call mem pop",
	"call mem this", "call mem this", "call mem this", "call mem this",
	"call mem this", "call mem this", "call mem this", "call mem this",
	"call mem this pop", "call mem this pop", "call mem this pop", "call mem this pop",
	"call mem this pop", "call mem this pop", "call mem this pop", "call mem this pop",
	"push sub", "push sub this", "push literal", "push auto",
	"push dynamic", "push mem", "push mem this", "push global",
	"push value false", "push value true", "push value \"\"", "push value this",
	"push value -1", "push value 0", "push value 1", "try",
	"+= sub", "-= sub",	"$= sub", "*= sub", "/= sub", "%= sub",
	"<<= sub", ">>= sub", "&= sub", "|= sub", "^= sub",
	"= sub", "++? sub", "--? sub", "?++ sub", "?-- sub",
	"+= sub this", "-= sub this", "$= sub this", "*= sub this", "/= sub this", "%= sub this",
	"<<= sub this", ">>= sub this", "&= sub this", "|= sub this", "^= sub this",
	"= sub this","++? sub this", "--? sub this", "?++ sub this", "?-- sub this",
	"call sub", "call sub this", "call stack", "call auto",
	"call dynamic", "call mem", "call mem this", "call global",
	"jump", "jump pop yes", "jump pop no", "jump case yes", "jump case no",
	"jump else pop yes", "jump else pop no", "catch",
	"+= auto", "-= auto",	"$= auto", "*= auto", "/= auto", "%= auto",
	"<<= auto", ">>= auto", "&= auto", "|= auto", "^= auto",
	"= auto", "++? auto", "--? auto", "?++ auto", "?-- auto",
	"+= dynamic", "-= dynamic",	"$= dynamic", "*= dynamic", "/= dynamic", "%= dynamic",
	"<<= dynamic", ">>= dynamic", "&= dynamic", "|= dynamic", "^= dynamic",
	"= dynamic", "++? dynamic", "--? dynamic", "?++ dynamic", "?-- dynamic",
	"+= mem", "-= mem",	"$= mem", "*= mem", "/= mem", "%= mem",
	"<<= mem", ">>= mem", "&= mem", "|= mem", "^= mem",
	"= mem", "++? mem", "--? mem", "?++ mem", "?-- mem",
	"+= mem this", "-= mem this", "$= mem this", "*= mem this", "/= mem this", "%= mem this",
	"<<= mem this", ">>= mem this", "&= mem this", "|= mem this", "^= mem this",
	"= mem this", "++? mem this", "--? mem this", "?++ mem this", "?-- mem this",
	"+", "-", "$", "*", "/", "%", "<<", ">>", "&", "|", "^",
	"==", "!=", "=~", "!~", "-?"
	};
