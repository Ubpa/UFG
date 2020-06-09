#include <UFG/core/FrameGraph.h>

using namespace Ubpa;

size_t FG::FrameGraph::AddResourceNode(std::string name) {
	size_t idx = rsrcNodes.size();
	rsrcname2idx[name] = idx;
	rsrcNodes.emplace_back(std::move(name));
	return idx;
}

size_t FG::FrameGraph::AddPassNode(
	std::string name,
	std::vector<size_t> inputs,
	std::vector<size_t> outputs)
{
	size_t idx = passNodes.size();
	passnodename2idx[name] = idx;
	passNodes.emplace_back(std::move(name), std::move(inputs), std::move(outputs));
	return idx;
}

void FG::FrameGraph::Clear() {
	rsrcname2idx.clear();
	passnodename2idx.clear();
	rsrcNodes.clear();
	passNodes.clear();
}

Graphviz::Graph FG::FrameGraph::ToGraphvizGraph() const {
	Graphviz::Graph graph(Name, true);

	auto& registrar = graph.GetRegistrar();

	auto& subgraph_rsrc = graph.GenSubgraph("Resource Nodes");
	auto& subgraph_pass = graph.GenSubgraph("Pass Nodes");

	auto& subgraph_read = graph.GenSubgraph("Read Edges");
	auto& subgraph_write = graph.GenSubgraph("Write Edges");

	subgraph_rsrc
		.RegisterGraphNodeAttr("color", "#F79646")
		.RegisterGraphNodeAttr("shape", "box");

	subgraph_pass
		.RegisterGraphNodeAttr("shape", "ellipse")
		.RegisterGraphNodeAttr("color", "#6597AD");

	subgraph_rsrc
		.RegisterGraphNodeAttr("color", "#F79646")
		.RegisterGraphNodeAttr("shape", "box");

	subgraph_read.RegisterGraphEdgeAttr("color", "#9BBB59");
	subgraph_write.RegisterGraphEdgeAttr("color", "#B54E4C");

	for (const auto& rsrcNode : rsrcNodes) {
		auto rsrcIdx = registrar.RegisterNode(rsrcNode.Name());
		subgraph_rsrc.AddNode(rsrcIdx);
	}

	for (const auto& passNode : passNodes) {
		size_t passIdx = registrar.RegisterNode(passNode.Name());
		subgraph_pass.AddNode(passIdx);

		for (size_t rsrcNodeIdx : passNode.Inputs()) {
			const auto& rsrcNodeName = rsrcNodes[rsrcNodeIdx].Name();
			auto edgeIdx = registrar.RegisterEdge(registrar.GetNodeIdx(rsrcNodeName), passIdx);
			subgraph_read.AddEdge(edgeIdx);
		}

		for (size_t rsrcNodeIdx : passNode.Outputs()) {
			const auto& rsrcNodeName = rsrcNodes[rsrcNodeIdx].Name();
			auto edgeIdx = registrar.RegisterEdge(passIdx, registrar.GetNodeIdx(rsrcNodeName));
			subgraph_write.AddEdge(edgeIdx);
		}
	}

	graph
		.RegisterGraphNodeAttr("style", "filled")
		.RegisterGraphNodeAttr("fontcolor", "white")
		.RegisterGraphNodeAttr("fontname", "consolas");

	return graph;
}
