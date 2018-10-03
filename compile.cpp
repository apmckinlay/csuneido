// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// NOTE: because of the static's this code is NOT re-entrant

#include "compile.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "std.h"
#include <vector>
#include <memory>
#include "value.h"
#include "sunumber.h"
#include "scanner.h"
#include "sustring.h"
#include "suobject.h"
#include "sufunction.h"
#include "type.h"
#include "structure.h"
#include "dll.h"
#include "callback.h"
#include "suclass.h"
#include "interp.h"
#include "globals.h"
#include "sudate.h"
#include "symbols.h"
#include "catstr.h"
#include "surecord.h"
#include "gc.h"
#include "codevisitor.h"
#include "sublock.h" // for BLOCK_REST
#include "varint.h"
#include "opcodes.h"
#include "ostreamstr.h"

bool getSystemOption(const char* option, bool def_value);

using namespace std;

template <class T>
T* dup(const vector<T>& x) {
	T* y = reinterpret_cast<T*>(new char[x.size() * sizeof(T)]);
	std::uninitialized_copy(x.begin(), x.end(), y);
	return y;
}

template <class T>
T* dup(const vector<T>& x, NoPtrs) {
	T* y = reinterpret_cast<T*>(new (noptrs) char[x.size() * sizeof(T)]);
	std::uninitialized_copy(x.begin(), x.end(), y);
	return y;
}

struct Container {
	virtual Value get(Value m) = 0;
	virtual void add(Value m) {
		unreachable();
	}
	virtual void put(Value m, Value x) = 0;
	virtual const Named* get_named() {
		return nullptr;
	}
};
struct ObjectContainer : public Container {
	SuObject* ob;
	ObjectContainer(SuObject* o) : ob(o) {
	}
	Value get(Value m) override {
		return ob->get(m);
	}
	void add(Value m) override {
		ob->add(m);
	}
	void put(Value m, Value x) override {
		ob->put(m, x);
	}
};
struct ClassContainer : public Container {
	SuClass* c;
	ClassContainer(SuClass* c_) : c(c_) {
	}
	Value get(Value m) override {
		return c->get(m);
	}
	void put(Value m, Value x) override {
		c->put(m, x);
	}
	const Named* get_named() override {
		return c->get_named();
	}
};

class Compiler {
public:
	explicit Compiler(const char* s, CodeVisitor* v = 0);
	Compiler(Scanner& sc, int t, int sn) // for FunctionCompiler
		: scanner(sc), stmtnest(sn), token(t) {
	}
	Value constant(const char* gname, const char* className);
	Value constant() {
		return constant(nullptr, nullptr);
	}
	Value object();
	Value suclass(const char* gname, const char* classNam);
	Value functionCompiler(
		short base, bool newfn, const char* gname, const char* className);
	Value functionCompiler(const char* gname) {
		return functionCompiler(-1, false, gname, nullptr);
	}
	Value dll();
	Value structure();
	Value callback();
	Value number();

	Scanner& scanner;
	int stmtnest; // if 0 then newline = end of statement
	int token;

	void match();
	bool binopnext();
	void match(int t);
	void match1();
	void matchnew();
	void matchnew(int t);
	void ckmatch(int t);
	[[noreturn]] void syntax_error_(const char* err = "") const;

	void member(
		Container& ob, const char* gname, const char* className, short base);
	void member(Container& ob) {
		member(ob, nullptr, nullptr, -1);
	}
	Value memname(const char* className, const char* s);
	const char* ckglobal(const char*);

private:
	bool valid_dll_arg_type();

protected:
	bool anyName() const;
};

#define syntax_error(stuff) \
	do { \
		OstreamStr os; \
		os << stuff; /* NOLINT */ \
		syntax_error_(os.str()); \
	} while (false)

struct PrevLit {
	PrevLit() {
	}
	PrevLit(int a, Value l, int i = -99) : adr(a), il(i), lit(l) {
	}
	uint16_t adr = 0;
	short il = 0;
	Value lit;
};

class FunctionCompiler : public Compiler {
public:
	FunctionCompiler(Scanner& scanner, int token, int stmtnest, short b,
		bool nf, const char* gn, const char* cn)
		: Compiler(scanner, token, stmtnest), newfn(nf), base(b), gname(gn),
		  className(cn) {
		code.reserve(2000);
		db.reserve(500);
	}
	SuFunction* function();

private:
	SuFunction* fn = nullptr; // so local functions/classes can set parent
	vector<uint8_t> code;
	vector<Debug> db;
	short last_adr = -1;
	vector<PrevLit> prevlits; // for emit const expr optimization
	vector<Value> literals;
	vector<short> locals;
	vector<bool> localsHide;
	short nparams = 0;
	short ndefaults = 0;
	bool rest = false;
	bool newfn;
	short base;
	const char* gname;
	const char* className;
	bool inblock = false;
	bool expecting_compound = false;
	// for loops
	enum { maxtest = 100 };
	uint8_t test[maxtest]{};
	bool it_used = false; // for blocks
	bool inside_try = false;

	void block();
	void statement(short = -1, short* = NULL);
	void compound(short = -1, short* = NULL);
	void body();
	void stmtexpr();
	void exprlist();
	void opt_paren_expr();
	void expr() {
		triop();
	}
	void triop();
	void orop();
	void andop();
	void inop();
	bool isIn();
	void bitorop();
	void bitxor();
	void bitandop();
	void isop();
	void cmpop();
	void shift();
	void addop();
	void mulop();
	void unop();
	void expr0(bool newtype = false);
	void args(short&, vector<short>&, const char* delims = "()");
	void args_at(short& nargs, const char* delims);
	void add_argname(vector<short>& argnames, int id);
	void args_list(short& nargs, const char* delims, vector<short>& argnames);
	void keywordArgShortcut(vector<short>& argnames);
	bool isKeyword();
	bool just_name();
	void record();
	uint16_t literal(Value value, bool reuse = false);
	short emit_literal();
	uint16_t local(bool init = false);
	void addLocal(int num);
	void addLocal(const char* name);
	PrevLit poplits();
	short emit(short, short = 0, short = 0, short = 0, vector<short>* = 0);
	void patch(short);
	void mark();
	void params(vector<char>& flags);
	uint16_t param();
	bool notAllZero(vector<char>& flags);
	void emit_target(int option, int target);
	uint16_t mem(const char* s);
};

Value compile(const char* s, const char* gname, CodeVisitor* visitor) {
	Compiler compiler(s, visitor);
	Value x = compiler.constant(gname, gname);
	if (compiler.token != -1)
		compiler.syntax_error_();
	return x;
}

// Compiler ---------------------------------------------------------------

Compiler::Compiler(const char* s, CodeVisitor* visitor)
	: scanner(*new Scanner(dupstr(s), 0, visitor ? visitor : new CodeVisitor)),
	  stmtnest(99), token(0) {
	match(); // get first token
}

Value Compiler::constant(const char* gname, const char* className) {
	Value x;
	switch (token) {
	case I_SUB:
		match();
		return -number();
	case I_ADD:
		match();
		// fallthrough
	case T_NUMBER:
		return number();
	case T_STRING: {
		if (scanner.source[scanner.prev] == '#') {
			x = symbol(scanner.value);
			match();
			return x;
		}
		gcstring s;
		while (true) {
			s += gcstring(scanner.value, scanner.len);
			match(T_STRING);
			if (token == I_CAT && scanner.ahead() == T_STRING)
				matchnew(I_CAT);
			else
				break;
		}
		return s == "" ? SuEmptyString : new SuString(s);
	}
	case '#':
		match();
		if (token == T_NUMBER) {
			if (!((x = SuDate::literal(scanner.value))))
				syntax_error("bad date literal");
			match();
			return x;
		} else if (token != '(' && token != '{' && token != '[')
			syntax_error("invalid literal following '#'");
		// else fall thru
	case '(':
	case '{':
	case '[':
		return object();
	case T_IDENTIFIER:
		switch (scanner.keyword) {
		case K_FUNCTION:
			matchnew();
			return functionCompiler(gname);
		case K_CLASS:
			return suclass(gname, className);
		case K_DLL:
			return dll();
		case K_STRUCT:
			return structure();
		case K_CALLBACK:
			return callback();
		case K_TRUE:
			match();
			return SuTrue;
		case K_FALSE:
			match();
			return SuFalse;
		default:
			if (scanner.ahead() == '{')
				return suclass(gname, className);
			// else identifier => string
			x = new SuString(scanner.value);
			match();
			return x;
		}
	default:
		break;
	}
	if (anyName()) {
		x = new SuString(scanner.value);
		match();
		return x;
	}
	syntax_error_();
}

bool Compiler::anyName() const {
	if (token == T_STRING || token == T_IDENTIFIER || token >= KEYWORDS)
		return true;
	if ((token == T_AND || token == T_OR || token == I_NOT || token == I_IS ||
			token == I_ISNT) &&
		isalpha(*scanner.value))
		return true;
	return false;
}

Value Compiler::number() {
	if (token != T_NUMBER)
		syntax_error_();
	Value result = SuNumber::literal(scanner.value);
	match(T_NUMBER);
	return result;
}

Value Compiler::object() { //=======================================
	SuObject* ob;
	char end;
	if (token == '(') {
		ob = new SuObject();
		end = ')';
		match();
	} else if (token == '{' || token == '[') {
		ob = new SuRecord();
		end = token == '{' ? '}' : ']';
		match();
	} else
		syntax_error_();
	ObjectContainer con(ob);
	while (token != end) {
		member(con);
		if (token == ',' || token == ';')
			match();
	}
	match(end);
	ob->set_readonly();
	return ob;
}

const char* Compiler::ckglobal(const char* s) {
	if (!(isupper(*s) || (*s == '_' && isupper(s[1]))))
		syntax_error("base class must be global defined in library");
	return s;
}

Value Compiler::suclass(const char* gname, const char* className) {
	if (!className) {
		static int classnum = 0;
		char buf[32] = "Class";
		_itoa(classnum++, buf + 5, 10);
		className = buf;
	}

	if (scanner.keyword == K_CLASS) {
		matchnew();
		if (token == ':')
			matchnew();
	}
	short base = 0;
	if (token != '{') {
		if (*scanner.value == '_') {
			if (!gname || 0 != strcmp(gname, scanner.value + 1))
				syntax_error("invalid reference to " << scanner.value);
			base = globals.copy(ckglobal(scanner.value)); // throws if undefined
		} else
			base = globals(ckglobal(scanner.value));
		scanner.visitor->global(scanner.prev, scanner.value);
		matchnew(T_IDENTIFIER);
	}
	SuClass* c = new SuClass(base);
	ClassContainer con(c);
	match('{');
	while (token != '}') {
		member(con, gname, className, base);
		if (token == ',' || token == ';')
			match();
	}
	match('}');
	return c;
}

