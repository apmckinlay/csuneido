#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "record.h"
#include "index.h"
#include "hashmap.h"
#include <deque>
#include <set>
#include <map>
#include "gcstring.h"
#include "lisp.h"
#include "mmfile.h"
#include "mmtypes.h"

// foreign key modes
enum Fkmode {
	BLOCK = 0,
	CASCADE_UPDATES = 1,
	CASCADE_DELETES = 2,
	CASCADE = 3
};

enum TranType { READONLY, READWRITE };

// table numbers
enum { TN_TABLES = 1, TN_COLUMNS = 2, TN_INDEXES = 3, TN_VIEWS = 4 };

// tables records fields
enum { T_TBLNUM, T_TABLE, T_NEXTFIELD, T_NROWS, T_TOTALSIZE };

// columns records fields
enum { C_TBLNUM, C_COLUMN, C_FLDNUM };

// indexes records fields
enum {
	I_TBLNUM,
	I_COLUMNS,
	I_KEY,
	I_FKTABLE,
	I_FKCOLUMNS,
	I_FKMODE,
	I_ROOT,
	I_TREELEVELS,
	I_NNODES
};

// views records fields
enum { V_NAME, V_DEFINITION };

typedef int TblNum;

// ReSharper disable once CppImplicitDefaultConstructorNotAvailable
struct Dbhdr { // NOLINT
	TblNum next_table;
	Mmoffset32 indexes;
	int version;
};

struct Col {
	Col(const gcstring& c, int n) : column(c), colnum(n) {
	}
	bool operator==(const Col& y) const {
		return column == y.column;
	} // can't use colnum because rule columns are all -1
	bool operator<(const Col& y) const {
		return colnum < y.colnum;
	}

	gcstring column;
	int colnum;
};

struct Fkey {
	Fkey(const gcstring& t, const gcstring& c, Fkmode m)
		: table(t), columns(c), mode(m) {
	}

	gcstring table;
	gcstring columns;
	Fkmode mode;
};

class Database;

struct Idx {
	Idx(const gcstring& table, Record r, const gcstring& c, short* n, Index* i,
		Database* db);
	void update();
	bool operator==(const Idx& y) const {
		return columns == y.columns;
	}

	Index* index;
	int nnodes;
	Record rec;
	gcstring columns;
	short* colnums;
	bool iskey;
	Fkey fksrc;
	Lisp<Fkey> fkdsts;
};

struct Tbl {
	Tbl(Record r, const Lisp<Col>& c, const Lisp<Idx>& i);
	void update();
	int colNum(const gcstring& col);
	int specialFieldNum(const gcstring& col);
	Lisp<gcstring> get_fields();
	void user_trigger(int tran, Record oldrec, Record newrec);

	Record rec;
	Lisp<Col> cols;
	Lisp<Idx> idxs;
	TblNum num;
	gcstring name;
	int nextfield;
	int nrecords;
	int totalsize;
	int trigger;
	Lisp<int> flds; // for user defined triggers
};

typedef int TranTime;

enum ActType { CREATE_ACT, DELETE_ACT };

struct TranAct {
	TranAct(ActType typ, TblNum tbl, Mmoffset o, TranTime tim)
		: type(typ), tblnum(tbl), off(o), time(tim) {
	}
	ActType type;
	TblNum tblnum;
	Mmoffset off;
	TranTime time;
};

struct TranRead {
	TranRead(TblNum tbl, const char* idx)
		: tblnum(tbl), index(idx), org(keymax) {
	}
	TblNum tblnum;
	const char* index;
	Record org;
	Record end;
};

struct TranDelete {
	TranDelete() {
	}
	TranDelete(TranTime t);
	TranTime tran = 0;
	TranTime time = 0;
};

class Transaction {
public:
	Transaction() = default; // need for trans map
	Transaction(TranType t, TranTime clock, const char* session_id = "");

	bool operator<(const Transaction& t) const { // for set
		return asof < t.asof;
	}

	TranType type = READONLY;
	TranTime tran = 0;
	TranTime asof = 0;
	std::deque<TranAct> acts;
	std::deque<TranRead> reads;
	const char* session_id = "";
	const char* conflict = nullptr;
};

inline bool operator<(const Transaction& tran, TranTime time) {
	return tran.asof < time;
}

