// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "opcodes.h"

const char* opcodes[256] = { //
	"nop", "return", "return nil", "pop", "super", "throw", "<", "<=", ">",
	">=", "not", "~", "each", "dup", "block", "push int",

	"push literal", "push literal", "push literal", "push literal",
	"push literal", "push literal", "push literal", "push literal",
	"push literal", "push literal", "push literal", "push literal",
	"push literal", "push literal", "push literal", "push literal",

	"push auto", "push auto", "push auto", "push auto", "push auto",
	"push auto", "push auto", "push auto", "push auto", "push auto",
	"push auto", "push auto", "push auto", "push auto", "push auto", "bool",

	"= auto", "= auto", "= auto", "= auto", "= auto", "= auto", "= auto",
	"= auto",

	"= auto pop", "= auto pop", "= auto pop", "= auto pop", "= auto pop",
	"= auto pop", "= auto pop", "= auto pop",

	"call global", "call global", "call global", "call global", "call global",
	"call global", "call global", "call global",

	"call global pop", "call global pop", "call global pop", "call global pop",
	"call global pop", "call global pop", "call global pop", "call global pop",

	"call mem", "call mem", "call mem", "call mem", "call mem", "call mem",
	"call mem", "call mem",

	"call mem pop", "call mem pop", "call mem pop", "call mem pop",
	"call mem pop", "call mem pop", "call mem pop", "call mem pop",

	"call mem this", "call mem this", "call mem this", "call mem this",
	"call mem this", "call mem this", "call mem this", "call mem this",

	"call mem this pop", "call mem this pop", "call mem this pop",
	"call mem this pop", "call mem this pop", "call mem this pop",
	"call mem this pop", "call mem this pop",

	"push sub", "push sub this", "push literal", "push auto", "push dynamic",
	"push mem", "push mem this", "push global", "push value false",
	"push value true", "push value \"\"", "push value this", "push value 0",
	"push value 1", "uplus", "try",

	"+= sub", "-= sub", "$= sub", "*= sub", "/= sub", "%= sub", "<<= sub",
	">>= sub", "&= sub", "|= sub", "^= sub", "= sub", "++? sub", "--? sub",
	"?++ sub", "?-- sub", "+= sub this", "-= sub this", "$= sub this",
	"*= sub this", "/= sub this", "%= sub this", "<<= sub this", ">>= sub this",
	"&= sub this", "|= sub this", "^= sub this", "= sub this", "++? sub this",
	"--? sub this", "?++ sub this", "?-- sub this",

	"call sub", "call sub this", "call stack", "call auto", "call dynamic",
	"call mem", "call mem this", "call global", "jump", "jump pop yes",
	"jump pop no", "jump case yes", "jump case no", "jump else pop yes",
	"jump else pop no", "catch",

	"+= auto", "-= auto", "$= auto", "*= auto", "/= auto", "%= auto",
	"<<= auto", ">>= auto", "&= auto", "|= auto", "^= auto", "= auto",
	"++? auto", "--? auto", "?++ auto", "?-- auto",

	"+= dynamic", "-= dynamic", "$= dynamic", "*= dynamic", "/= dynamic",
	"%= dynamic", "<<= dynamic", ">>= dynamic", "&= dynamic", "|= dynamic",
	"^= dynamic", "= dynamic", "++? dynamic", "--? dynamic", "?++ dynamic",
	"?-- dynamic",

	"+= mem", "-= mem", "$= mem", "*= mem", "/= mem", "%= mem", "<<= mem",
	">>= mem", "&= mem", "|= mem", "^= mem", "= mem", "++? mem", "--? mem",
	"?++ mem", "?-- mem",

	"+= mem this", "-= mem this", "$= mem this", "*= mem this", "/= mem this",
	"%= mem this", "<<= mem this", ">>= mem this", "&= mem this", "|= mem this",
	"^= mem this", "= mem this", "++? mem this", "--? mem this", "?++ mem this",
	"?-- mem this",

	"+", "-", "$", "*", "/", "%", "<<", ">>", "&", "|", "^", "is", "isnt", "=~",
	"!~", "-?"};