// object constant & class members
void Compiler::member(
	Container& ob, const char* gname, const char* className, short base) {
	Value mv;
	bool name = false;
	bool minus = false;
	if (token == I_SUB) {
		minus = true;
		match();
		if (token != T_NUMBER)
			syntax_error_();
	}
	bool default_allowed = true;
	int ahead = scanner.ahead();
	if (ahead == ':' || (base >= 0 && ahead == '(')) {
		if (anyName()) {
			mv = memname(className, scanner.value);
			name = true;
			match();
		} else if (token == T_NUMBER) {
			mv = number();
			if (minus) {
				mv = -mv;
				minus = false;
			}
		} else
			syntax_error_();
		if (token == ':')
			match();
	} else
		default_allowed = false;
	if (base >= 0 && !mv)
		syntax_error("class members must be named");

	Value x;
	if (ahead == '(' && base >= 0)
		x = functionCompiler(base, mv.gcstr() == "New", gname, className);
	else if (token != ',' && token != ')' && token != '}' && token != ']') {
		x = constant();
		if (minus)
			x = -x;
	} else if (default_allowed)
		x = SuTrue; // default value
	else
		syntax_error_();

	if (mv) {
		if (ob.get(mv))
			syntax_error("duplicate member name (" << mv << ")");
		ob.put(mv, x);
	} else
		ob.add(x);
	if (name)
		if (Named* nx = const_cast<Named*>(x.get_named()))
			if (auto nob = ob.get_named()) {
				nx->parent = nob;
				nx->str = mv.str();
			}
}

// struct, dll, callback -------------------------------------------------

const int maxitems = 100;

Value Compiler::structure() {
	matchnew();
	match('{');
	TypeItem memtypes[maxitems];
	short memnames[maxitems];
	short n = 0;
	for (; token == T_IDENTIFIER; ++n) {
		except_if(n >= maxitems, "too many structure members");
		memtypes[n].gnum = globals(scanner.value);
		match();
		memtypes[n].n = 1;
		// NOTE: don't allow pointer & array in same type
		if (token == I_MUL) { // pointer
			match();
			memtypes[n].n = 0;
		} else if (token == '[') { // array
			match('[');
			memtypes[n].n = -atoi(scanner.value);
			match(T_NUMBER);
			match(']');
		}
		memnames[n] = symnum(scanner.value);
		match(T_IDENTIFIER);
		if (token == ';')
			match();
	}
	match('}');
	return new Structure(memtypes, memnames, n);
}

Value Compiler::dll() {
	matchnew();
	short rtype;
	switch (scanner.keyword) {
	case K_VOID:
		rtype = 0;
		break;
	case K_BOOL:
	case K_INT8:
	case K_INT16:
	case K_INT32:
	case K_INT64:
	case K_POINTER:
	case K_FLOAT:
	case K_DOUBLE:
	case K_STRING:
	case K_HANDLE:
	case K_GDIOBJ:
		rtype = globals(scanner.value);
		break;
	default:
		syntax_error("invalid dll return type");
	}
	matchnew(T_IDENTIFIER);

	const int maxname = 80;
	char library[maxname];
	strncpy(library, scanner.value, maxname);
	matchnew(T_IDENTIFIER);
	matchnew(':');
	char name[maxname];
	strncpy(name, scanner.value, maxname);
	matchnew(T_IDENTIFIER);
	if (token == '@') {
		if (strlen(name) + 1 < maxname)
			strcat(name, "@");
		matchnew('@');
		if (strlen(name) + strlen(scanner.value) < maxname)
			strcat(name, scanner.value);
		matchnew(T_NUMBER);
	}

	match('(');
	TypeItem paramtypes[maxitems];
	short paramnames[maxitems];
	int n = 0;
	for (; token != ')'; ++n) {
		except_if(n >= maxitems, "too many dll parameters");
		if (token == '[') {
			match('[');
			match(K_IN);
			match(']');
			if (scanner.keyword != K_STRING)
				syntax_error_();
			paramtypes[n].gnum = globals("instring");
		} else if (token == T_IDENTIFIER) {
			if (!valid_dll_arg_type())
				syntax_error("invalid dll parameter type");
			paramtypes[n].gnum = globals(scanner.value);
		} else
			syntax_error_();
		match();
		paramtypes[n].n = 1;
		// TODO: Reject pointer-to-primitive at syntax level.
		if (token == I_MUL) { // pointer
			match();
			paramtypes[n].n = 0;
		}
		paramnames[n] = symnum(scanner.value);
		match(T_IDENTIFIER);
		if (token == ',')
			match();
	}
	match(')');
	return new Dll(rtype, library, name, paramtypes, paramnames, n);
}

bool Compiler::valid_dll_arg_type() {
	if (isupper(*scanner.value))
		return true;
	switch (scanner.keyword) {
	case K_BOOL:
	case K_FLOAT:
	case K_DOUBLE:
	case K_INT8:
	case K_INT16:
	case K_INT32:
	case K_INT64:
	case K_POINTER:
	case K_STRING:
	case K_BUFFER:
	case K_HANDLE:
	case K_GDIOBJ:
	case K_RESOURCE:
		return true;
	default:
		return false;
	}
}

Value Compiler::callback() {
	matchnew();
	match('(');
	TypeItem paramtypes[maxitems];
	short paramnames[maxitems];
	short n = 0;
	for (; token == T_IDENTIFIER; ++n) {
		except_if(n >= maxitems, "too many callback parameters");
		paramtypes[n].gnum = globals(scanner.value);
		match();
		paramtypes[n].n = 1;
		if (token == I_MUL) { // pointer
			match();
			paramtypes[n].n = 0;
		}
		paramnames[n] = symnum(scanner.value);
		match(T_IDENTIFIER);
		if (token == ',')
			match();
	}
	match(')');
	return new Callback(paramtypes, paramnames, n);
}

// scanner functions ------------------------------------------------

void Compiler::match(int t) {
	ckmatch(t);
	match();
}

void Compiler::match() {
	match1();
	if (stmtnest != 0 || binopnext())
		while (token == T_NEWLINE)
			match1();
}

bool Compiler::binopnext() {
	switch (scanner.ahead()) {
	case T_AND:
	case T_OR:
	// NOTE: not ADD or SUB because they can be unary
	case I_CAT:
	case I_MUL:
	case I_DIV:
	case I_MOD:
	case I_ADDEQ:
	case I_SUBEQ:
	case I_CATEQ:
	case I_MULEQ:
	case I_DIVEQ:
	case I_MODEQ:
	case I_BITAND:
	case I_BITOR:
	case I_BITXOR:
	case I_BITANDEQ:
	case I_BITOREQ:
	case I_BITXOREQ:
	case I_GT:
	case I_GTE:
	case I_LT:
	case I_LTE:
	case I_LSHIFT:
	case I_LSHIFTEQ:
	case I_RSHIFT:
	case I_RSHIFTEQ:
	case I_EQ:
	case I_IS:
	case I_ISNT:
	case I_MATCH:
	case I_MATCHNOT:
	case '?':
		return true;
	default:
		return false;
	}
}

void Compiler::matchnew(int t) {
	ckmatch(t);
	matchnew();
}

void Compiler::matchnew() {
	do
		match1();
	while (token == T_NEWLINE);
}

void Compiler::match1() {
	if (token == '{' || token == '(' || token == '[')
		++stmtnest;
	if (token == '}' || token == ')' || token == ']')
		--stmtnest;
	token = scanner.next();
}

void Compiler::ckmatch(int t) {
	if (t != (t < KEYWORDS ? token : scanner.keyword))
		syntax_error_();
}

void Compiler::syntax_error_(const char* err) const {
	// figure out the line number
	int line = 1;
	for (int i = 0; i < scanner.prev; ++i)
		if (scanner.source[i] == '\n')
			++line;

	if (!*err && token == T_ERROR)
		err = scanner.err;
	except("syntax error at line " << line << "  " << err);
}

// function ---------------------------------------------------------

Value Compiler::functionCompiler(
	short base, bool newfn, const char* gname, const char* className) {
	scanner.visitor->begin_func();
	FunctionCompiler compiler(
		scanner, token, stmtnest, base, newfn, gname, className);
	SuFunction* fn = compiler.function();
	scanner.visitor->end_func();
	token = compiler.token;
	if (className)
		fn->isMethod = true;
	return fn;
}

const bool INIT = true;

SuFunction* FunctionCompiler::function() {
	fn = new SuFunction; // need this while code is generated
	vector<char> flags;

	params(flags);

	if (token != '{')
		syntax_error_();
	body();

	mark();

	if (code.size() > SHRT_MAX)
		except("can't compile code larger than 32K");
	fn->code = dup(code, noptrs);
	fn->nc = code.size();
	fn->db = dup(db, noptrs);
	fn->nd = db.size();
	fn->literals = dup(literals);
	fn->nliterals = literals.size();
	fn->locals = dup(locals, noptrs);
	fn->nlocals = locals.size();
	fn->nparams = nparams;
	fn->ndefaults = ndefaults;
	fn->rest = rest;
	fn->src = scanner.source; // NOTE: nested functions share the source string
	if (notAllZero(flags))
		fn->flags = dup(flags, noptrs);
	return fn;
}

void FunctionCompiler::params(vector<char>& flags) {
	match('(');
	if (token == '@') {
		match();
		local(INIT);
		match(T_IDENTIFIER);
		rest = true;
		nparams = 1;
		ndefaults = 0;
	} else {
		rest = false;
		for (nparams = ndefaults = 0; token != ')'; ++nparams) {
			flags.push_back(0);
			if (token == '.') {
				match();
				flags[nparams] |= DOT;
				fn->className = dupstr(className); // needed to privatize
			}
			if (scanner.value[0] == '_') {
				++scanner.value;
				++scanner.prev;
				flags[nparams] |= DYN;
			}
			if ((flags[nparams] & DOT) && isupper(*scanner.value)) {
				char* s = dupstr(scanner.value);
				*s = tolower(*s);
				scanner.value = s;
				flags[nparams] |= PUB;
			}
			int i = param();
			if (flags[nparams] & DOT)
				// mark as used to prevent code warning
				scanner.visitor->local(scanner.prev, i, false);
			match(T_IDENTIFIER);

			if (token == I_EQ) {
				match();
				bool was_id = (token == T_IDENTIFIER);
				Value k = constant();
				if (was_id && val_cast<SuString*>(k))
					syntax_error("parameter defaults must be constants");
				verify(ndefaults == literal(k));
				++ndefaults;
			} else if (ndefaults)
				syntax_error("default parameters must come last");
			if (token != ')')
				match(',');
		}
	}
	matchnew(')');
}

uint16_t FunctionCompiler::param() {
	int before = locals.size();
	int i = local(INIT);
	if (i != before)
		syntax_error("duplicate function parameter (" << scanner.value << ")");
	return i;
}

bool FunctionCompiler::notAllZero(vector<char>& flags) {
	for (int i = 0; i < flags.size(); ++i)
		if (flags[i] != 0)
			return true;
	return false;
}

void FunctionCompiler::body() {
	compound(); // NOTE: caller must check for {
	int last = last_adr > 0 ? code[last_adr] : -1;
	if (last == I_POP)
		code[last_adr] = I_RETURN;
	else if (I_EQ_AUTO <= last && last < I_PUSH && (last & 8)) {
		code[last_adr] &= ~8; // clear POP
		emit(I_RETURN);
	} else if (last != I_RETURN && last != I_RETURN_NIL) {
		mark();
		emit(I_RETURN_NIL);
	}
}

struct Save {
	Save(bool& cur) : current(cur) {
		save = current;
	}
	~Save() {
		current = save;
	}
	bool& current;
	bool save;
};

