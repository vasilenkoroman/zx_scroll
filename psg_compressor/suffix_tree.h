#pragma once

#include <string>
#include <map>
#include <vector>

class SuffixTree
{
public:

    SuffixTree(const std::string& s);

    void build_tree();

private:
	struct Node
	{
		int l, r, par, link;
		std::map<char, int> next;

		Node(int l = 0, int r = 0, int par = -1) :
			l(l), r(r), par(par), link(-1) {}
		int len() { return r - l; }
		int& get(char c)
		{
			if (!next.count(c))  next[c] = -1;
			return next[c];
		}
	};

	struct State
	{
		int v = 0;
		int pos = 0;
	};
	State ptr;


	std::string s;
	std::vector<Node> t;
    int sz;

private:
	State go(State st, int l, int r);
	int split(State st);
	int get_link(int v);
	void tree_extend(int pos);
	void ensureNodeSize(int x1, int x2 = -1, int x3 = -1);
};
