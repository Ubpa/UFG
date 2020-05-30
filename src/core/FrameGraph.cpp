#include <UFG/FrameGraph.h>

#include <iostream>

#include <algorithm>

#include <cassert>

using namespace Ubpa::FG;
using namespace std;

FrameGraph::~FrameGraph() {
	delete rsrcMngr;
}

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
			last = std::max(last, index2order.find(reader)->second);
		info.last = last;
	}

	return compileResult;
}

void FrameGraph::Execute() {
	map<string, Resource> resourceMap;
	map<size_t, vector<string>> needRecycle;

	for (const auto& [name, info] : compileResult.resource2info) {
		if (info.last == static_cast<size_t>(-1))
			continue;

		needRecycle[info.last].push_back(name);
	}

	for (auto i : compileResult.sortedPasses) {
		const auto& pass = passes[i];
		
		map<string, Resource> passResources;

		for (auto input : pass.Inputs()) {
			auto target = resourceMap.find(input.name);
			if (target == resourceMap.end())
				resourceMap[input.name] = rsrcMngr->Request(input.type, input.state);
			else {
				auto& resource = target->second;
				if (resource.state != input.state)
					rsrcMngr->Transition(resource, input.state);
			}

			passResources[input.name] = resourceMap[input.name];
		}
		for (auto output : pass.Outputs()) {
			auto target = resourceMap.find(output.name);
			if (target == resourceMap.end())
				resourceMap[output.name] = rsrcMngr->Request(output.type, output.state);
			else {
				auto& resource = target->second;
				if (resource.state != output.state)
					rsrcMngr->Transition(resource, output.state);
			}

			passResources[output.name] = resourceMap[output.name];
		}

		pass.Execute(passResources);

		auto target = needRecycle.find(i);
		if (target != needRecycle.end()) {
			for (const auto& name : target->second) {
				rsrcMngr->Recycle(resourceMap[name]);
				resourceMap.erase(name);
			}
		}
	}
}
