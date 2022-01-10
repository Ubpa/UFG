#include <UFG/FrameGraph.hpp>

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
	PassNode::Type type,
	std::string name,
	std::vector<size_t> inputs,
	std::vector<size_t> outputs
) {
	return RegisterPassNode(PassNode{ type, std::move(name), std::move(inputs), std::move(outputs) });
}

size_t FrameGraph::RegisterGeneralPassNode(
	std::string name,
	std::vector<size_t> inputs,
	std::vector<size_t> outputs)
{
	return RegisterPassNode(PassNode::Type::General, std::move(name), std::move(inputs), std::move(outputs));
}

size_t FrameGraph::RegisterCopyPassNode(
	std::string name,
	std::vector<size_t> inputs,
	std::vector<size_t> outputs)
{
	return RegisterPassNode(PassNode::Type::Copy, std::move(name), std::move(inputs), std::move(outputs));
}

std::string FrameGraph::GenerateCopyPassNodeName() const
{
	return std::string("Copy#") + std::to_string(passNodes.size());
}

size_t FrameGraph::RegisterCopyPassNode(
	std::vector<size_t> inputs,
	std::vector<size_t> outputs)
{
	return RegisterCopyPassNode(GenerateCopyPassNodeName(), std::move(inputs), std::move(outputs));
}

//
// Move
/////////

bool FrameGraph::IsRegisteredMoveNode(size_t dst, size_t src) const {
	auto target = srcRsrcNodeIdx2moveNodeIdx.find(src);
	if (target == srcRsrcNodeIdx2moveNodeIdx.end())
		return false;
	size_t idx = target->second;
	return moveNodes[idx].GetDestinationNodeIndex() == dst;
}

bool FrameGraph::IsMovedOut(size_t src) const {
	return srcRsrcNodeIdx2moveNodeIdx.find(src) != srcRsrcNodeIdx2moveNodeIdx.end();
}

bool FrameGraph::IsMovedIn(size_t dst) const {
	return dstRsrcNodeIdx2moveNodeIdx.find(dst) != dstRsrcNodeIdx2moveNodeIdx.end();
}

size_t FrameGraph::GetMoveNodeIndex(size_t dst, size_t src) const {
	assert(IsRegisteredMoveNode(dst, src));
	return srcRsrcNodeIdx2moveNodeIdx.find(src)->second;
}

size_t FrameGraph::GetMoveSourceNodeIndex(size_t dst) const {
	assert(IsMovedIn(dst));
	return moveNodes[dstRsrcNodeIdx2moveNodeIdx.find(dst)->second].GetSourceNodeIndex();
}

size_t FrameGraph::GetMoveDestinationNodeIndex(size_t src) const {
	assert(IsMovedOut(src));
	return moveNodes[srcRsrcNodeIdx2moveNodeIdx.find(src)->second].GetDestinationNodeIndex();
}

size_t FrameGraph::RegisterMoveNode(MoveNode node) {
	assert(!IsRegisteredMoveNode(node.GetDestinationNodeIndex(), node.GetSourceNodeIndex()));
	size_t idx = moveNodes.size();
	moveNodes.push_back(node);
	srcRsrcNodeIdx2moveNodeIdx.emplace(node.GetSourceNodeIndex(), idx);
	dstRsrcNodeIdx2moveNodeIdx.emplace(node.GetDestinationNodeIndex(), idx);
	return idx;
}

size_t FrameGraph::RegisterMoveNode(size_t dst, size_t src) {
	return RegisterMoveNode(MoveNode{ dst,src });
}

void FrameGraph::Clear() noexcept {
	name2rsrcNodeIdx.clear();
	name2passNodeIdx.clear();
	dstRsrcNodeIdx2moveNodeIdx.clear();
	srcRsrcNodeIdx2moveNodeIdx.clear();
	resourceNodes.clear();
	passNodes.clear();
	moveNodes.clear();
}