void FunctionCompiler::block() {
	int first = locals.size();
	verify(first < BLOCK_REST);
	int np = 0;
	if (scanner.ahead() == I_BITOR) { // parameters
		match('{');
		match(I_BITOR);
		if (token == '@') {
			match();
			addLocal(scanner.value); // ensure new
			scanner.visitor->local(scanner.prev, locals.size() - 1, true);
			match(T_IDENTIFIER);
			np = BLOCK_REST;
		} else {
			// TODO: handle named and default parameters
			while (token == T_IDENTIFIER) {
				addLocal(scanner.value); // ensure new
				scanner.visitor->local(scanner.prev, locals.size() - 1, true);
				match();
				if (token == ',')
					match();
			}
			np = locals.size() - first;
		}
		if (token != I_BITOR) // i.e. |
			syntax_error_();
	}

	static int it = symnum("it");
	bool it_param = false;
	if (np == 0) {
		// create an "it" param, remove later if not used
		it_param = true;
		addLocal(it); // ensure new
		scanner.visitor->local(scanner.prev, locals.size() - 1, true);
	}
	int last = locals.size();

	int a = emit(I_BLOCK, 0, -1);
	code.push_back(first);
	int nparams_loc = code.size();
	code.push_back(np); // number of params
	bool prev_inblock = inblock;
	inblock = true; // for break & continue

	{
		Save save_it_used(it_used);
		it_used = false;
		body();

		inblock = prev_inblock;
		patch(a);

		if (it_param) {
			if (it_used)
				code[nparams_loc] = 1;
			else
				scanner.visitor->local(scanner.prev, first,
					false); // mark as used to prevent code warning
		}
	}
	// hide block parameter locals from rest of code
	for (int i = first; i < last; ++i)
		localsHide[i] = true;
}

#define NEW mem("New")

void FunctionCompiler::compound(short cont, short* pbrk) {
	match(); // NOTE: caller must check for {
	if (newfn && last_adr == -1) {
		if (scanner.keyword != K_SUPER || scanner.ahead() != '(') {
			emit(I_SUPER, 0, base);
			emit(I_CALL, MEM_SELF, NEW);
			emit(I_POP);
		}
	}
	while (token != '}')
		statement(cont, pbrk);
	match('}');
}

#define OPT_PAREN_EXPR1 \
	bool parens = (token == '('); \
	int prev_stmtnest = stmtnest; \
	if (parens) \
		match('(');

#define OPT_PAREN_EXPR2 \
	if (!parens) { \
		stmtnest = 0; /* enable newlines */ \
		expecting_compound = true; \
	} \
	expr(); \
	if (parens) \
		match(')'); \
	else if (token == T_NEWLINE) \
		match(); \
	stmtnest = prev_stmtnest; \
	expecting_compound = false;

void FunctionCompiler::opt_paren_expr() {
	OPT_PAREN_EXPR1
	OPT_PAREN_EXPR2
}

#define NEXT mem("Next")
#define ITER mem("Iter")

void FunctionCompiler::statement(short cont, short* pbrk) {
	short a, b, c;

	a = code.size(); // before mark to include nop
	mark();

	while (token == T_NEWLINE || token == T_WHITE || token == T_COMMENT)
		match();
	switch (scanner.keyword) {
	case K_IF:
		b = -1;
		for (;;) {
			match();
			opt_paren_expr();
			a = emit(I_JUMP, POP_NO, -1);
			statement(cont, pbrk);
			if (scanner.keyword == K_ELSE) {
				b = emit(I_JUMP, UNCOND, b);
				patch(a);
				mark();
				match();
				if (scanner.keyword == K_IF)
					continue;
				statement(cont, pbrk);
			} else
				patch(a);
			break;
		}
		patch(b);
		break;
	case K_FOREVER:
		b = -1;
		match();
		statement(a, &b);
		emit(I_JUMP, UNCOND, a - (code.size() + 3));
		patch(b);
		break;
	case K_WHILE:
		match();
		opt_paren_expr();
		b = emit(I_JUMP, POP_NO, -1);
		statement(a, &b);
		emit(I_JUMP, UNCOND, a - (code.size() + 3));
		patch(b);
		break;
	case K_DO: {
		b = -1;
		match();
		short skip = emit(I_JUMP, UNCOND, -1);
		c = emit(I_JUMP, UNCOND, -1);
		patch(skip);
		short body = code.size();
		statement(c, &b);
		patch(c);
		mark();
		match(K_WHILE);
		opt_paren_expr();
		if (token == ';' || token == T_NEWLINE)
			match();
		emit(I_JUMP, POP_YES, body - (code.size() + 3));
		patch(b);
		break;
	}
	case K_FOR: {
		b = -1;
		match();
		OPT_PAREN_EXPR1
		if (token == T_IDENTIFIER &&
			scanner.ahead() == K_IN) { // for var in expr
			int var = local(INIT);
			match(T_IDENTIFIER);
			match(K_IN);
			last_adr = -1;
			OPT_PAREN_EXPR2
			emit(I_CALL, MEM, ITER);
			a = code.size();
			emit(I_DUP); // to save the iterator
			emit(I_DUP); // to compare against for end
			emit(I_CALL, MEM, NEXT);
			emit(I_EQ, 0x80 + (AUTO << 4), var);
			emit(I_ISNT);
			b = emit(I_JUMP, POP_NO, -1);
			statement(a, &b);
			emit(I_JUMP, UNCOND, a - (code.size() + 3));
			patch(b);
			emit(I_POP);
			last_adr = -1; // prevent POP from being optimized away
			break;
		}
		if (!parens)
			syntax_error("parenthesis required: for (expr; expr; expr)");
		if (token != ';') {
			// INITIALIZATION
			exprlist();
			emit(I_POP);
		}
		match(';');
		short aa = code.size();
		short test_nc = 0;
		if (token != ';') {
			// TEST
			test_nc = code.size();
			expr();
			// cut out the test code
			if (code.size() - test_nc >= maxtest)
				syntax_error("for loop test too large");
			copy(code.begin() + test_nc, code.end(), test);
			int tmp = code.size();
			code.erase(code.begin() + test_nc, code.end());
			test_nc = tmp - test_nc;
		}
		match(';');
		if (token != ')') {
			// INCREMENT
			c = emit(I_JUMP, UNCOND, -1); // skip over the increment
			aa = code.size();
			exprlist();
			emit(I_POP);
			patch(c);
		}
		match(')');
		// paste the test code back in after the increment
		if (test_nc) {
			code.insert(code.end(), test, test + test_nc);
			b = emit(I_JUMP, POP_NO, b);
		}
		// BODY
		if (a != code.size() - 1)
			a = aa;
		statement(a, &b);
		emit(I_JUMP, UNCOND, a - (code.size() + 3));
		patch(b);
		break;
	}
	case K_CONTINUE:
		match();
		if (token == ';')
			match();
		if (cont >= 0)
			emit(I_JUMP, UNCOND, cont - (code.size() + 3));
		else if (inblock) {
			static Value con("block:continue");
			emit(I_PUSH, LITERAL, literal(con));
			emit(I_THROW);
		} else
			syntax_error_();
		break;
	case K_BREAK:
		match();
		if (token == ';')
			match();
		if (pbrk)
			*pbrk = emit(I_JUMP, UNCOND, *pbrk);
		else if (inblock) {
			static Value brk("block:break");
			emit(I_PUSH, LITERAL, literal(brk));
			emit(I_THROW);
		} else
			syntax_error_();
		break;
	case K_RETURN:
		match1(); // don't discard newline
		if (inblock) {
			switch (token) {
			case ';':
			case T_NEWLINE:
				match();
				[[fallthrough]];
			case '}':
				emit(I_PUSH, LITERAL, literal(Value()));
				break;
			default:
				stmtexpr();
			}
			static Value ret("block return");
			emit(I_PUSH, LITERAL, literal(ret));
			emit(I_THROW);
		} else
			switch (token) {
			case ';':
			case T_NEWLINE:
				match();
				[[fallthrough]];
			case '}':
				emit(I_RETURN_NIL);
				break;
			default:
				stmtexpr();
				emit(I_RETURN);
			}
		break;
	case K_SWITCH:
		a = -1;
		match();
		if (token == '{')
			emit(I_PUSH_VALUE, TRUE);
		else
			opt_paren_expr();
		match('{');
		while (scanner.keyword == K_CASE) {
			mark();
			match();
			b = -1;
			for (;;) {
				expr();
				if (token == ',') {
					b = emit(I_JUMP, CASE_YES, b);
					match();
				} else {
					c = emit(I_JUMP, CASE_NO, -1);
					break;
				}
			}
			match(':');
			patch(b);
			while (scanner.keyword != K_CASE && scanner.keyword != K_DEFAULT &&
				token != '}')
				statement(cont, pbrk);
			a = emit(I_JUMP, UNCOND, a);
			patch(c);
		}
		if (scanner.keyword == K_DEFAULT) {
			mark();
			match();
			match(':');
			emit(I_POP);
			while (token != '}')
				statement(cont, pbrk);
			a = emit(I_JUMP, UNCOND, a);
		} else if (getSystemOption("SwitchUnhandledThrow", true)) {
			mark();
			emit(I_POP);
			emit(I_PUSH, LITERAL, literal("unhandled switch value"));
			emit(I_THROW);
		}
		match('}');
		emit(I_POP);
		patch(a);
		break;
	case K_TRY: {
		if (inside_try)
			syntax_error("nested try-catch is not supported on cSuneido");
		match();
		a = emit(I_TRY, 0, -1);
		int catchvalue = literal(0);
		emit_target(LITERAL, catchvalue);
		inside_try = true;
		statement(cont, pbrk); // try code
		inside_try = false;
		mark();
		b = emit(I_CATCH, 0, -1);
		patch(a);
		Value value = SuEmptyString;
		if (scanner.keyword == K_CATCH) {
			match();
			if (token == '(') {
				match('(');
				if (token != ')') {
					uint16_t exception = local(INIT);
					match(T_IDENTIFIER);
					if (token == ',') {
						match();
						if (token == T_STRING)
							value = new SuString(scanner.value, scanner.len);
						match(T_STRING);
					}
					emit(I_EQ, 0x80 + (AUTO << 4), exception);
				}
				match(')');
			}
			emit(I_POP);
			statement(cont, pbrk); // catch code
		} else {
			emit(I_POP);
		}
		literals[catchvalue] = value;
		patch(b);
		break;
	}
	case K_THROW:
		match();
		stmtexpr();
		emit(I_THROW);
		break;
	default:
		if (token == ';')
			match();
		else if (token == '{')
			compound(cont, pbrk);
		else {
			stmtexpr();
			emit(I_POP);
		}
	}
}

void FunctionCompiler::stmtexpr() {
	int prev_stmtnest = stmtnest;
	stmtnest = 0; // enable newlines
	expr();
	stmtnest = prev_stmtnest;
	if (token == ';' || token == T_NEWLINE)
		match();
	else if (token != '}' && scanner.keyword != K_CATCH &&
		scanner.keyword != K_WHILE && scanner.keyword != K_ELSE)
		syntax_error_();
}

void FunctionCompiler::exprlist() { // used by for
	expr();
	while (token == ',') {
		match();
		emit(I_POP);
		expr();
	}
}

void FunctionCompiler::triop() {
	orop();
	if (token == '?') {
		++stmtnest;
		match();
		short a = emit(I_JUMP, POP_NO, -1);
		triop();
		match(':');
		--stmtnest;
		short b = emit(I_JUMP, UNCOND, -1);
		patch(a);
		triop();
		patch(b);
	}
}

void FunctionCompiler::orop() {
	andop();
	short a = -1;
	while (token == T_OR) {
		matchnew();
		a = emit(I_JUMP, ELSE_POP_YES, a);
		andop();
		if (token != T_OR)
			emit(I_BOOL);
	}
	patch(a);
}

