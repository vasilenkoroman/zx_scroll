#include "suffix_tree.h"

#include <algorithm>

using namespace std;

template <typename T>
State SuffixTree<T>::go(State st, int l, int r)
{
	while (l < r)
	{
		ensureNodeSize(st.v);

		if (st.pos == t[st.v].len())
		{
			st = State{ t[st.v].get(s[l]), 0 };
			if (st.v == -1)  
				return st;
		}
		else
		{
			if (s[t[st.v].l + st.pos] != s[l])
				return State{ -1, -1 };
			if (r - l < t[st.v].len() - st.pos)
				return State{ st.v, st.pos + r - l };
			l += t[st.v].len() - st.pos;
			st.pos = t[st.v].len();
		}
	}
	return st;
}

template <typename T>
void SuffixTree<T>::ensureNodeSize(int x1, int x2, int x3)
{
	int value = std::max(std::max(x1, x2), x3);
	if ((int) t.size() <= value)
		t.resize(value + 1);
}

template <typename T>
int SuffixTree<T>::split(State st)
{
	ensureNodeSize(st.v);

	if (st.pos == t[st.v].len())
		return st.v;
	if (st.pos == 0)
		return t[st.v].par;
	Node v = t[st.v];
	int id = sz++;

	ensureNodeSize(id, v.par);
	t[id] = Node(v.l, v.l+st.pos, v.par);
	t[v.par].get( s[v.l] ) = id;
	t[id].get( s[v.l+st.pos] ) = st.v;
	t[st.v].par = id;
	t[st.v].l += st.pos;
	return id;
}
 
template <typename T>
int SuffixTree<T>::get_link (int v)
{
	if (t[v].link != -1)  return t[v].link;
	if (t[v].par == -1)  return 0;
	int to = get_link (t[v].par);
	return t[v].link = split(go(State{ to, t[to].len() }, t[v].l + (t[v].par == 0), t[v].r));
}
 
template <typename T>
void SuffixTree<T>::tree_extend (int pos)
{
	for(;;) 
	{
		State nptr = go(ptr, pos, pos+1);
		if (nptr.v != -1) {
			ptr = nptr;
			return;
		}
 
		int mid = split (ptr);
		int leaf = sz++;
		ensureNodeSize(leaf, mid);
		t[leaf] = Node(pos, s.size(), mid);
		t[mid].get( s[pos] ) = leaf;
 
		ptr.v = get_link (mid);
		ptr.pos = t[ptr.v].len();
		if (!mid)  break;
	}
}

template <typename T>
void SuffixTree<T>::build_tree()
{
	for (int i = 0; i < s.size(); ++i)
		tree_extend (i);
}

template class SuffixTree<char>;
template class SuffixTree<uint16_t>;
