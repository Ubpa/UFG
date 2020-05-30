#include <UFG/FrameGraph.h>

#include <iostream>

#include <algorithm>

#include <cassert>

using namespace Ubpa::FG;
using namespace std;

const CompileResult& FrameGraph::Compile() {
	compileResult.Clear();

	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		for (const auto& input : pass.Inputs())
			compileResult.resource2info[input.name].readers.push_back(i);
		for (const auto& output : pass.Outputs())
			compileResult.resource2info[output.name].writer = i;
	}
	
	for (const auto& [name, info] : compileResult.resource2info) {
		auto& adj = compileResult.passgraph.adjList[info.writer];
		for (const auto& reader : info.readers)
			adj.insert(reader);
	}
	
	auto [success, sortedPasses] = compileResult.passgraph.TopoSort();
	assert(success);

	compileResult.sortedPasses = std::move(sortedPasses);

	std::unordered_map<size_t, size_t> index2order;
	for (size_t i = 0; i < compileResult.sortedPasses.size(); i++)
		index2order.emplace(compileResult.sortedPasses[i], i);
	for (auto& [name, info] : compileResult.resource2info) {
		if (info.readers.empty())
			continue;
		size_t last = 0;
		for (const auto& reader : info.readers)
			last = std::min(last, index2order.find(reader)->second);
		info.last = last;
	}

	return compileResult;
}