void FunctionCompiler::andop() {
	inop();
	short a = -1;
	while (token == T_AND) {
		matchnew();
		a = emit(I_JUMP, ELSE_POP_NO, a);
		inop();
		if (token != T_AND)
			emit(I_BOOL);
	}
	patch(a);
}

void FunctionCompiler::inop() {
	bitorop();
	int op = token;
	if (isIn()) {
		matchnew();
		match('(');
		short a = -1;
		while (token != ')') {
			expr();
			a = emit(I_JUMP, CASE_YES, a);
			if (token == ',')
				match(',');
		}
		emit(I_POP);
		emit(I_PUSH_VALUE, op == I_NOT);
		short b = emit(I_JUMP, UNCOND, -1);
		patch(a);
		emit(I_PUSH_VALUE, op != I_NOT);
		patch(b);
		match(')');
	}
}

bool FunctionCompiler::isIn() {
	if (token == I_NOT && scanner.ahead() == K_IN)
		match();
	return scanner.keyword == K_IN;
}

void FunctionCompiler::bitorop() {
	bitxor();
	while (token == I_BITOR) {
		short t = token;
		matchnew();
		bitxor();
		emit(t);
	}
}

void FunctionCompiler::bitxor() {
	bitandop();
	while (token == I_BITXOR) {
		short t = token;
		matchnew();
		bitandop();
		emit(t);
	}
}

void FunctionCompiler::bitandop() {
	isop();
	while (token == I_BITAND) {
		short t = token;
		matchnew();
		isop();
		emit(t);
	}
}

void FunctionCompiler::isop() {
	cmpop();
	while (I_IS <= token && token <= I_MATCHNOT) {
		short t = token;
		matchnew();
		cmpop();
		emit(t);
	}
}

void FunctionCompiler::cmpop() {
	shift();
	while (token == I_LT || token == I_LTE || token == I_GT || token == I_GTE) {
		short t = token;
		matchnew();
		shift();
		emit(t);
	}
}

void FunctionCompiler::shift() {
	addop();
	while (token == I_LSHIFT || token == I_RSHIFT) {
		short t = token;
		matchnew();
		addop();
		emit(t);
	}
}

void FunctionCompiler::addop() {
	mulop();
	while (token == I_ADD || token == I_SUB || token == I_CAT) {
		short t = token;
		matchnew();
		mulop();
		emit(t);
	}
}

void FunctionCompiler::mulop() {
	unop();
	while (token == I_MUL || token == I_DIV || token == I_MOD) {
		short t = token;
		matchnew();
		unop();
		emit(t);
	}
}

#define INSTANTIATE mem("instantiate")

void FunctionCompiler::unop() {
	if (token == I_NOT || token == I_ADD || token == I_SUB ||
		token == I_BITNOT) {
		short t = token;
		match();
		unop();
		if (t == I_ADD)
			t = I_UPLUS;
		if (t == I_SUB)
			t = I_UMINUS;
		emit(t);
	} else if (scanner.keyword == K_NEW) {
		match();
		expr0(true);
		short nargs = 0;
		vector<short> argnames;
		if (token == '(')
			args(nargs, argnames);
		emit(I_CALL, MEM, INSTANTIATE, nargs, &argnames);
	} else
		expr0();
}

short FunctionCompiler::emit_literal() {
	return emit(I_PUSH, LITERAL, literal(constant()));
}

void FunctionCompiler::expr0(bool newtype) {
	bool lvalue = true;
	bool value = true;
	short option;
	short id = -1;
	short incdec = 0;
	int local_pos = -1;
	if (token == I_PREINC || token == I_PREDEC) {
		incdec = token;
		matchnew();
	}
	bool super = false;
	switch (token) {
	case T_STRING: {
		Value x = (scanner.source[scanner.prev] == '#')
			? symbol(scanner.value)
			: (scanner.len == 0) ? SuEmptyString
								 : new SuString(scanner.value, scanner.len);
		match();
		emit(I_PUSH, LITERAL, literal(x));
		option = LITERAL;
		lvalue = value = false;
		break;
	}
	case T_NUMBER:
	case '#':
		emit_literal();
		option = LITERAL;
		lvalue = value = false;
		break;
	case '{': // block
		block();
		option = LITERAL;
		lvalue = value = false;
		break;
	case T_IDENTIFIER:
		switch (scanner.keyword) {
		case K_FUNCTION:
		case K_CLASS:
		case K_DLL:
		case K_STRUCT:
		case K_CALLBACK:
			emit_literal();
			option = LITERAL;
			lvalue = value = false;
			break;
		case K_SUPER:
			match();
			super = true;
			if (incdec || base < 0)
				syntax_error_();
			if (last_adr == 0 // not -1 because mark() has generated NOP
				&& newfn && token == '(') {
				// only allow super(...) at start of new function
				option = MEM_SELF;
				id = NEW;
			} else {
				match('.');
				option = MEM_SELF;
				id = mem(scanner.value);
				match(T_IDENTIFIER);
			}
			break;
		default:
			if (isupper(scanner.value[*scanner.value == '_' ? 1 : 0]) &&
				'{' ==
					(expecting_compound ? scanner.aheadnl()
										: scanner.ahead())) { // Name { => class
				emit_literal();
				option = LITERAL;
				lvalue = value = false;
			} else {
				option = *scanner.value == '_'
					? isupper(scanner.value[1]) ? LITERAL : DYNAMIC
					: isupper(scanner.value[0]) ? GLOBAL : AUTO;
				if (option == GLOBAL) {
					lvalue = false;
					id = globals(scanner.value);
					scanner.visitor->global(scanner.prev, scanner.value);
				} else if (option == LITERAL) { // _Name
					// check if name is the name of what we're compiling
					if (!gname || 0 != strcmp(gname, scanner.value + 1))
						syntax_error("invalid reference to " << scanner.value);
					Value x = globals.get(scanner.value + 1);
					if (!x)
						syntax_error("can't find " << scanner.value);
					scanner.visitor->global(scanner.prev, scanner.value);
					emit(I_PUSH, LITERAL, literal(x));
					lvalue = value = false;
				} else { // AUTO or DYNAMIC
					switch (scanner.keyword) {
					case K_THIS:
						emit(I_PUSH_VALUE, SELF);
						lvalue = value = false;
						break;
					case K_TRUE:
					case K_FALSE:
						emit(I_PUSH, LITERAL,
							literal(
								scanner.keyword == K_TRUE ? SuTrue : SuFalse));
						lvalue = value = false;
						break;
					default:
						local_pos = scanner.prev;
						id = local();
					}
				}
				match();
			}
			break;
		}
		break; // end of IDENTIFIER
	case '.':
		matchnew('.');
		option = MEM_SELF;
		id = literal(memname(className, scanner.value));
		match(T_IDENTIFIER);
		if (!expecting_compound && token == T_NEWLINE && scanner.ahead() == '{')
			match();
		break;
	case '[':
		record();
		option = LITERAL;
		lvalue = value = super = false;
		break;
	case '(':
		match('(');
		expr();
		match(')');
		option = LITERAL;
		lvalue = value = false;
		break;
	default:
		syntax_error_();
	}
	while (token == '.' || token == '[' || token == '(' ||
		(token == '{' && !expecting_compound)) {
		if (value && (token == '.' || token == '[')) {
			emit(I_PUSH, option, id);
			option = -1;
		}
		if (newtype && token == '(') {
			if (value)
				emit(I_PUSH, option, id);
			return;
		}
		lvalue = value = true;
		if (token == '.') {
			matchnew();
			option = MEM;
			id = mem(scanner.value);
			match(T_IDENTIFIER);
			if (!expecting_compound && token == T_NEWLINE &&
				scanner.ahead() == '{')
				match();
		} else if (token == '[') {
			static Value intmax(INT_MAX);
			match('[');
			if (token == T_RANGETO || token == T_RANGELEN)
				emit(I_PUSH, LITERAL, literal(SuZero));
			else
				expr();
			if (token == T_RANGETO || token == T_RANGELEN) {
				int type = token;
				match();
				if (token == ']')
					emit(I_PUSH, LITERAL, literal(intmax));
				else
					expr();
				static int g_rangeTo = globals("rangeTo");
				static int g_rangeLen = globals("rangeLen");
				vector<short> argnames;
				emit(I_CALL, GLOBAL, type == T_RANGETO ? g_rangeTo : g_rangeLen,
					3, &argnames);
				option = LITERAL;
				lvalue = value = super = false;
			} else
				option = SUB;
			match(']');
		} else if (token == '(' || // function call
			token == '{') {        // func { } == func() { } == func({ })
			static Value NEWVAL("New");
			short nargs = 0;
			vector<short> argnames;
			args(nargs, argnames);
			if (super)
				emit(I_SUPER, 0, base);
			else if ((option == MEM || option == MEM_SELF) &&
				literals[id] == NEWVAL)
				syntax_error("cannot explicitly call New method");
			emit(I_CALL, option, id, nargs, &argnames);
			option = LITERAL;
			lvalue = value = super = false;
		} else
			break;
	}
	if (incdec) {
		if (!lvalue)
			syntax_error_();
		emit(incdec, 0x80 + (option << 4), id);
	} else if (I_ADDEQ <= token && token <= I_EQ) {
		int t = token;
		if (!lvalue)
			syntax_error_();
		bool fixup = token == I_EQ && local_pos != -1 &&
			(option == AUTO || option == DYNAMIC);
		if (fixup)
			scanner.visitor->local(
				-1, id, false); // undo previous incorrect "usage"
		matchnew();
		if (scanner.keyword == K_FUNCTION || scanner.keyword == K_CLASS) {
			if (t != I_EQ)
				syntax_error_();
			Value k = constant();
			Named* n = const_cast<Named*>(k.get_named());
			verify(n);
			n->parent = &fn->named;
			if (option == AUTO || option == DYNAMIC)
				n->num = locals[id];
			else if (option == MEM || option == MEM_SELF)
				n->str = literals[id].str();
			emit(I_PUSH, LITERAL, literal(k));
		} else
			expr();
		if (fixup)
			scanner.visitor->local(local_pos, id, INIT);
		emit(t, 0x80 + (option << 4), id);
	} else if (token == I_PREINC || token == I_PREDEC) {
		if (!lvalue)
			syntax_error_();
		emit(token + 2, 0x80 + (option << 4), id);
		match();
	} else if (value) {
		emit(I_PUSH, option, id);
	}
}

void FunctionCompiler::args(
	short& nargs, vector<short>& argnames, const char* delims) {
	nargs = 0;
	bool just_block = (token == '{');
	if (!just_block)
		match(delims[0]);
	if (token == '@')
		args_at(nargs, delims);
	else {
		if (!just_block)
			args_list(nargs, delims, argnames);
		while (
			token == T_NEWLINE && !expecting_compound && scanner.ahead() == '{')
			match();
		if (token == '{') { // take block following args as another arg
			static short n_block = symnum("block");
			argnames.push_back(n_block);
			block();
			++nargs;
		}
	}
}

void FunctionCompiler::args_at(short& nargs, const char* delims) {
	match();
	short each = 0;
	if (token == I_ADD) {
		match();
		each = strtoul(scanner.value, NULL, 0);
		match(T_NUMBER);
	}
	expr();
	emit(I_EACH, 0, each);
	nargs = 1;
	match(delims[1]);
}