inline bool operator<(TranTime time, const Transaction& tran) {
	return time < tran.asof;
}

#define DBCREATE true

struct Dbhdr;
class Tables;
class SuValue;

class Database {
	friend struct Idx;
	friend class Index;
	friend class Index::iterator;
	friend SuValue* su_transactions();
	friend class DbRecoverImp;
	friend void test_transaction();
	friend void test_transaction_reads();
	friend void test_transaction_finalization();
	friend class DbmsLocal;

public:
	explicit Database(const char* filename, bool create = false);
	~Database() {
		shutdown();
		delete mmf;
		mmf = 0;
	}

	int transaction(TranType type, const char* session_id = "");
	bool refresh(int tran);
	bool commit(int tran, const char** conflict = 0);
	void abort(int tran);
	Lisp<int> tranlist();
	int final_size() const;
	bool visible(int tran, Mmoffset adr);

	void add_table(const gcstring& table);
	void add_column(const gcstring& table, const gcstring& column);
	void add_index(const gcstring& table, const gcstring& columns, bool key,
		const gcstring& fktable = "", const gcstring& fkcolumns = "",
		Fkmode fkmode = BLOCK, bool unique = false);
	void add_view(const gcstring& table, const gcstring& definition);

	void add_record(int tran, const gcstring& table, Record r);
	void add_any_record(int tran, const gcstring& table, Record& r) {
		add_any_record(tran, ck_get_table(table), r);
	}
	void add_any_record(int tran, Tbl* tbl, Record& r);
	void update_record(int tran, const gcstring& table, const gcstring& index,
		Record key, Record newrec);
	Mmoffset update_record(
		int tran, Tbl* tbl, Record oldrec, Record newrec, bool block = true);
	void update_any_record(int tran, const gcstring& table,
		const gcstring& index, Record key, Record newrec);

	void remove_table(const gcstring& table);
	void remove_any_table(const gcstring& table);
	void remove_column(const gcstring& table, const gcstring& column);
	void remove_index(const gcstring& table, const gcstring& columns);
	void remove_record(
		int tran, const gcstring& table, const gcstring& index, Record key);
	void remove_any_record(
		int tran, const gcstring& table, const gcstring& index, Record key);
	void remove_view(const gcstring& table);

	bool istable(const gcstring& table) {
		return get_table(table) != 0;
	}
	Tbl* get_table(const gcstring& table);
	Tbl* ck_get_table(const gcstring& table);
	Index* get_index(const gcstring& table, const gcstring& columns) {
		return get_index(get_table(table), columns);
	}
	Index* first_index(const gcstring& table);
	Lisp<Lisp<gcstring> > get_indexes(const gcstring& table);
	Lisp<Lisp<gcstring> > get_keys(const gcstring& table);
	Lisp<Lisp<gcstring> > get_nonkeys(const gcstring& table);
	Lisp<gcstring> get_columns(const gcstring& table);
	Lisp<gcstring> get_fields(const gcstring& table);
	Lisp<gcstring> get_rules(const gcstring& table);
	gcstring get_view(const gcstring& table);
	Record get_record(
		int tran, const gcstring& table, const gcstring& index, Record key);
	void schema_out(Ostream& os, const gcstring& table);
	Dbhdr* dbhdr() const;

	bool rename_table(const gcstring& oldname, const gcstring& newname);
	bool rename_column(const gcstring& table, const gcstring& oldname,
		const gcstring& newname);
	Record find(int tran, Index* index, Record key);

	int nrecords(const gcstring& table);
	size_t totalsize(const gcstring& table);
	float rangefrac(
		const gcstring& table, const gcstring& index, Record org, Record end);

	static bool is_system_table(const gcstring& table);

	// should be private, but used by recover schema
	void add_index_entries(int tran, Tbl* tbl, Record r);
	Tbl* get_table(TblNum tblnum);
	bool recover_index(Record& idxrec);
	void invalidate_table(TblNum tblnum);
	Mmoffset alloc(uint32_t n, char type = MM_OTHER) {
		return mmf->alloc(n, type);
	}
	void unalloc(uint32_t n) {
		mmf->unalloc(n);
	}
	void* adr(Mmoffset offset) {
		return mmf->adr(offset);
	}

	// for use by dbcopy only
	Mmoffset output_record(int tran, Tbl* tbl, Record& rec);
	void create_indexes(Tbl* tbl, Mmoffset first, Mmoffset last);

