#include <UFG/Compiler.hpp>

#include <UFG/ResourceNode.hpp>

#include <algorithm>
#include <stack>
#include <cassert>

using namespace Ubpa;

using namespace Ubpa::UFG;
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
	const tuple<bool, Compiler::Result> fail{ false, {} };

	Result rst;
	const auto& passes = fg.GetPassNodes();

	for (size_t i = 0; i < passes.size(); i++) {
		const auto& pass = passes[i];
		for (const auto& input : pass.Inputs())
			rst.rsrc2info[input].readers.push_back(i);
		for (const auto& output : pass.Outputs())
			rst.rsrc2info[output].writer = i;
	}

	for (const auto& moveNode : fg.GetMoveNodes()) {
		auto src = moveNode.GetSourceNodeIndex();
		auto dst = moveNode.GetDestinationNodeIndex();
		if (rst.moves_src2dst.find(src) != rst.moves_src2dst.end())
			return fail; // move out more than once
		rst.moves_src2dst.emplace(src, dst);
	}

	// pruning move
	std::set<size_t> deleteMoves;
	auto iter = rst.moves_src2dst.begin();
	while (iter != rst.moves_src2dst.end()) {
		auto& [src, dst] = *iter;
		if (deleteMoves.find(src) != deleteMoves.end()) {
			++iter;
			continue;
		}

		const auto& info = rst.rsrc2info.find(dst)->second;
		auto next = rst.moves_src2dst.find(dst);
		if (info.writer == static_cast<size_t>(-1)
			&& info.readers.empty()
			&& next != rst.moves_src2dst.end())
		{
			dst = next->second;
			deleteMoves.insert(dst);
		}
		else
			++iter;
	}
	for (auto idx : deleteMoves)
		rst.moves_src2dst.erase(idx);

	// move_src2dst -> move_dst2src
	for (const auto& [src, dst] : rst.moves_src2dst) {
		auto [iter, success] = rst.moves_dst2src.emplace(dst, src);
		assert(success);
	}

	// init rst.passgraph.adjList
	rst.passgraph.adjList.reserve(passes.size());
	for (size_t i = 0; i < passes.size(); i++)
		rst.passgraph.adjList.try_emplace(i);

	// resource order (write -> readers)
	for (const auto& [name, info] : rst.rsrc2info) {
		/*if (info.writer == static_cast<size_t>(-1) && info.readers.size() == 0)
			return { false, {} };*/
		if (info.writer == static_cast<size_t>(-1))
			continue;

		auto& adj = rst.passgraph.adjList[info.writer];
		for (const auto& reader : info.readers)
			adj.insert(reader);
	}

	// move order
	for (const auto& [src, dst] : rst.moves_src2dst) {
		const auto& info_dst = rst.rsrc2info.find(dst)->second;
		const auto& info_src = rst.rsrc2info.find(src)->second;
		if (info_dst.writer != static_cast<size_t>(-1)) {
			if (!info_src.readers.empty()) {
				for (auto reader_src : info_src.readers)
					rst.passgraph.adjList[reader_src].insert(info_dst.writer);
			}
			else if (info_src.writer != static_cast<size_t>(-1))
				rst.passgraph.adjList[info_src.writer].insert(info_dst.writer);
			// else [do nothing]
		}
		else if (!info_dst.readers.empty()) {
			if (!info_src.readers.empty()) {
				for (auto reader_src : info_src.readers) {
					rst.passgraph.adjList[reader_src].insert(
						info_dst.readers.begin(),
						info_dst.readers.end()
					);
				}
			}
			else if (info_src.writer != static_cast<size_t>(-1)) {
				rst.passgraph.adjList[info_src.writer].insert(
					info_dst.readers.begin(),
					info_dst.readers.end()
				);
			}
			// else [[do nothing]];
		}
		// else [[do nothing]];
	}

	// sortedPasses : order -> index
	auto [success, sortedPasses] = rst.passgraph.TopoSort();
	if (!success)
		return { false,{} };

	vector<size_t> index2order(sortedPasses.size());
	for (size_t i = 0; i < sortedPasses.size(); i++)
		index2order[sortedPasses[i]] = i;

	// set resource's first last
	for (auto& [rsrcNodeIdx, info] : rst.rsrc2info) {
		if (info.writer != static_cast<size_t>(-1)) {
			info.first = index2order[info.writer];
			info.last = info.first;
			for (const auto& reader : info.readers)
				info.last = max(info.last, index2order[reader]);
		}
		else if (!info.readers.empty()) {
			size_t first = static_cast<size_t>(-1); // max size_t
			for (const auto& reader : info.readers)
				first = min(first, index2order[reader]);
			info.first = first;
		}
		//else [[do nothing]]; // info.first = static_cast<size_t>(-1)

		info.last = info.first;
		for (const auto& reader : info.readers)
			info.last = max(info.last, index2order[reader]);

		assert(info.first == static_cast<size_t>(-1) || info.last != static_cast<size_t>(-1));

		size_t firstPassIdx = info.first != static_cast<size_t>(-1) ? sortedPasses[info.first]
			: static_cast<size_t>(-1);
		size_t lastPassIdx = info.last != static_cast<size_t>(-1) ? sortedPasses[info.last]
			: static_cast<size_t>(-1);
		if (rst.moves_dst2src.find(rsrcNodeIdx) == rst.moves_dst2src.end())
			rst.idx2info[firstPassIdx].constructRsrcs.push_back(rsrcNodeIdx);
		if (rst.moves_src2dst.find(rsrcNodeIdx) != rst.moves_src2dst.end())
			rst.idx2info[lastPassIdx].moveRsrcs.push_back(rsrcNodeIdx);
		else
			rst.idx2info[lastPassIdx].destructRsrcs.push_back(rsrcNodeIdx);
	}

	rst.sortedPasses = move(sortedPasses);
	return { true, rst };
}

UGraphviz::Graph Compiler::Result::PassGraph::ToGraphvizGraph(const FrameGraph& fg) const {
	UGraphviz::Graph graph("Compiler Result Pass Graph", true);

	auto& registry = graph.GetRegistry();

	graph
		.RegisterGraphNodeAttr("shape", "ellipse")
		.RegisterGraphNodeAttr("color", "#6597AD")
		.RegisterGraphEdgeAttr("color", "#B54E4C")
		.RegisterGraphNodeAttr("style", "filled")
		.RegisterGraphNodeAttr("fontcolor", "white")
		.RegisterGraphNodeAttr("fontname", "consolas");

	for (const auto& [src, dsts] : adjList)
		graph.AddNode(registry.RegisterNode(fg.GetPassNodes().at(src).Name()));

	for (const auto& [src, dsts] : adjList) {
		auto idx_src = registry.GetNodeIndex(fg.GetPassNodes().at(src).Name());
		for (auto dst : dsts) {
			auto idx_dst = registry.GetNodeIndex(fg.GetPassNodes().at(dst).Name());
			graph.AddEdge(registry.RegisterEdge(idx_src, idx_dst));
		}
	}

	return graph;
}