void FunctionCompiler::args_list(
	short& nargs, const char* delims, vector<short>& argnames) {
	bool key = false;
	for (nargs = 0; token != delims[1]; ++nargs) {
		if (token == ':') {
			key = true;
			keywordArgShortcut(argnames);
		} else {
			if (isKeyword()) {
				key = true;
				int id;
				if (anyName())
					id = symnum(scanner.value);
				else if (token == T_NUMBER) {
					id = strtoul(scanner.value, NULL, 0); // FIXME check end
					if (id >= 0x8000)
						syntax_error("numeric subscript overflow: ("
							<< scanner.value << ")");
				} else
					syntax_error_();
				add_argname(argnames, id);
				match();
				match();
			} else if (key)
				syntax_error(
					"un-named arguments must come before named arguments");
			if (key && (token == ',' || token == delims[1] || isKeyword()))
				emit(I_PUSH, LITERAL, literal(SuTrue));
			else
				expr();
		}
		if (token == ',')
			match();
	}
	match(delims[1]);
}

void FunctionCompiler::keywordArgShortcut(vector<short>& argnames) {
	// f(:name) is equivalent to f(name: name)
	match(':');
	if (!just_name())
		syntax_error_();
	add_argname(argnames, symnum(scanner.value));
	expr0();
}

bool FunctionCompiler::isKeyword() {
	return (anyName() || token == T_NUMBER) && scanner.ahead() == ':';
}

bool FunctionCompiler::just_name() {
	// only checks enough to call expr0
	if (token != T_IDENTIFIER)
		return false;
	switch (scanner.ahead()) {
	case '.':
	case '(':
	case '[':
	case '{':
		return false;
	default:
		return true;
	}
}

void FunctionCompiler::add_argname(vector<short>& argnames, int id) {
	if (find(argnames.begin(), argnames.end(), id) != argnames.end())
		syntax_error("duplicate argument name: " << symstr(id));
	argnames.push_back(id);
}

void FunctionCompiler::record() {
	SuRecord* rec = 0;
	vector<short> argnames;
	short nargs;
	match('[');
	bool key = false;
	int argi = 0;
	for (nargs = 0; token != ']'; ++nargs, ++argi) {
		if (token == ':') {
			key = true;
			keywordArgShortcut(argnames);
		} else {
			if (scanner.ahead() == ':') {
				key = true;
				int id;
				if (token == T_IDENTIFIER || token == T_STRING)
					id = symnum(scanner.value);
				else if (token == T_NUMBER) {
					id = strtoul(scanner.value, NULL, 0); // FIXME check end
					if (id >= 0x8000)
						syntax_error("numeric subscript overflow: ("
							<< scanner.value << ")");
				} else
					syntax_error_();
				add_argname(argnames, id);
				match();
				match();
			} else if (key)
				syntax_error_();
			prevlits.clear();
			if (key && (scanner.ahead() == ':' || token == ',' || token == ']'))
				emit(I_PUSH, LITERAL, literal(SuTrue));
			else
				expr();
			if (prevlits.size() > 0) {
				PrevLit prev = poplits();
				if (!rec)
					rec = new SuRecord;
				if (!key)
					rec->put(argi, prev.lit);
				else {
					rec->put(symbol(argnames.back()), prev.lit);
					argnames.pop_back();
				}
				--nargs;
			}
		}
		if (token == ',')
			match();
	}
	match(']');
	if (rec) {
		emit(I_PUSH, LITERAL, literal(rec));
		static int litrec = symnum("litrec");
		if (argnames.size() > 0)
			argnames.push_back(litrec);
		static int gMkRec = globals("mkrec");
		emit(I_CALL, GLOBAL, gMkRec, nargs + 1, &argnames);
	} else {
		static int gRecord = globals("Record");
		emit(I_CALL, GLOBAL, gRecord, nargs, &argnames);
	}
}

static Value symbolOrString(const char* s) {
	Value m = symbol_existing(s);
	return m ? m : new SuString(s);
}

uint16_t FunctionCompiler::mem(const char* s) {
	return literal(symbolOrString(s));
}

// TODO use "reuse" in more places
uint16_t FunctionCompiler::literal(Value x, bool reuse) {
	if (reuse)
		for (int i = 0; i < literals.size(); ++i)
			if (literals[i] == x)
				return i;
	except_if(literals.size() >= USHRT_MAX, "too many literals");
	literals.push_back(x);
	return literals.size() - 1;
}

uint16_t FunctionCompiler::local(bool init) {
	short num = symnum(scanner.value);

	// for blocks
	static short it_num = symnum("it");
	if (0 == strcmp("_", scanner.value))
		num = it_num;
	if (num == it_num)
		it_used = true;

	for (int i = locals.size() - 1; i >= 0; --i)
		if (locals[i] == num && !localsHide[i]) {
			scanner.visitor->local(scanner.prev, i, init);
			return i;
		}
	except_if(locals.size() >= UCHAR_MAX, "too many local variables");
	addLocal(num);
	if (*scanner.value == '_')
		scanner.visitor->dynamic(locals.size() - 1);
	else
		scanner.visitor->local(scanner.prev, locals.size() - 1, init);
	return locals.size() - 1;
}

void FunctionCompiler::addLocal(const char* name) {
	addLocal(symnum(name));
}

void FunctionCompiler::addLocal(int sym) {
	locals.push_back(sym);
	localsHide.push_back(false);
}

PrevLit FunctionCompiler::poplits() {
	PrevLit prev = prevlits.back();
	prevlits.pop_back();
	if (prev.il == literals.size() - 1)
		literals.pop_back();
	code.resize(prev.adr);
	return prev;
}

short FunctionCompiler::emit(short op, short option, short target, short nargs,
	vector<short>* argnames) {
	short adr;

	// merge pop with previous instruction where possible
	if (last_adr >= 0 && op == I_POP) {
		adr = last_adr;
		short last_op = code[last_adr] & 0xf0;
		if (I_EQ_AUTO <= last_op && last_op < I_PUSH && !(last_op & 8)) {
			code[last_adr] += 8;
			return adr;
		}
	}

	// constant folding e.g. replace 2 * 4 with 8
	if (!prevlits.empty() &&
		(op == I_UPLUS || op == I_UMINUS || op == I_NOT || op == I_BITNOT)) {
		Value prev = poplits().lit;
		Value result;
		if (op == I_UPLUS)
			result = +prev;
		else if (op == I_UMINUS)
			result = -prev;
		else if (op == I_NOT)
			result = prev.toBool() ? SuFalse : SuTrue;
		else if (op == I_BITNOT)
			result = ~prev.integer();
		op = I_PUSH;
		option = LITERAL;
		target = literal(result);
	} else if (prevlits.size() >= 2 && I_ADD <= op && op <= I_BITXOR) {
		PrevLit prev2 = poplits();
		PrevLit prev1 = poplits();
		Value result;
		switch (op) {
		case I_ADD:
			result = prev1.lit + prev2.lit;
			break;
		case I_SUB:
			result = prev1.lit - prev2.lit;
			break;
		case I_MUL:
			result = prev1.lit * prev2.lit;
			break;
		case I_DIV:
			result = prev1.lit / prev2.lit;
			break;
		case I_CAT:
			result = new SuString(prev1.lit.to_gcstr() + prev2.lit.to_gcstr());
			break;
		case I_MOD:
			result = prev1.lit.integer() % prev2.lit.integer();
			break;
		case I_LSHIFT:
			result = (uint32_t) prev1.lit.integer() << prev2.lit.integer();
			break;
		case I_RSHIFT:
			result = (uint32_t) prev1.lit.integer() >> prev2.lit.integer();
			break;
		case I_BITAND:
			result =
				(uint32_t) prev1.lit.integer() & (uint32_t) prev2.lit.integer();
			break;
		case I_BITOR:
			result =
				(uint32_t) prev1.lit.integer() | (uint32_t) prev2.lit.integer();
			break;
		case I_BITXOR:
			result =
				(uint32_t) prev1.lit.integer() ^ (uint32_t) prev2.lit.integer();
			break;
		default:
			unreachable();
		}
		op = I_PUSH;
		option = LITERAL;
		target = literal(result);
	}
	if (op == I_PUSH && option == LITERAL) {
		Value x = literals[target];
		prevlits.push_back(PrevLit(code.size(), x, target));
		int lit = target;
		if (x == SuOne) {
			op = I_PUSH_VALUE;
			option = ONE;
		} else if (x == SuZero) {
			op = I_PUSH_VALUE;
			option = ZERO;
		} else if (x == SuEmptyString) {
			op = I_PUSH_VALUE;
			option = EMPTY_STRING;
		} else if (x == SuTrue) {
			op = I_PUSH_VALUE;
			option = TRUE;
		} else if (x == SuFalse) {
			op = I_PUSH_VALUE;
			option = FALSE;
		} else if (x.is_int()) {
			op = I_PUSH_INT;
			target = x.integer();
		}
		if (op != I_PUSH && lit == literals.size() - 1) {
			literals.pop_back();
			prevlits.back().il = -99;
		}
	} else
		prevlits.clear();

	last_adr = adr = code.size();
	code.push_back(op | option);

	if (op == I_PUSH) {
		// OPTIMIZE: pushes
		if (option == LITERAL && target < 16)
			code[adr] = I_PUSH_LITERAL | target;
		else if (option == AUTO && target < 15)
			code[adr] = I_PUSH_AUTO | target;
		else if (option >= LITERAL)
			emit_target(option, target);
	}

	else if (op == I_CALL) {
		if (option > LITERAL) // literal means stack
			emit_target(option, target);

		// OPTIMIZE: calls
		int nargnames = nargs && argnames ? argnames->size() : 0;
		if (option == GLOBAL && nargs < 8 && nargnames == 0)
			code[adr] = I_CALL_GLOBAL | nargs;
		else if (option == MEM && nargs < 8 && nargnames == 0)
			code[adr] = I_CALL_MEM | nargs;
		else if (option == MEM_SELF && nargs < 8 && nargnames == 0)
			code[adr] = I_CALL_MEM_SELF | nargs;
		else {
			code.push_back(nargs);
			code.push_back(nargnames);
			for (short i = 0; i < nargnames; ++i) {
				code.push_back((*argnames)[i]);
				code.push_back((*argnames)[i] >> 8);
			}
		}
	}

	else if (op == I_JUMP || op == I_TRY || op == I_CATCH || op == I_SUPER ||
		op == I_BLOCK || op == I_PUSH_INT) {
		code.push_back(target & 255);
		code.push_back(target >> 8);
		// I_TRY is followed by literal index
	}

	else if (I_ADDEQ <= op && op <= I_POSTDEC) {
		option = ((option & 0x70) >> 4);
		if (op == I_EQ && option == AUTO && target < 8)
			code[adr] = I_EQ_AUTO | target;
		else if (option >= LITERAL)
			emit_target(option, target);
	}

	else if (op == I_EACH)
		code.push_back(target);

	return adr;
}

void FunctionCompiler::emit_target(int option, int target) {
	if (option == LITERAL || option == MEM || option == MEM_SELF)
		push_varint(code, target);
	else {
		code.push_back(target & 255);
		if (option == GLOBAL) // TODO use varint
			code.push_back(target >> 8);
	}
}

void FunctionCompiler::mark() {
	if (db.size() > 0 && db[db.size() - 1].ci + 1 == code.size())
		return;
	db.push_back(Debug(scanner.prev, code.size()));
	emit(I_NOP);
}

#define TARGET(i) (short) ((code[(i) + 1] + (code[(i) + 2] << 8)))

void FunctionCompiler::patch(short i) {
	short adr, next;
	for (; i >= 0; i = next) {
		verify(i < code.size());
		next = TARGET(i);
		adr = code.size() - (i + 3);
		code[i + 1] = adr & 255;
		code[i + 2] = adr >> 8;
		last_adr = -1; // prevent optimizations
		prevlits.clear();
	}
}

