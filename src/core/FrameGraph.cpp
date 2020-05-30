#include <UFG/FrameGraph.h>

#include <iostream>

#include <algorithm>

#include <cassert>

using namespace Ubpa::FG;
using namespace std;

FrameGraph::~FrameGraph() {
	delete rsrcMngr;
}

void FrameGraph::Compile() {
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
		if (imports.find(name) != imports.end())
			info.last = static_cast<size_t>(-1);
		else if (!info.readers.empty()) {
			size_t last = 0;
			for (const auto& reader : info.readers)
				last = std::max(last, index2order.find(reader)->second);
			info.last = last;
		}
		else
			info.last = info.writer;
	}
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
		
		map<string, const Resource*> passResources;

		auto findRsrc = [&](ResourceDecs decs) -> Resource* {
			Resource* rsrc;
			auto target = resourceMap.find(decs.name);
			if (target == resourceMap.end()) {
				rsrc = &resourceMap[decs.name];
				auto import_target = imports.find(decs.name);
				if (import_target != imports.end())
					*rsrc = import_target->second;
				else
					*rsrc = rsrcMngr->Request(decs.type, decs.state);
			}
			else
				rsrc = &target->second;

			if (rsrc->state != decs.state)
				rsrcMngr->Transition(rsrc, decs.state);

			return rsrc;
		};

		for (auto input : pass.Inputs())
			passResources[input.name] = findRsrc(input);
		for (auto output : pass.Outputs())
			passResources[output.name] = findRsrc(output);

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

void FrameGraph::Clear() {
	compileResult.Clear();
	imports.clear();
	passes.clear();
}
