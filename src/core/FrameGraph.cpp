#include <UFG/core/FrameGraph.h>

#include <cassert>

using namespace Ubpa;
using namespace Ubpa::UFG;

bool FrameGraph::IsRegisteredResourceNode(std::string_view name) const {
	return name2rsrcNodeIdx.find(name) != name2rsrcNodeIdx.end();
}

size_t FrameGraph::GetResourceNodeIndex(std::string_view name) const {
	assert(IsRegisteredResourceNode(name));
	return name2rsrcNodeIdx.find(name)->second;
}

size_t FrameGraph::RegisterResourceNode(ResourceNode node) {
	assert(!IsRegisteredResourceNode(node.Name()));
	size_t idx = resourceNodes.size();
	name2rsrcNodeIdx.emplace(node.Name(), idx);
	resourceNodes.push_back(std::move(node));
	return idx;
}

size_t FrameGraph::RegisterResourceNode(std::string node) {
	return RegisterResourceNode(ResourceNode{ node });
}

bool FrameGraph::IsRegisteredPassNode(std::string_view name) const {
	return name2passNodeIdx.find(name) != name2passNodeIdx.end();
}

size_t FrameGraph::GetPassNodeIndex(std::string_view name) const {
	assert(IsRegisteredPassNode(name));
	return name2passNodeIdx.find(name)->second;
}

size_t FrameGraph::RegisterPassNode(PassNode node) {
	assert(!IsRegisteredPassNode(node.Name()));
	size_t idx = passNodes.size();
	name2passNodeIdx.emplace(node.Name(), idx);
	passNodes.push_back(std::move(node));
	return idx;
}

size_t FrameGraph::RegisterPassNode(
	std::string name,
	std::vector<size_t> inputs,
	std::vector<size_t> outputs
) {
	return RegisterPassNode(PassNode{ std::move(name), std::move(inputs), std::move(outputs) });
}

void FrameGraph::Clear() noexcept {
	name2rsrcNodeIdx.clear();
	name2passNodeIdx.clear();
	resourceNodes.clear();
	passNodes.clear();
}


UGraphviz::Graph FrameGraph::ToGraphvizGraph() const {
	UGraphviz::Graph graph(name, true);

	auto& registry = graph.GetRegistry();

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

	for (const auto& rsrcNode : resourceNodes) {
		auto rsrcIndex = registry.RegisterNode(rsrcNode.Name());
		subgraph_rsrc.AddNode(rsrcIndex);
	}

	for (const auto& passNode : passNodes) {
		size_t passIndex = registry.RegisterNode(passNode.Name());
		subgraph_pass.AddNode(passIndex);

		for (size_t rsrcNodeIndex : passNode.Inputs()) {
			const auto& rsrcNodeName = resourceNodes[rsrcNodeIndex].Name();
			auto edgeIndex = registry.RegisterEdge(registry.GetNodeIndex(rsrcNodeName), passIndex);
			subgraph_read.AddEdge(edgeIndex);
		}

		for (size_t rsrcNodeIndex : passNode.Outputs()) {
			const auto& rsrcNodeName = passNodes[rsrcNodeIndex].Name();
			auto edgeIndex = registry.RegisterEdge(passIndex, registry.GetNodeIndex(rsrcNodeName));
			subgraph_write.AddEdge(edgeIndex);
		}
	}

	graph
		.RegisterGraphNodeAttr("style", "filled")
		.RegisterGraphNodeAttr("fontcolor", "white")
		.RegisterGraphNodeAttr("fontname", "consolas");

	return graph;
}
