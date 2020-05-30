#include <UFG/FrameGraph.h>

#include <set>
#include <stack>
#include <iostream>

#include <cassert>

using namespace Ubpa::FG;
using namespace std;

struct FrameGraph::CompileResult {
	struct Info {
		// first == writer
		// size_t first; // index in sortedPass
		size_t last; // index in sortedPass
		std::vector<size_t> readers;
		size_t writer;
	};

	struct PassGraph {
		tuple<bool, vector<size_t>> TopoSort() const;
		unordered_map<size_t, set<size_t>> adjList;
	};

	void Clear() {
		resource2info.clear();
		sortedPasses.clear();
	}

	unordered_map<string, Info> resource2info;
	vector<size_t> sortedPasses;
};

FrameGraph::FrameGraph()
	: compileResult{ new CompileResult } {}

FrameGraph::~FrameGraph() {
	delete compileResult;
}

void FrameGraph::Compile() {
	compileResult->Clear();

	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		for (const auto& input : pass.Inputs())
			compileResult->resource2info[input.name].readers.push_back(i);
		for (const auto& output : pass.Outputs())
			compileResult->resource2info[output.name].writer = i;
	}

	CompileResult::PassGraph passGraph;
	for (const auto& [name, info] : compileResult->resource2info) {
		auto& adj = passGraph.adjList[info.writer];
		for (const auto& reader : info.readers)
			adj.insert(reader);
	}
	
	auto [success, sortedPasses] = passGraph.TopoSort();
	assert(success);

	compileResult->sortedPasses = std::move(sortedPasses);
	for (auto i : compileResult->sortedPasses)
		cout << i << ": " << passes[i].Name() << endl;
}


// =========================

tuple<bool, vector<size_t>> FrameGraph::CompileResult::PassGraph::TopoSort() const {
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
