#include <UFG/core/Compiler.h>

#include <UFG/core/ResourceNode.h>

#include <algorithm>
#include <stack>

using namespace Ubpa;

using namespace Ubpa::FG;
using namespace std;

tuple<bool, vector<size_t>> Compiler::Result::PassGraph::TopoSort() const {
	unordered_map<size_t, size_t> in_degree_map;

	for (const auto& [parent, children] : adjList)
		in_degree_map.emplace(parent, 0);

	for (const auto& [parent, children] : adjList) {
		for (const auto& child : children)
			in_degree_map[child] += 1;
	}

	stack<size_t> zero_in_degree_vertices;
	vector<size_t> sorted_vertices;

	for (const auto& [v, d] : in_degree_map) {
		if (d == 0)
			zero_in_degree_vertices.push(v);
	}

	while (!zero_in_degree_vertices.empty()) {
		auto v = zero_in_degree_vertices.top();
		zero_in_degree_vertices.pop();
		sorted_vertices.push_back(v);
		in_degree_map.erase(v);
		for (auto child : adjList.find(v)->second) {
			auto target = in_degree_map.find(child);
			if (target == in_degree_map.end())
				continue;
			target->second--;
			if (target->second == 0)
				zero_in_degree_vertices.push(child);
		}
	}

	if (!in_degree_map.empty())
		return { false, vector<size_t>{} };

	return { true, sorted_vertices };
}

tuple<bool, Compiler::Result> Compiler::Compile(const FrameGraph& fg) {
	Result rst;
	const auto& passes = fg.GetPassNodes();

	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		for (const auto& input : pass.Inputs())
			rst.rsrc2info[input].readers.push_back(i);
		for (const auto& output : pass.Outputs())
			rst.rsrc2info[output].writer = i;
	}

	for (const auto& [name, info] : rst.rsrc2info) {
		auto& adj = rst.passgraph.adjList[info.writer];
		for (const auto& reader : info.readers)
			adj.insert(reader);
	}

	// sortedPasses : order 2 index
	auto [success, sortedPasses] = rst.passgraph.TopoSort();
	if (!success)
		return { false,{} };

	vector<size_t> index2order(sortedPasses.size());
	for (size_t i = 0; i < sortedPasses.size(); i++)
		index2order[sortedPasses[i]] = i;

	for (auto& [rsrcNodeIdx, info] : rst.rsrc2info) {
		info.first = index2order[info.writer];

		if (!info.readers.empty()) {
			size_t last = 0;
			for (const auto& reader : info.readers)
				last = max(last, index2order[reader]);
			info.last = last;
		}
		else
			info.last = info.first;

		rst.idx2info[info.writer].constructRsrcs.push_back(rsrcNodeIdx);
		rst.idx2info[sortedPasses[info.last]].destructRsrcs.push_back(rsrcNodeIdx);
	}

	rst.sortedPasses = move(sortedPasses);
	return { true, rst };
}