UGraphviz::Graph FrameGraph::ToGraphvizGraph() const {
	UGraphviz::Graph graph(name, true);

	graph.RegisterGraphAttr(UGraphviz::Attrs_rankdir, "LR");

	auto& registry = graph.GetRegistry();

	auto& subgraph_rsrc = graph.GenSubgraph("Resource Nodes");
	auto& subgraph_pass = graph.GenSubgraph("Pass Nodes");

	auto& subgraph_read = graph.GenSubgraph("Read Edges");
	auto& subgraph_write = graph.GenSubgraph("Write Edges");
	auto& subgraph_move = graph.GenSubgraph("Move Edges");
	auto& subgraph_copy = graph.GenSubgraph("Copy Edges");

	subgraph_rsrc
		.RegisterGraphNodeAttr(UGraphviz::Attrs_shape, "box")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_color, "#F79646");

	subgraph_pass
		.RegisterGraphNodeAttr(UGraphviz::Attrs_shape, "record")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_color, "#6597AD");

	subgraph_read.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#9BBB59");
	subgraph_write.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#ED1C24");
	subgraph_move.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#F79646");

	subgraph_copy
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#A349A4")
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_dir, "back");

	for (const auto& rsrcNode : resourceNodes) {
		auto rsrcIndex = registry.RegisterNode(std::string{ rsrcNode.Name() });
		subgraph_rsrc.AddNode(rsrcIndex);
	}

	for (const auto& passNode : passNodes) {
		size_t passIndex = registry.RegisterNode(std::string{ passNode.Name() });
		subgraph_pass.AddNode(passIndex);

		for (size_t rsrcNodeIndex : passNode.Inputs()) {
			const auto& rsrcNodeName = resourceNodes[rsrcNodeIndex].Name();
			auto edgeIndex = registry.RegisterEdge(registry.GetNodeIndex(rsrcNodeName), passIndex);
			subgraph_read.AddEdge(edgeIndex);
		}

		for (size_t rsrcNodeIndex : passNode.Outputs()) {
			const auto& rsrcNodeName = resourceNodes[rsrcNodeIndex].Name();
			//auto edgeIndex = registry.RegisterEdge(passIndex, registry.GetNodeIndex(rsrcNodeName));
			//subgraph_write.AddEdge(edgeIndex);

			switch (passNode.GetType())
			{
			case PassNode::Type::General: {
				auto edgeIndex = registry.RegisterEdge(passIndex, registry.GetNodeIndex(rsrcNodeName));
				subgraph_write.AddEdge(edgeIndex);
			} break;
			case PassNode::Type::Copy: {
				auto edgeIndex = registry.RegisterEdge(registry.GetNodeIndex(rsrcNodeName), passIndex);
				subgraph_copy.AddEdge(edgeIndex);
			} break;
			default:
				assert(false); // no entry
				break;
			}
		}
	}

	for (const auto& moveNode : moveNodes) {
		auto srcNodeIdx = registry.GetNodeIndex(resourceNodes[moveNode.GetSourceNodeIndex()].Name());
		auto dstNodeIdx = registry.GetNodeIndex(resourceNodes[moveNode.GetDestinationNodeIndex()].Name());
		auto src2dst = registry.RegisterEdge(srcNodeIdx, dstNodeIdx);
		subgraph_move.AddEdge(src2dst);
	}

	graph
		.RegisterGraphNodeAttr(UGraphviz::Attrs_style, "filled")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_fontcolor, "white")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_fontname, "consolas");

	return graph;
}

