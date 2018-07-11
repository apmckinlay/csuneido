#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class Value;
class Frame;

Value suBlock(Frame* frame, int pc, int first, int nparams);

void persist_if_block(Value x);

const int BLOCK_REST = 255;
