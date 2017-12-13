#ifndef BTREE_H
#define BTREE_H

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

#include <algorithm>
#include "std.h"
#include "record.h"
#include "mmfile.h"
#include "except.h"
#include "slots.h" // for keydup
#include "gc.h" // for noptrs
#include <malloc.h> // for alloca

//#include "ostreamcon.h"
//#include "ostreamstr.h"

using namespace std;

const Mmoffset NIL(0);

const int NODESIZE = 4096 - MM_OVERHEAD;

enum Insert { OK, DUP, WONT_FIT };	// return values for insert

template <class LeafSlot, class TreeSlot, class LeafSlots, class TreeSlots, class Dest>
class Btree
	{
	typedef typename LeafSlot::Key Key;
	typedef typename LeafSlots::iterator LeafSlotsIterator;
	typedef typename TreeSlots::iterator TreeSlotsIterator;
	typedef Btree<LeafSlot,TreeSlot,LeafSlots,TreeSlots,Dest> btree;
	friend class test_btree;
private:
	// LeafNode -----------------------------------------------------
	struct LeafNode
		{
		LeafNode()
			{
			set_next(NIL);
			set_prev(NIL);
			}
		Insert insert(const LeafSlot& x) // returns false if no room
			{
			LeafSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), x);
			if (slot < slots.end() && *slot == x)
				return DUP;
			else if (! slots.insert(slot, x))
				return WONT_FIT;
			return OK;
			}
		bool erase(const Key& key)
			{
			LeafSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), LeafSlot(key));
			if (slot == slots.end() || (*slot).key != key)
				return false;
			slots.erase(slot);
			return true;
			}
		Mmoffset split(Dest* dest, const LeafSlot& x, Mmoffset off)
			{
			// variable split
			int percent = 50;
			if (x < slots.front())
				percent = 75;
			else if (x > slots.back())
				percent = 25;
			Mmoffset leftoff = dest->alloc(NODESIZE);
			LeafNode* left = new(dest->adr(leftoff)) LeafNode;

			//~ int n = slots.size();
			//~ int nright = (n * percent) / 100;
			//~ // move first half of right keys to left
			//~ left->slots.append(slots.begin(), slots.end() - nright);
			//~ slots.erase(slots.begin(), slots.end() - nright);

			// bool new_one_added = false;
			int rem = (left->slots.remaining() * percent) / 100;
			while (left->slots.remaining() > rem)
				{
				left->slots.push_back(slots.front());
				slots.erase(slots.begin());
				}

			// maintain linked list of leaves
			left->set_prev(prev());
			left->set_next(off);
			set_prev(leftoff);
			if (left->prev() != NIL)
				((LeafNode*) dest->adr(left->prev()))->set_next(leftoff);
			return leftoff;
			}
		void unlink(Dest* dest)
			{
			if (prev() != NIL)
				((LeafNode*) dest->adr(prev()))->set_next(next());
			if (next() != NIL)
				((LeafNode*) dest->adr(next()))->set_prev(prev());
			}
		bool empty()
			{ return slots.empty(); }

		Mmoffset next()
			{ return next_.unpack(); }
		Mmoffset prev()
			{ return prev_.unpack(); }
		void set_next(Mmoffset next)
			{ next_ = Mmoffset32(next); }
		void set_prev(Mmoffset prev)
			{ prev_ = Mmoffset32(prev); }

		LeafSlots slots;
		Mmoffset32 next_, prev_;
		};
	// TreeNode -----------------------------------------------------
	struct TreeNode
		{
		TreeNode() : lastoff(NIL)
			{ }
		bool insert(const Key& key, Mmoffset off) // returns false if no room
			{
			TreeSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), TreeSlot(key));
			verify(slot == slots.end() || (*slot).key != key); // no dups
			return slots.insert(slot, TreeSlot(key, off));
			}
		Mmoffset find(const Key& key)
			{
			TreeSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), TreeSlot(key));
			return slot < slots.end() ? (*slot).adr : lastoff.unpack();
			}
		void erase(const Key& key)
			{
			// NOTE: erases the first key >= key
			// this is so Btree::erase can use target key
			if (slots.size() == 0)
				{
				lastoff = NIL;
				return ;
				}
			TreeSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), TreeSlot(key));
			if (slot == slots.end())
				{
				--slot;
				lastoff = (*slot).adr;
				}
			slots.erase(slot);
			}
		Mmoffset split(Dest* dest, const Key& key)
			{
			int percent = 50;
			if (key < slots.front().key)
				percent = 75;
			else if (key > slots.back().key)
				percent = 25;
			Mmoffset leftoff = dest->alloc(NODESIZE);
			TreeNode* left = new(dest->adr(leftoff)) TreeNode;
			int n = slots.size();
			int nright = (n * percent) / 100;
			// move first part of right keys to left
			left->slots.append(slots.begin(), slots.end() - nright);
			slots.erase(slots.begin(), slots.end() - nright);
			return leftoff;
			}
		bool empty()
			{ return slots.empty() && lastoff == NIL; }

		TreeSlots slots;
		Mmoffset32 lastoff;
		};

	class Node
		{
		public:
			Node(int lev) : level_(lev)
				{ }
			virtual ~Node() = default;
			int level() const
				{ return level_; }
			virtual int size() const = 0;
			virtual int findPos(const Key& key) const = 0;
			virtual Mmoffset adr(int pos) const = 0;
		private:
			int level_;
		};

	class TNode : public Node
		{
		public:
			TNode(int level, TreeNode* n) : Node(level), node(n)
				{ }
			int size() const override
				{
				return node->slots.size() + 1;
				}
			int findPos(const Key& key) const override
				{
				TreeSlots& slots = node->slots;
				TreeSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), TreeSlot(key));
				return slot < slots.end() ? slot - slots.begin() : slots.size();
				}
			Mmoffset adr(int pos) const override
				{
				return pos == node->slots.size() ? node->lastoff.unpack() : node->slots[pos].adr;
				}
		private:
			TreeNode* node;
		};

	class LNode : public Node
		{
		public:
			LNode(int level, LeafNode* n) : Node(level), node(n)
				{ }
			int size() const override
				{
				return node->slots.size();
				}
			int findPos(const Key& key) const override
				{
				LeafSlots& slots = node->slots;
				LeafSlotsIterator slot = std::lower_bound(slots.begin(), slots.end(), LeafSlot(key));
				return slot - slots.begin();
				}
			Mmoffset adr(int pos) const override
				{
				except("should not be called");
				}
		private:
			LeafNode* node;
		};