UGraphviz::Graph FrameGraph::ToGraphvizGraph2() const {
	UGraphviz::Graph graph(name, true);

	graph.RegisterGraphAttr(UGraphviz::Attrs_rankdir, "LR");

	auto& registry = graph.GetRegistry();

	auto& subgraph_rsrc = graph.GenSubgraph("Resource Nodes");
	auto& subgraph_pass = graph.GenSubgraph("Pass Nodes");

	auto& subgraph_read = graph.GenSubgraph("Read Edges");
	auto& subgraph_write = graph.GenSubgraph("Write Edges");
	auto& subgraph_move = graph.GenSubgraph("Move Edges");
	auto& subgraph_copy = graph.GenSubgraph("Copy Edges");

	subgraph_rsrc
		.RegisterGraphNodeAttr(UGraphviz::Attrs_shape, "box")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_color, "#F79646");

	subgraph_pass
		.RegisterGraphNodeAttr(UGraphviz::Attrs_shape, "record")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_style, "filled")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_fillcolor, "#6597AD")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_fontcolor, "white");

	subgraph_read
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#9BBB59")
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_arrowhead, "none");

	subgraph_write
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#ED1C24")
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_arrowhead, "none");

	subgraph_move
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#F79646");

	subgraph_copy
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_color, "#A349A4")
		.RegisterGraphEdgeAttr(UGraphviz::Attrs_dir, "back");

	for (const auto& rsrcNode : resourceNodes) {
		auto rsrcIndex = registry.RegisterNode(std::string{ rsrcNode.Name() });
		subgraph_rsrc.AddNode(rsrcIndex);
	}

	for (const auto& passNode : passNodes) {
		size_t passIndex = registry.RegisterNode(std::string{ passNode.Name() });
		subgraph_pass.AddNode(passIndex);
		std::string label;
		//label += "{"; // begin pass
		label += std::string{ passNode.Name() };
		label += "|";
		label += "{"; // begin inout

		label += "{"; // begin in
		if (!passNode.Inputs().empty()) {
			for (size_t i = 0; i < passNode.Inputs().size(); ++i) {
				label += "<in_" + std::to_string(i) + "> ";
				label += resourceNodes[passNode.Inputs()[i]].Name();
				if (i != passNode.Inputs().size() - 1)
					label += "|";
			}
		}
		else
			label += "-";
		label += "}"; // end in

		label += "|";

		label += "{"; // begin out
		if (!passNode.Outputs().empty()) {
			for (size_t i = 0; i < passNode.Outputs().size(); ++i) {
				label += "<out_" + std::to_string(i) + "> ";
				label += resourceNodes[passNode.Outputs()[i]].Name();
				if (i != passNode.Outputs().size() - 1)
					label += "|";
			}
		}
		else
			label += "-";
		label += "}"; // end out

		label += "}"; // end inout

		registry.RegisterNodeAttr(passIndex, UGraphviz::Attrs_label, std::move(label));

		for (size_t i = 0; i < passNode.Inputs().size(); ++i) {
			size_t rsrcNodeIndex = passNode.Inputs()[i];
			const auto& rsrcNodeName = resourceNodes[rsrcNodeIndex].Name();
			auto edgeIndex = registry.RegisterEdge(registry.GetNodeIndex(rsrcNodeName), passIndex);
			subgraph_read.AddEdge(edgeIndex);
			registry.RegisterEdgePort(
				edgeIndex,
				{ .compass = UGraphviz::Registry::Port::Compass::E },
				{.ID = "in_" + std::to_string(i), .compass = UGraphviz::Registry::Port::Compass::W }
			);
		}
		for (size_t i = 0; i < passNode.Outputs().size(); ++i) {
			size_t rsrcNodeIndex = passNode.Outputs()[i];
			const auto& rsrcNodeName = resourceNodes[rsrcNodeIndex].Name();
			switch (passNode.GetType())
			{
			case PassNode::Type::General: {
				auto edgeIndex = registry.RegisterEdge(passIndex, registry.GetNodeIndex(rsrcNodeName));
				subgraph_write.AddEdge(edgeIndex);
				registry.RegisterEdgePort(
					edgeIndex,
					{ .ID = "out_" + std::to_string(i), .compass = UGraphviz::Registry::Port::Compass::E },
					{ .compass = UGraphviz::Registry::Port::Compass::W }
				);
			} break;
			case PassNode::Type::Copy: {
				auto edgeIndex = registry.RegisterEdge(registry.GetNodeIndex(rsrcNodeName), passIndex);
				subgraph_copy.AddEdge(edgeIndex);
				registry.RegisterEdgePort(
					edgeIndex,
					{ .compass = UGraphviz::Registry::Port::Compass::NW },
					{ .ID = "out_" + std::to_string(i), .compass = UGraphviz::Registry::Port::Compass::NE }
				);
			} break;
			default:
				assert(false); // no entry
				break;
			}
		}
	}

	for (const auto& moveNode : moveNodes) {
		auto srcNodeIdx = registry.GetNodeIndex(resourceNodes[moveNode.GetSourceNodeIndex()].Name());
		auto dstNodeIdx = registry.GetNodeIndex(resourceNodes[moveNode.GetDestinationNodeIndex()].Name());
		auto src2dst = registry.RegisterEdge(srcNodeIdx, dstNodeIdx);
		subgraph_move.AddEdge(src2dst);
		registry.RegisterEdgePort(
			src2dst,
			{ .compass = UGraphviz::Registry::Port::Compass::SE },
			{ .compass = UGraphviz::Registry::Port::Compass::NW }
		);
	}

	graph
		.RegisterGraphNodeAttr(UGraphviz::Attrs_style, "filled")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_fontcolor, "white")
		.RegisterGraphNodeAttr(UGraphviz::Attrs_fontname, "consolas");

	return graph;
}