	// for tests
	bool final_empty() const {
		return final.empty();
	}
	bool trans_empty() const {
		return trans.empty();
	}

	Mmfile* mmf;
	bool loading = false;

private:
	void open();
	void create();
	Tbl* get_table(Record table_rec);
	Index* get_index(Tbl* tbl, const gcstring& columns);
	void remove_record(int tran, Tbl* tbl, Record r);
	void remove_index_entries(Tbl* tbl, Record r);
	void remove_any_index(const gcstring& table, const gcstring& columns) {
		remove_any_index(ck_get_table(table), columns);
	}
	void remove_any_index(Tbl* tbl, const gcstring& columns);
	static Record record(TblNum tblnum, const gcstring& column, int field);
	static Record record(TblNum tblnum, const gcstring& columns, Index* index,
		const gcstring& fktable = "", const gcstring& fkcolumns = "",
		int fkmode = 0);
	static Record record(TblNum tblnum, const gcstring& table, int nrows,
		int nextfield, int totalsize = 100);
	static Record key(TblNum tblnum);
	static Record key(const char* table);
	static Record key(const gcstring& table);
	static Record key(TblNum tblnum, const char* columns);
	static Record key(TblNum tblnum, const gcstring& columns);
	void table_record(
		TblNum tblnum, const char* tblname, int nrows, int nextfield);
	void columns_record(TblNum tblnum, const char* column, int field);
	Mmoffset indexes_record(Index* index);
	Index* mkindex(Record r);
	static bool is_system_column(const gcstring& table, const gcstring& column);
	static bool is_system_index(const gcstring& table, const gcstring& columns);
	const char* fkey_source_block(int tran, Tbl* tbl, Record r);
	bool fkey_source_block(
		int tran, Tbl* fktbl, const gcstring& fkcolumns, Record key);
	const char* fkey_target_block(int tran, Tbl* tbl, Record r);
	const char* fkey_target_block(
		int tran, const Idx& idx, Record key, Record newkey = Record());
	Mmoffset output(TblNum tblnum, Record& record);
	Record input(Mmoffset off) {
		return Record(mmf, off);
	}

	TranTime create_time(Mmoffset adr);
	TranTime delete_time(Mmoffset adr);
	TranTime table_create_time(TblNum tblnum);
	void create_act(int tran, TblNum tblnum, Mmoffset adr);
	bool delete_act(int tran, TblNum tblnum, Mmoffset adr);
	void undo_delete_act(int tran, TblNum tblnum, Mmoffset adr);
	TranRead* read_act(int tran, TblNum tblnum, const char* index);
	void table_create_act(TblNum tblnum);
	bool validate_reads(Transaction* t);
	bool finalize();
	void shutdown();
	Transaction* get_tran(int tran);
	Transaction* ck_get_tran(int tran);
	void checksum(void* buf, size_t n);
	void commit_update_tran(int tran);
	void write_commit_record(
		int tran, const std::deque<TranAct>& acts, int ncreates, int ndeletes);
	const Transaction* find_tran(int tran);
	char* write_conflict(TblNum tblnum, Mmoffset a, TranDelete* p);
	char* read_conflict(const Transaction* t, int tblnum, Record from,
		Record to, const char* index, Record key, ActType type);

	Tables* tables;
	// used by get_table
	Index* tables_index{};
	Index* tblnum_index{};
	Index* columns_index{};
	Index* indexes_index{};
	Index* fkey_index{};
	// used by get_view
	Index* views_index{};

	int clock;
	std::map<int, Transaction> trans;        // TODO consider Transaction*
	HashMap<Mmoffset, TranTime> created;     // record address -> create time
	HashMap<Mmoffset, TranDelete> deleted;   // record address -> delete time
	HashMap<TblNum, TranTime> table_created; // table name -> create time
	std::set<Transaction> final; // transactions that need to be finalized
	uint32_t cksum;              // since commit
	Lisp<Mmoffset> schema_deletes;
	int output_type;
	IndexDest* dest;
};

short* comma_to_nums(const Lisp<Col>& cols, const gcstring& str);
Record project(Record r, short* cols, Mmoffset adr = 0);

enum { END = -1 }; // for column numbers

const int schema_tran = 0;

bool isSpecialField(const gcstring& col);