public:
	// iterator -----------------------------------------------------
	class iterator;
	friend class iterator;

	class iterator
		{
		friend class Btree<LeafSlot,TreeSlot,LeafSlots,TreeSlots,Dest>;
	public:
		iterator() // end
			{ }
		LeafSlot& operator*()
			{ return cur; }
		LeafSlot* operator->()
			{ return &cur; }
		void operator++()
			{
			if (off == NIL)
				return ;
			verify(bt);
			if (bt->modified > valid && ! seek(cur.key))
				return ;	// key has been erased so we're on the next one
			// get next key sequentially
			LeafNode* leaf = (LeafNode*) bt->dest->adr(off);
			LeafSlotsIterator t = std::upper_bound(leaf->slots.begin(), leaf->slots.end(), cur);
			if (t < leaf->slots.end())
				cur.copy(*t);
			else if ((off = leaf->next()) != NIL)
				{
				LeafNode* node = (LeafNode*) bt->dest->adr(off);
				cur.copy(node->slots.front());
				}
			}
		void operator--()
			{
			if (off == NIL)
				return ;
			verify(bt);
			if (bt->modified > valid)
				seek(cur.key);
			// get prev key sequentially
			LeafNode* leaf = (LeafNode*) bt->dest->adr(off);
			LeafSlotsIterator t = std::lower_bound(leaf->slots.begin(), leaf->slots.end(), cur);
			if (t > leaf->slots.begin())
				{ --t; cur.copy(*t); }
			else if ((off = leaf->prev()) != NIL)
				cur.copy(((LeafNode*) bt->dest->adr(off))->slots.back());
			}
		bool operator==(const iterator& j) const
			{
			return (off == NIL && j.off == NIL) ||
				(off == j.off && cur == j.cur);
			}
		bool operator!=(const iterator& j) const
			{
			return ! (*this == j);
			}
		bool eof() const
			{ return off == NIL; }
		void seteof()
			{ off = NIL; }
		bool seek(const Key& key)
			{
			if (! bt)
				return false;
			off = bt->root();
			for (int i = 0; i < bt->treelevels; ++i)
				{
				TreeNode* node = (TreeNode*) bt->dest->adr(off);
				off = node->find(key);
				}
			LeafNode* leaf = (LeafNode*) bt->dest->adr(off);
			if (bt->treelevels == 0 && leaf->slots.size() == 0)
				{
				// empty btree
				off = NIL;
				return false;
				}
			LeafSlotsIterator t = std::lower_bound(leaf->slots.begin(), leaf->slots.end(), LeafSlot(key));
			valid = bt->modified;
			bool found;
			if (t == leaf->slots.end())
				{
				--t;
				cur.copy(*t);
				operator++();
				found = false;
				}
			else
				{
				found = (key == (*t).key);
				cur.copy(*t);
				}
			return found;
			}
	private:
		iterator(btree* b, Mmoffset o, const LeafSlot& t)
			: bt(b), off(o), valid(b->modified)
			{ cur.copy(t); }
		iterator(btree* b, const Key& key) : bt(b)
			{ seek(key); }
		btree* bt = nullptr;
		Mmoffset off = NIL;
		LeafSlot cur;
		// TODO: make slot.copy support a static buffer to reduce allocation
		ulong valid = 0;
		};
	//---------------------------------------------------------------
	iterator first()
		{
		Mmoffset off = root();
		for (int i = 0; i < treelevels; ++i)
			off = ((TreeNode*) (dest->adr(off)))->slots.front().adr;
		LeafNode* leaf = (LeafNode*) (dest->adr(off));
		if (off == root() && leaf->empty())
			return iterator();
		return iterator(this, off, leaf->slots.front());
		}
	iterator last()
		{
		Mmoffset off = root();
		for (int i = 0; i < treelevels; ++i)
			off = ((TreeNode*) (dest->adr(off)))->lastoff.unpack();
		LeafNode* leaf = (LeafNode*) (dest->adr(off));
		if (off == root() && leaf->empty())
			return iterator();
		return iterator(this, off, leaf->slots.back());
		}
	iterator locate(const Key& key)
		{ return iterator(this, key); }
	iterator end()
		{ return iterator(); }
	iterator find(const Key& key)
		{
		iterator iter = locate(key);
		return iter != end() && (*iter).key == key ? iter : end();
		}
	//---------------------------------------------------------------
	explicit Btree(Dest* d) : root_(0), treelevels(0), nnodes(0), modified(0), dest(d)
		{ }
	Btree(Dest* d, Mmoffset r, int tl, int nn)
		: root_(r), treelevels(tl), nnodes(nn), modified(0), dest(d)
		{ verify(r >= 0); }
	void free()
		{
		dest->free();
		}
	// insert -------------------------------------------------------
	bool insert(LeafSlot x) // returns false for duplicate key
		{
		TreeNode* nodes[MAXLEVELS];

		// search down the tree
		Mmoffset off = root();
		int i;
		for (i = 0; i < treelevels; ++i)
			{
			nodes[i] = (TreeNode*) dest->adr(off);
			off = nodes[i]->find(x.key);
			}
		LeafNode* leaf = (LeafNode*) dest->adr(off);

		// insert the key & data into the leaf
		Insert status = leaf->insert(x);
		if (status != WONT_FIT)
			return status == OK;
		else if (leaf->empty())
			except("index entry too large to insert");
		// split
		++modified;
		Mmoffset leftoff = leaf->split(dest, x, off);
		LeafNode* left = (LeafNode*) dest->adr(leftoff);
		++nnodes;
		verify(OK == (x <= left->slots.back() ? left->insert(x) : leaf->insert(x)));
		Key key = keydup(left->slots.back().key);
		off = leftoff;

		// insert up the tree as necessary
		for (--i; i >= 0; --i)
			{
			if (nodes[i]->insert(key, off))
				return true;
			// else split
			Mmoffset tleftoff = nodes[i]->split(dest, key);
			TreeNode* tleft = (TreeNode*) dest->adr(tleftoff);
			++nnodes;
			verify(tleft->slots.back().key < key ? nodes[i]->insert(key, off) : tleft->insert(key, off));
			key = keydup(tleft->slots.back().key);
			tleft->lastoff = tleft->slots.back().adr;
			tleft->slots.pop_back();
			off = tleftoff;
			}
		// create new root
		Mmoffset roff = dest->alloc(NODESIZE);
		TreeNode* r = new(dest->adr(roff)) TreeNode;
		++nnodes;
		r->insert(key, off);
		r->lastoff = root_;
		root_ = roff;
		++treelevels;
		verify(treelevels < MAXLEVELS);
		return true ;
		}
	// erase --------------------------------------------------------
	bool erase(const Key& key)
		{
		TreeNode* nodes[MAXLEVELS];

		// search down the tree
		Mmoffset off = root();
		int i;
		for (i = 0; i < treelevels; ++i)
			{
			nodes[i] = (TreeNode*) dest->adr(off);
			off = nodes[i]->find(key);
			}
		// erase the key and data from the leaf
		LeafNode* leaf = (LeafNode*) dest->adr(off);
		if (! leaf->erase(key))
			return false;
		if (! leaf->empty() || treelevels == 0)
			return true;	// this is the usual path
		leaf->unlink(dest);
		--nnodes;
		++modified;
		// erase empty nodes up the tree as necessary
		for (--i; i > 0; --i)
			{
			nodes[i]->erase(key);
			if (! nodes[i]->empty())
				return true;
			--nnodes;
			}
		nodes[0]->erase(key);
		off = root_;
		for (; treelevels > 0; --treelevels)
			{
			TreeNode* node = (TreeNode*) dest->adr(off);
			if (node->slots.size() > 0)
				break ;
			off = node->lastoff.unpack();
			--nnodes;
			}
		root_ = off;
		return true;
		}
	// NOTE: increment is only for Filters since it refers to count
	bool increment(const Key& key)
		{
		Mmoffset off = root();
		for (int i = 0; i < treelevels; ++i)
			{
			TreeNode* node = (TreeNode*) dest->adr(off);
			off = node->find(key);
			}
		LeafNode* leaf = (LeafNode*) dest->adr(off);
		LeafSlotsIterator t = std::lower_bound(leaf->slots.begin(), leaf->slots.end(), LeafSlot(key));
		if ((*t).key == key)
			{
			++(*t).count; // Note: couldn't do this for Index
			return true;
			}
		else
			return false;
		}

	//---------------------------------------------------------------

	float rangefrac(const Key& from, const Key& to)
		{
		const float MIN_FRAC = 1e-6f;
		if (isEmpty())
			return MIN_FRAC;

		bool fromMinimal = isMinimal(from);
		bool toMaximal = isMaximal(to);
		if (fromMinimal && toMaximal)
			return 1;

		//con() << "rangefrac " << from << " ... " << to << endl;
		float* rootChildFracs = getChildFracs(rootNode());

		float fromPos = fromMinimal ? 0 : fracPos(from, rootChildFracs);
		float toPos = toMaximal ? 1 : fracPos(to, rootChildFracs);
		//con() << "fromPos " << fromPos << " toPos " << toPos << " = " << max(toPos - fromPos, MIN_FRAC) << endl;

		return max(toPos - fromPos, MIN_FRAC);
	}

	bool isEmpty()
		{
		if (! root_)
			return true;
		if (treelevels > 0)
			return false;
		LeafNode* node = (LeafNode*) dest->adr(root());
		return node->slots.size() == 0;
		}

	static bool isMinimal(const Key& key)
		{
		for (int i = 0; i < key.size(); ++i)
			if (key.getraw(i) != "")
				return false;
		return true;
		}

	static bool isMaximal(const Key& key)
		{
		if (key.size() == 0)
			return false;
		for (int i = 0; i < key.size(); ++i)
			if (key.getraw(i) != "\x7f")
				return false;
		return true;
		}

	float* getChildFracs(Node* node)
		{
		int n = node->size();
		int *childSizes = (int*) _alloca(n * sizeof (int));
		int total = 0;
		//con() << "child sizes";
		for (int i = 0; i < n; ++i)
			{
			total += childSizes[i] = node->level() >= treelevels ? 1 : childNode(node, i)->size();
			//con() << " " << childSizes[i];
			}
		//con() << endl;
		float* childFracs = new(noptrs) float[n];
		for (int i = 0; i < n; ++i)
			childFracs[i] = (float) childSizes[i] / total;
		return childFracs;
		}

	float fracPos(const Key& key, float* childFracs)
		{
		//con() << "fracPos " << key << endl;
		Node* node = rootNode();
		int pos = node->findPos(key);
		float fracPos = 0;
		for (int i = 0; i < pos; ++i)
			fracPos += childFracs[i];
		//OstreamStr oss;
		//oss << "fracPos " << node->size() << "^" << pos << " " << fracPos;

		if (treelevels > 0)
			{
			float portion = childFracs[pos];
			node = childNode(node, pos);
			childFracs = getChildFracs(node);
			pos = node->findPos(key);
			for (int i = 0; i < pos; ++i)
				fracPos += portion * childFracs[i];
			//oss << ", " << node->size() << "^" << pos << " " << fracPos;
			if (treelevels > 1)
				{
				portion *= childFracs[pos];
				node = childNode(node, pos);
				pos = node->findPos(key);
				fracPos += portion * pos / node->size();
				//oss << ", " << node->size() << "^" << pos << " " << fracPos;

				if (treelevels > 2)
					{
					portion /= node->size();
					fracPos += portion * 0.5f;
					//oss << ", " << fracPos;
					}
				}
			}
		//con() << oss.gcstr() << endl;
		return fracPos;
		}

	Node* rootNode()
		{
		if (treelevels)
			return new TNode(0, (TreeNode*)dest->adr(root()));
		else
			return new LNode(0, (LeafNode*)dest->adr(root()));
		}

	Node* childNode(Node* node, int pos) const
		{
		int level = node->level() + 1;
		verify(level <= treelevels);
		Mmoffset adr = node->adr(pos);
		if (level < treelevels)
			return new TNode(level, (TreeNode*)dest->adr(adr));
		else
			return new LNode(level, (LeafNode*)dest->adr(adr));
		}
