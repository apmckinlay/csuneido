/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2006 Suneido Software Corp.
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

#include "sufinalize.h"
#include "gc.h"

SuFinalize::SuFinalize()
	{
	if (void* base = GC_base(this))
		GC_register_finalizer_ignore_self(base, (GC_finalization_proc) cleanup,
			(void*)((char*) this - (char*) base), 0, 0);
	}

void SuFinalize::removefinal()
	{
	GC_register_finalizer_ignore_self(GC_base(this), 0, 0, 0, 0);
	}

void SuFinalize::cleanup(void* base, void* data)
	{
	SuValue* ob = (SuValue*)((char*) base + (int) data);
	ob->finalize();
	}