// make lower case member names private by prefixing with class name
Value Compiler::memname(const char* className, const char* s) {
	if (className && islower(s[0])) {
		if (has_prefix(s, "get_"))
			s = CATSTR3("Get_", className, s + 3); // get_name => Get_Class_name
		else
			s = CATSTR3(className, "_", s);
	}
	return symbolOrString(s);
}

#include "testing.h"
#include "except.h"
#include "exceptimp.h"

struct Cmpltest {
	const char* query;
	const char* result;
};

static Cmpltest cmpltests[] = {{"123;", "123; }\n\
					  0  nop \n\
					  1  push int 123\n\
					  4  return \n\
					  5\n"},

	{"0x100;", "0x100; }\n\
					  0  nop \n\
					  1  push int 256\n\
					  4  return \n\
					  5\n"},

	{"0x7fff;", "0x7fff; }\n\
					  0  nop \n\
					  1  push int 32767\n\
					  4  return \n\
					  5\n"},

	{"0x10000;", "0x10000; }\n\
					  0  nop \n\
					  1  push literal 65536\n\
					  2  return \n\
					  3\n"},

	{"0.001;", "0.001; }\n\
					  0  nop \n\
					  1  push literal .001\n\
					  2  return \n\
					  3\n"},

	{"a;", "a; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  return \n\
					  3\n"},

	{"a = 123;", "a = 123; }\n\
					  0  nop \n\
					  1  push int 123\n\
					  4  = auto a\n\
					  5  return \n\
					  6\n"},

	{"a = b = c;", "a = b = c; }\n\
					  0  nop \n\
					  1  push auto c\n\
					  2  = auto b\n\
					  3  = auto a\n\
					  4  return \n\
					  5\n"},

	{"F();", "F(); }\n\
					  0  nop \n\
					  1  call global F 0\n\
					  4  return \n\
					  5\n"},

	{"a.b();", "a.b(); }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  call mem b 0\n\
					  4  return \n\
					  5\n"},

	{".b();", ".b(); }\n\
					  0  nop \n\
					  1  call mem this b 0\n\
					  3  return \n\
					  4\n"},

	{"a[b];", "a[b]; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  push sub \n\
					  4  return \n\
					  5\n"},

	{"a.b;", "a.b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push mem b\n\
					  4  return \n\
					  5\n"},

	{".b;", ".b; }\n\
					  0  nop \n\
					  1  push mem this b\n\
					  3  return \n\
					  4\n"},

	{"X;", "X; }\n\
					  0  nop \n\
					  1  push global X\n\
					  4  return \n\
					  5\n"},

	{"\"\";", "\"\"; }\n\
					  0  nop \n\
					  1  push value \"\" \n\
					  2  return \n\
					  3\n"},

	{"this;", "this; }\n\
					  0  nop \n\
					  1  push value this \n\
					  2  return \n\
					  3\n"},

	{"0;", "0; }\n\
					  0  nop \n\
					  1  push value 0 \n\
					  2  return \n\
					  3\n"},

	{"1;", "1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  return \n\
					  3\n"},

	{"x = 1;", "x = 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  = auto x\n\
					  3  return \n\
					  4\n"},

	{"x += 1;", "x += 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  += auto x\n\
					  4  return \n\
					  5\n"},

	{"x -= 1;", "x -= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  -= auto x\n\
					  4  return \n\
					  5\n"},

	{"x $= \"\";", "x $= \"\"; }\n\
					  0  nop \n\
					  1  push value \"\" \n\
					  2  $= auto x\n\
					  4  return \n\
					  5\n"},

	{"x *= 1;", "x *= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  *= auto x\n\
					  4  return \n\
					  5\n"},

	{"x /= 1;", "x /= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  /= auto x\n\
					  4  return \n\
					  5\n"},

	{"x %= 1;", "x %= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  %= auto x\n\
					  4  return \n\
					  5\n"},

	{"x &= 1;", "x &= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  &= auto x\n\
					  4  return \n\
					  5\n"},

	{"x |= 1;", "x |= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  |= auto x\n\
					  4  return \n\
					  5\n"},

	{"x ^= 1;", "x ^= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  ^= auto x\n\
					  4  return \n\
					  5\n"},

	{"x <<= 1;", "x <<= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  <<= auto x\n\
					  4  return \n\
					  5\n"},

	{"x >>= 1;", "x >>= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  >>= auto x\n\
					  4  return \n\
					  5\n"},

	{"++x;", "++x; }\n\
					  0  nop \n\
					  1  ++? auto x\n\
					  3  return \n\
					  4\n"},

	{"--x;", "--x; }\n\
					  0  nop \n\
					  1  --? auto x\n\
					  3  return \n\
					  4\n"},

	{"x++;", "x++; }\n\
					  0  nop \n\
					  1  ?++ auto x\n\
					  3  return \n\
					  4\n"},

	{"x--;", "x--; }\n\
					  0  nop \n\
					  1  ?-- auto x\n\
					  3  return \n\
					  4\n"},

	{".x = 1;", ".x = 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  = mem this x\n\
					  4  return \n\
					  5\n"},

	{".x += 1;", ".x += 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  += mem this x\n\
					  4  return \n\
					  5\n"},

	{".x -= 1;", ".x -= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  -= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x $= \"\";", ".x $= \"\"; }\n\
					  0  nop \n\
					  1  push value \"\" \n\
					  2  $= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x *= 1;", ".x *= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  *= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x /= 1;", ".x /= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  /= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x %= 1;", ".x %= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  %= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x &= 1;", ".x &= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  &= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x |= 1;", ".x |= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  |= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x ^= 1;", ".x ^= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  ^= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x <<= 1;", ".x <<= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  <<= mem this x\n\
					  4  return \n\
					  5\n"},

	{".x >>= 1;", ".x >>= 1; }\n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  >>= mem this x\n\
					  4  return \n\
					  5\n"},

	{"++.x;", "++.x; }\n\
					  0  nop \n\
					  1  ++? mem this x\n\
					  3  return \n\
					  4\n"},

	{"--.x;", "--.x; }\n\
					  0  nop \n\
					  1  --? mem this x\n\
					  3  return \n\
					  4\n"},

	{".x++;", ".x++; }\n\
					  0  nop \n\
					  1  ?++ mem this x\n\
					  3  return \n\
					  4\n"},

	{".x--;", ".x--; }\n\
					  0  nop \n\
					  1  ?-- mem this x\n\
					  3  return \n\
					  4\n"},

	{"a + b;", "a + b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  + \n\
					  4  return \n\
					  5\n"},

	{"a - b;", "a - b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  - \n\
					  4  return \n\
					  5\n"},

	{"a $ b;", "a $ b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  $ \n\
					  4  return \n\
					  5\n"},

	{"a * b;", "a * b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  * \n\
					  4  return \n\
					  5\n"},

	{"a / b;", "a / b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  / \n\
					  4  return \n\
					  5\n"},

	{"a % b;", "a % b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  % \n\
					  4  return \n\
					  5\n"},

	{"a & b;", "a & b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  & \n\
					  4  return \n\
					  5\n"},

	{"a | b;", "a | b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  | \n\
					  4  return \n\
					  5\n"},

	{"a ^ b;", "a ^ b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  ^ \n\
					  4  return \n\
					  5\n"},

	{"a is b;", "a is b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  is \n\
					  4  return \n\
					  5\n"},

	{"a isnt b;", "a isnt b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  isnt \n\
					  4  return \n\
					  5\n"},

	{"a =~ b;", "a =~ b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  =~ \n\
					  4  return \n\
					  5\n"},

	{"a !~ b;", "a !~ b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  !~ \n\
					  4  return \n\
					  5\n"},

	{"a < b;", "a < b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  < \n\
					  4  return \n\
					  5\n"},

	{"a <= b;", "a <= b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  <= \n\
					  4  return \n\
					  5\n"},

	{"a > b;", "a > b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  > \n\
					  4  return \n\
					  5\n"},

	{"a >= b;", "a >= b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  >= \n\
					  4  return \n\
					  5\n"},

	{"not a;", "not a; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  not \n\
					  3  return \n\
					  4\n"},

	{"- a;", "- a; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  -? \n\
					  3  return \n\
					  4\n"},

	{"~ a;", "~ a; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  ~ \n\
					  3  return \n\
					  4\n"},

	{"a ? b : c;", "a ? b : c; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 9\n\
					  5  push auto b\n\
					  6  jump 10\n\
					  9  push auto c\n\
					 10  return \n\
					 11\n"},

	{"a ? b ? c : d : e ? f : g;", "a ? b ? c : d : e ? f : g; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 17\n\
					  5  push auto b\n\
					  6  jump pop no 13\n\
					  9  push auto c\n\
					 10  jump 14\n\
					 13  push auto d\n\
					 14  jump 26\n\
					 17  push auto e\n\
					 18  jump pop no 25\n\
					 21  push auto f\n\
					 22  jump 26\n\
					 25  push auto g\n\
					 26  return \n\
					 27\n"},

	{"a or b or c;", "a or b or c; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump else pop yes 11\n\
					  5  push auto b\n\
					  6  jump else pop yes 11\n\
					  9  push auto c\n\
					 10  bool \n\
					 11  return \n\
					 12\n"},

	{"a and b and c;", "a and b and c; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump else pop no 11\n\
					  5  push auto b\n\
					  6  jump else pop no 11\n\
					  9  push auto c\n\
					 10  bool \n\
					 11  return \n\
					 12\n"},

	{"a in (b, c);", "a in (b, c); }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  jump case yes 15\n\
					  6  push auto c\n\
					  7  jump case yes 15\n\
					 10  pop \n\
					 11  push value false \n\
					 12  jump 16\n\
					 15  push value true \n\
					 16  return \n\
					 17\n"},

	{"a < b + c * -d;", "a < b + c * -d; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  push auto c\n\
					  4  push auto d\n\
					  5  -? \n\
					  6  * \n\
					  7  + \n\
					  8  < \n\
					  9  return \n\
					 10\n"},

	{".a;", ".a; }\n\
					  0  nop \n\
					  1  push mem this a\n\
					  3  return \n\
					  4\n"},

	{"(a);", "(a); }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  return \n\
					  3\n"},

	{"a.b;", "a.b; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push mem b\n\
					  4  return \n\
					  5\n"},

	{"a[b];", "a[b]; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  push sub \n\
					  4  return \n\
					  5\n"},
	{"x[i .. j];", "x[i .. j]; }\n\
					  0  nop \n\
					  1  push auto x\n\
					  2  push auto i\n\
					  3  push auto j\n\
					  4  call global rangeTo 3\n\
					  7  return \n\
					  8\n"},
	{"x[i :: n];", "x[i :: n]; }\n\
					  0  nop \n\
					  1  push auto x\n\
					  2  push auto i\n\
					  3  push auto n\n\
					  4  call global rangeLen 3\n\
					  7  return \n\
					  8\n"},
	{"x[.. j];", "x[.. j]; }\n\
					  0  nop \n\
					  1  push auto x\n\
					  2  push value 0 \n\
					  3  push auto j\n\
					  4  call global rangeTo 3\n\
					  7  return \n\
					  8\n"},
	{"x[i ::];", "x[i ::]; }\n\
					  0  nop \n\
					  1  push auto x\n\
					  2  push auto i\n\
					  3  push literal 2147483647\n\
					  4  call global rangeLen 3\n\
					  7  return \n\
					  8\n"},

	{"a();", "a(); }\n\
					  0  nop \n\
					  1  call auto a 0 0\n\
					  5  return \n\
					  6\n"},

	{"a.b.c.d;", "a.b.c.d; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push mem b\n\
					  4  push mem c\n\
					  6  push mem d\n\
					  8  return \n\
					  9\n"},

	{"a[b][c][d];", "a[b][c][d]; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  push auto b\n\
					  3  push sub \n\
					  4  push auto c\n\
					  5  push sub \n\
					  6  push auto d\n\
					  7  push sub \n\
					  8  return \n\
					  9\n"},

	{"++a; --a; a++; a--;", "++a; \n\
					  0  nop \n\
					  1  ++? auto a\n\
					  3  pop \n\
--a; \n\
					  4  nop \n\
					  5  --? auto a\n\
					  7  pop \n\
a++; \n\
					  8  nop \n\
					  9  ?++ auto a\n\
					 11  pop \n\
a--; }\n\
					 12  nop \n\
					 13  ?-- auto a\n\
					 15  return \n\
					 16\n"},

	{"a().b;", "a().b; }\n\
					  0  nop \n\
					  1  call auto a 0 0\n\
					  5  push mem b\n\
					  7  return \n\
					  8\n"},

	{"a()()();", "a()()(); }\n\
					  0  nop \n\
					  1  call auto a 0 0\n\
					  5  call stack  0 0\n\
					  8  call stack  0 0\n\
					 11  return \n\
					 12\n"},

	{"a.b();", "a.b(); }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  call mem b 0\n\
					  4  return \n\
					  5\n"},

	{"a = b;  a.b = c.d; a().b = c;", "a = b;  \n\
					  0  nop \n\
					  1  push auto b\n\
					  2  = auto pop a\n\
a.b = c.d; \n\
					  3  nop \n\
					  4  push auto a\n\
					  5  push auto c\n\
					  6  push mem d\n\
					  8  = mem b\n\
					 10  pop \n\
a().b = c; }\n\
					 11  nop \n\
					 12  call auto a 0 0\n\
					 16  push auto c\n\
					 17  = mem b\n\
					 19  return \n\
					 20\n"},

	{"a(); a(b); a(b()); a(b, c, d); a(b(), c());", "a(); \n\
					  0  nop \n\
					  1  call auto a 0 0\n\
					  5  pop \n\
a(b); \n\
					  6  nop \n\
					  7  push auto b\n\
					  8  call auto a 1 0\n\
					 12  pop \n\
a(b()); \n\
					 13  nop \n\
					 14  call auto b 0 0\n\
					 18  call auto a 1 0\n\
					 22  pop \n\
a(b, c, d); \n\
					 23  nop \n\
					 24  push auto b\n\
					 25  push auto c\n\
					 26  push auto d\n\
					 27  call auto a 3 0\n\
					 31  pop \n\
a(b(), c()); }\n\
					 32  nop \n\
					 33  call auto b 0 0\n\
					 37  call auto c 0 0\n\
					 41  call auto a 2 0\n\
					 45  return \n\
					 46\n"},

	{"a(b: 1); a(b: 0, c: 1); a(b, c, d: 0, e: 1);", "a(b: 1); \n\
					  0  nop \n\
					  1  push value 1 \n\
					  2  call auto a 1 1 b\n\
					  8  pop \n\
a(b: 0, c: 1); \n\
					  9  nop \n\
					 10  push value 0 \n\
					 11  push value 1 \n\
					 12  call auto a 2 2 b c\n\
					 20  pop \n\
a(b, c, d: 0, e: 1); }\n\
					 21  nop \n\
					 22  push auto b\n\
					 23  push auto c\n\
					 24  push value 0 \n\
					 25  push value 1 \n\
					 26  call auto a 4 2 d e\n\
					 34  return \n\
					 35\n"},

	{"a(@args);", "a(@args); }\n\
					  0  nop \n\
					  1  push auto args\n\
					  2  each 0\n\
					  4  call auto a 1 0\n\
					  8  return \n\
					  9\n"},

	{"if (a) b;", "if (a) \n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 8\n\
b; }\n\
					  5  nop \n\
					  6  push auto b\n\
					  7  pop \n\
\n\
					  8  nop \n\
					  9  return nil \n\
					 10\n"},

	{"if (a) b; else c;", "if (a) \n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 11\n\
b; \n\
					  5  nop \n\
					  6  push auto b\n\
					  7  pop \n\
					  8  jump 14\n\
else c; }\n\
					 11  nop \n\
					 12  push auto c\n\
					 13  pop \n\
\n\
					 14  nop \n\
					 15  return nil \n\
					 16\n"},

	{"if (a) b; else if (c) d; else e;", "if (a) \n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 11\n\
b; \n\
					  5  nop \n\
					  6  push auto b\n\
					  7  pop \n\
					  8  jump 25\n\
else if (c) \n\
					 11  nop \n\
					 12  push auto c\n\
					 13  jump pop no 22\n\
d; \n\
					 16  nop \n\
					 17  push auto d\n\
					 18  pop \n\
					 19  jump 25\n\
else e; }\n\
					 22  nop \n\
					 23  push auto e\n\
					 24  pop \n\
\n\
					 25  nop \n\
					 26  return nil \n\
					 27\n"},

	{"if (a) b; else if (c) d; else if (e) f;", "if (a) \n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 11\n\
b; \n\
					  5  nop \n\
					  6  push auto b\n\
					  7  pop \n\
					  8  jump 30\n\
else if (c) \n\
					 11  nop \n\
					 12  push auto c\n\
					 13  jump pop no 22\n\
d; \n\
					 16  nop \n\
					 17  push auto d\n\
					 18  pop \n\
					 19  jump 30\n\
else if (e) \n\
					 22  nop \n\
					 23  push auto e\n\
					 24  jump pop no 30\n\
f; }\n\
					 27  nop \n\
					 28  push auto f\n\
					 29  pop \n\
\n\
					 30  nop \n\
					 31  return nil \n\
					 32\n"},

	{"forever\n F();", "forever  F(); }\n\
					  0  nop \n\
					  1  call global pop F 0\n\
					  4  jump 0\n\
\n\
					  7  nop \n\
					  8  return nil \n\
					  9\n"},

	{"while (a)\n F();", "while (a)  \n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 12\n\
F(); }\n\
					  5  nop \n\
					  6  call global pop F 0\n\
					  9  jump 0\n\
\n\
					 12  nop \n\
					 13  return nil \n\
					 14\n"},

	{"do\n F();\n while (b);", "do  \n\
					  0  nop \n\
					  1  jump 7\n\
					  4  jump 11\n\
F();  \n\
					  7  nop \n\
					  8  call global pop F 0\n\
while (b); }\n\
					 11  nop \n\
					 12  push auto b\n\
					 13  jump pop yes 7\n\
\n\
					 16  nop \n\
					 17  return nil \n\
					 18\n"},

	{"for (i = 0; i < 8; ++i)\n F();", "for (i = 0; i < 8; ++i)  \n\
					  0  nop \n\
					  1  push value 0 \n\
					  2  = auto pop i\n\
					  3  jump 9\n\
					  6  ++? auto i\n\
					  8  pop \n\
					  9  push auto i\n\
					 10  push int 8\n\
					 13  < \n\
					 14  jump pop no 24\n\
F(); }\n\
					 17  nop \n\
					 18  call global pop F 0\n\
					 21  jump 6\n\
\n\
					 24  nop \n\
					 25  return nil \n\
					 26\n"},

	{"for (i = 0; i < 8; ++i)\n ;", "for (i = 0; i < 8; ++i)  \n\
					  0  nop \n\
					  1  push value 0 \n\
					  2  = auto pop i\n\
					  3  jump 9\n\
					  6  ++? auto i\n\
					  8  pop \n\
					  9  push auto i\n\
					 10  push int 8\n\
					 13  < \n\
					 14  jump pop no 21\n\
; }\n\
					 17  nop \n\
					 18  jump 6\n\
\n\
					 21  nop \n\
					 22  return nil \n\
					 23\n"},

	{"for (i = 0; ; ++i)\n F();", "for (i = 0; ; ++i)  \n\
					  0  nop \n\
					  1  push value 0 \n\
					  2  = auto pop i\n\
					  3  jump 9\n\
					  6  ++? auto i\n\
					  8  pop \n\
F(); }\n\
					  9  nop \n\
					 10  call global pop F 0\n\
					 13  jump 6\n\
\n\
					 16  nop \n\
					 17  return nil \n\
					 18\n"},

	{"for (i = 0; i < 8; )\n F();", "for (i = 0; i < 8; )  \n\
					  0  nop \n\
					  1  push value 0 \n\
					  2  = auto pop i\n\
					  3  push auto i\n\
					  4  push int 8\n\
					  7  < \n\
					  8  jump pop no 18\n\
F(); }\n\
					 11  nop \n\
					 12  call global pop F 0\n\
					 15  jump 3\n\
\n\
					 18  nop \n\
					 19  return nil \n\
					 20\n"},

	{"for (i = 0; ; )\n F();", "for (i = 0; ; )  \n\
					  0  nop \n\
					  1  push value 0 \n\
					  2  = auto pop i\n\
F(); }\n\
					  3  nop \n\
					  4  call global pop F 0\n\
					  7  jump 3\n\
\n\
					 10  nop \n\
					 11  return nil \n\
					 12\n"},

	{"for (; i < 8; )\n F();", "for (; i < 8; )  \n\
					  0  nop \n\
					  1  push auto i\n\
					  2  push int 8\n\
					  5  < \n\
					  6  jump pop no 16\n\
F(); }\n\
					  9  nop \n\
					 10  call global pop F 0\n\
					 13  jump 1\n\
\n\
					 16  nop \n\
					 17  return nil \n\
					 18\n"},

	{"for (; ; ++i)\n F();", "for (; ; ++i)  \n\
					  0  nop \n\
					  1  jump 7\n\
					  4  ++? auto i\n\
					  6  pop \n\
F(); }\n\
					  7  nop \n\
					  8  call global pop F 0\n\
					 11  jump 4\n\
\n\
					 14  nop \n\
					 15  return nil \n\
					 16\n"},

	{"for (; ; )\n F();", "for (; ; )  F(); }\n\
					  0  nop \n\
					  1  call global pop F 0\n\
					  4  jump 0\n\
\n\
					  7  nop \n\
					  8  return nil \n\
					  9\n"},

	{"for (a in b) F();", "for (a in b) \n\
					  0  nop \n\
					  1  push auto b\n\
					  2  call mem Iter 0\n\
					  4  dup \n\
					  5  dup \n\
					  6  call mem Next 0\n\
					  8  = auto a\n\
					  9  isnt \n\
					 10  jump pop no 20\n\
F(); }\n\
					 13  nop \n\
					 14  call global pop F 0\n\
					 17  jump 4\n\
					 20  pop \n\
\n\
					 21  nop \n\
					 22  return nil \n\
					 23\n"},

	{"forever\n {\n if (a)\n continue;\n if (b)\n continue ;\n if (c)\n break "
	 ";\n if "
	 "(d)\n break ;\n}",
		"forever  {  if (a)  \n\
					  0  nop \n\
					  1  push auto a\n\
					  2  jump pop no 9\n\
continue;  \n\
					  5  nop \n\
					  6  jump 0\n\
if (b)  \n\
					  9  nop \n\
					 10  push auto b\n\
					 11  jump pop no 18\n\
continue ;  \n\
					 14  nop \n\
					 15  jump 0\n\
if (c)  \n\
					 18  nop \n\
					 19  push auto c\n\
					 20  jump pop no 27\n\
break ;  \n\
					 23  nop \n\
					 24  jump 39\n\
if (d)  \n\
					 27  nop \n\
					 28  push auto d\n\
					 29  jump pop no 36\n\
break ; } }\n\
					 32  nop \n\
					 33  jump 39\n\
					 36  jump 0\n\
\n\
					 39  nop \n\
					 40  return nil \n\
					 41\n"},

	{"return ;", "return ; }\n\
					  0  nop \n\
					  1  return nil \n\
					  2\n"},

	{"return a;", "return a; }\n\
					  0  nop \n\
					  1  push auto a\n\
					  2  return \n\
					  3\n"},

	{"switch (a)\n {\n case 1 :\n b();\n case 2, 3, 4 :\n c();\n }",
		"switch (a)  {  \n\
					  0  nop \n\
					  1  push auto a\n\
case 1 :  \n\
					  2  nop \n\
					  3  push value 1 \n\
					  4  jump case no 16\n\
b();  \n\
					  7  nop \n\
					  8  call auto b 0 0\n\
					 12  pop \n\
					 13  jump 49\n\
case 2, 3, 4 :  \n\
					 16  nop \n\
					 17  push int 2\n\
					 20  jump case yes 35\n\
					 23  push int 3\n\
					 26  jump case yes 35\n\
					 29  push int 4\n\
					 32  jump case no 44\n\
c();  \n\
					 35  nop \n\
					 36  call auto c 0 0\n\
					 40  pop \n\
					 41  jump 49\n\
} }\n\
					 44  nop \n\
					 45  pop \n\
					 46  push literal \"unhandled switch value\"\n\
					 47  throw \n\
					 48  pop \n\
\n\
					 49  nop \n\
					 50  return nil \n\
					 51\n"},

	{"switch (a)\n { case 1 :\n b();\n case 2, 3, 4 :\n c();\n default :\n "
	 "d();\n }",
		"switch (a)  { \n\
					  0  nop \n\
					  1  push auto a\n\
case 1 :  \n\
					  2  nop \n\
					  3  push value 1 \n\
					  4  jump case no 16\n\
b();  \n\
					  7  nop \n\
					  8  call auto b 0 0\n\
					 12  pop \n\
					 13  jump 56\n\
case 2, 3, 4 :  \n\
					 16  nop \n\
					 17  push int 2\n\
					 20  jump case yes 35\n\
					 23  push int 3\n\
					 26  jump case yes 35\n\
					 29  push int 4\n\
					 32  jump case no 44\n\
c();  \n\
					 35  nop \n\
					 36  call auto c 0 0\n\
					 40  pop \n\
					 41  jump 56\n\
default :  \n\
					 44  nop \n\
					 45  pop \n\
d();  } }\n\
					 46  nop \n\
					 47  call auto d 0 0\n\
					 51  pop \n\
					 52  jump 56\n\
					 55  pop \n\
\n\
					 56  nop \n\
					 57  return nil \n\
					 58\n"},

	{"#();", "#(); }\n\
					  0  nop \n\
					  1  push literal #()\n\
					  2  return \n\
					  3\n"},

	{"#(1);", "#(1); }\n\
					  0  nop \n\
					  1  push literal #(1)\n\
					  2  return \n\
					  3\n"},

	{"#(1, 2, 3);", "#(1, 2, 3); }\n\
					  0  nop \n\
					  1  push literal #(1, 2, 3)\n\
					  2  return \n\
					  3\n"},

	{"#(a: 1);", "#(a: 1); }\n\
					  0  nop \n\
					  1  push literal #(a: 1)\n\
					  2  return \n\
					  3\n"},

	{"#(0, b: 1);", "#(0, b: 1); }\n\
					  0  nop \n\
					  1  push literal #(0, b: 1)\n\
					  2  return \n\
					  3\n"},

	{"function () { };", "function () { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"function (a) { };", "function (a) { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"function (a, b, c) { };", "function (a, b, c) { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"function (a = 1) { };", "function (a = 1) { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"function (a = 1, b = 2, c = 3) { };",
		"function (a = 1, b = 2, c = 3) { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"function (a, b, c = 0, d = 1) { };",
		"function (a, b, c = 0, d = 1) { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"function (@args) { };", "function (@args) { }; }\n\
					  0  nop \n\
					  1  push literal  /* function */\n\
					  2  return \n\
					  3\n"},

	{"class { };", "class { }; }\n\
					  0  nop \n\
					  1  push literal  /* class */\n\
					  2  return \n\
					  3\n"},

	{"class : X { };", "class : X { }; }\n\
					  0  nop \n\
					  1  push literal  /* class : X */\n\
					  2  return \n\
					  3\n"},

	{"class { a: 1 };", "class { a: 1 }; }\n\
					  0  nop \n\
					  1  push literal  /* class */\n\
					  2  return \n\
					  3\n"},
	{"class { a: 1, b: 2 };", "class { a: 1, b: 2 }; }\n\
					  0  nop \n\
					  1  push literal  /* class */\n\
					  2  return \n\
					  3\n"},

	{"class { a: 1; b: 2; };", "class { a: 1; b: 2; }; }\n\
					  0  nop \n\
					  1  push literal  /* class */\n\
					  2  return \n\
					  3\n"},

	{"class { a: 1, b: 2, c: 3 };", "class { a: 1, b: 2, c: 3 }; }\n\
					  0  nop \n\
					  1  push literal  /* class */\n\
					  2  return \n\
					  3\n"},

	{"-7;", "-7; }\n\
					  0  nop \n\
					  1  push int -7\n\
					  4  return \n\
					  5\n"},

	{"-7.0;", "-7.0; }\n\
					  0  nop \n\
					  1  push literal -7\n\
					  2  return \n\
					  3\n"},

	{"~5;", "~5; }\n\
					  0  nop \n\
					  1  push int -6\n\
					  4  return \n\
					  5\n"},

	{"6 + 0;", "6 + 0; }\n\
					  0  nop \n\
					  1  push int 6\n\
					  4  return \n\
					  5\n"},

	{"6.0 + 3.0;", "6.0 + 3.0; }\n\
					  0  nop \n\
					  1  push literal 9\n\
					  2  return \n\
					  3\n"},

	{"6 - 3;", "6 - 3; }\n\
					  0  nop \n\
					  1  push int 3\n\
					  4  return \n\
					  5\n"},

	{"6.0 - 3.0;", "6.0 - 3.0; }\n\
					  0  nop \n\
					  1  push literal 3\n\
					  2  return \n\
					  3\n"},

	{"6 * 3;", "6 * 3; }\n\
					  0  nop \n\
					  1  push int 18\n\
					  4  return \n\
					  5\n"},

	{"6.0 * 3.0;", "6.0 * 3.0; }\n\
					  0  nop \n\
					  1  push literal 18\n\
					  2  return \n\
					  3\n"},

	{"6 / 3;", "6 / 3; }\n\
					  0  nop \n\
					  1  push int 2\n\
					  4  return \n\
					  5\n"},

	{"6.0 / 3.0;", "6.0 / 3.0; }\n\
					  0  nop \n\
					  1  push literal 2\n\
					  2  return \n\
					  3\n"},

	{"6 % 4;", "6 % 4; }\n\
					  0  nop \n\
					  1  push int 2\n\
					  4  return \n\
					  5\n"},

	{"5 & 6;", "5 & 6; }\n\
					  0  nop \n\
					  1  push int 4\n\
					  4  return \n\
					  5\n"},

	{"5 | 6;", "5 | 6; }\n\
					  0  nop \n\
					  1  push int 7\n\
					  4  return \n\
					  5\n"},

	{"5 ^ 6;", "5 ^ 6; }\n\
					  0  nop \n\
					  1  push int 3\n\
					  4  return \n\
					  5\n"},

	{"1 << 4;", "1 << 4; }\n\
					  0  nop \n\
					  1  push int 16\n\
					  4  return \n\
					  5\n"},

	{"32 >> 2;", "32 >> 2; }\n\
					  0  nop \n\
					  1  push int 8\n\
					  4  return \n\
					  5\n"},

	{"'hello' $ '';", "'hello' $ ''; }\n\
					  0  nop \n\
					  1  push literal \"hello\"\n\
					  2  return \n\
					  3\n"},

	{"(true ? 'xx' : 'yy') $ 'z'", "(true ? 'xx' : 'yy') $ 'z' }\n\
					  0  nop \n\
					  1  push value true \n\
					  2  jump pop no 9\n\
					  5  push literal \"xx\"\n\
					  6  jump 10\n\
					  9  push literal \"yy\"\n\
					 10  push literal \"z\"\n\
					 11  $ \n\
					 12  return \n\
					 13\n"},

	{"-(true ? 123 : 456)", "-(true ? 123 : 456) }\n\
					  0  nop \n\
					  1  push value true \n\
					  2  jump pop no 11\n\
					  5  push int 123\n\
					  8  jump 14\n\
					 11  push int 456\n\
					 14  -? \n\
					 15  return \n\
					 16\n"}};

static void process(int i, const char* code, const char* result) {
	char buf[4000];
	strcpy(buf, "function () {\n");
	strcat(buf, code);
	strcat(buf, "\n}");
	SuFunction* fn;
	try {
		fn = force<SuFunction*>(compile(buf));
	} catch (const Except& e) {
		except(i << ": " << code << "\n\t=> " << e);
	}
	OstreamStr out;
	fn->disasm(out);
	const char* output = out.str();
	if (0 != strcmp(result, output))
		except(i << ": " << code << "\n\t=> " << result << "\n\t!= '" << output
				 << "'");
}

TEST(compile) {
	for (int i = 0; i < sizeof cmpltests / sizeof(Cmpltest); ++i)
		process(i, cmpltests[i].query, cmpltests[i].result);
}

TEST(compile_function) {
	const char* s = "function (a, b = 0, c = false, d = 123, e = 'hello') { }";
	SuFunction* fn = force<SuFunction*>(compile(s));
	assert_eq(fn->nparams, 5);
	assert_eq(fn->ndefaults, 4);
	assert_eq(fn->locals[0], symnum("a"));
	assert_eq(fn->locals[1], symnum("b"));
	assert_eq(fn->locals[2], symnum("c"));
	assert_eq(fn->locals[3], symnum("d"));
	assert_eq(fn->locals[4], symnum("e"));
	assert_eq(fn->literals[0], SuZero);
	assert_eq(fn->literals[1], SuFalse);
	assert_eq(fn->literals[2], 123);
	assert_eq(fn->literals[3], Value("hello"));
}

TEST(compile2) {
	compile("function (x) { if x\n{ } }");
	compile("function () {\n for x in F()\n { } }");
	compile("function () { x \n * y }");
	compile("function () { x \n ? y : z }");
	compile("function () { [1, x, a: y, b: 4] }");
	compile("function () { if (A) B else C }");
	compile("function (.x) { }");
}

#include "porttest.h"

PORTTEST(compile) {
	auto s = args[0].str();
	auto type = args[1];
	auto expected = args[2];
	try {
		Value x = compile(s);
		if (type != x.type())
			return OSTR("got: " << x.type());
		auto result = OSTR(x);
		if (expected != result)
			return OSTR("got: " << result);
	} catch (Except& e) {
		if (type != "Exception" || !e.gcstr().has(expected))
			return e.str();
	}
	return nullptr;
}