/*	void printree(Mmoffset off = NIL, int level = 0)
		{
		if (off == NIL)
			off = root;
		if (level < treelevels)
			{
			TreeNode* tn = (TreeNode*) dest->adr(off);
			int i = 0;
			for (; i < tn->slots.size(); ++i)
				{
				printree(tn->slots[i].adr, level + 1);
				for (int j = 0; j < level; ++j)
					cout << "    ";
				cout << i << ": " << tn->slots[i].key << endl;
				}
			if (i == 0)
				{
				for (int j = 0; j < level; ++j)
					cout << "    ";
				cout << "-o-" << endl;
				}
			printree(tn->lastoff, level + 1);
			}
		else
			{
			LeafNode* ln = (LeafNode*) dest->adr(off);
			int i = 0;
			for (; i < ln->slots.size(); ++i)
				{
				for (int j = 0; j < level; ++j)
					cout << "    ";
				cout << i << ": " << ln->slots[i].key << endl;
				}
			if (level == 0 && i == 0)
				cout << "- empty -";
			}
		if (level == 0)
			{ cout << endl; }
		} */
	void getinfo(Record& r) const
		{
		r.addmmoffset(root_);
		r.addval(treelevels);
		r.addval(nnodes);
		}
	int get_nnodes() const
		{ return nnodes; }
private:
	Mmoffset root_;
	Mmoffset root()
		{
		if (! root_)
			{
			verify(nnodes == 0);
			++nnodes;
			root_ = dest->alloc(NODESIZE);
			new(dest->adr(root_)) LeafNode;
			}
		verify(root_ >= 0);
		return root_;
		}
	int treelevels;	// not including leaves
	int nnodes;
	enum { MAXLEVELS = 20 };
	ulong modified;
	Dest* dest;
	};

#endif
