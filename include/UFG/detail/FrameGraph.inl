#pragma once

namespace Ubpa::UFG {
	template<size_t N, size_t M>
	size_t FrameGraph::RegisterPassNode(
		PassNode::Type type,
		std::string name,
		const std::array<std::string_view, N>& inputs_str,
		const std::array<std::string_view, M>& outputs_str)
	{
		std::vector<size_t> inputs(N);
		std::vector<size_t> outputs(M);

		for (size_t i = 0; i < N; i++)
			inputs[i] = GetResourceNodeIndex(inputs_str[i]);
		for (size_t i = 0; i < M; i++)
			outputs[i] = GetResourceNodeIndex(outputs_str[i]);

		return RegisterPassNode(type, std::move(name), std::move(inputs), std::move(outputs));
	}

	template<size_t N, size_t M>
	size_t FrameGraph::RegisterGeneralPassNode(
		std::string name,
		const std::array<std::string_view, N>& inputs_str,
		const std::array<std::string_view, M>& outputs_str)
	{
		RegisterCopyNode(
			PassNode::Type::General,
			std::move(name),
			inputs_str,
			outputs_str);
	}

	template<size_t N>
	size_t FrameGraph::RegisterCopyPassNode(
		std::string name,
		const std::array<std::string_view, N>& inputs_str,
		const std::array<std::string_view, N>& outputs_str)
	{
		RegisterCopyNode(
			PassNode::Type::Copy,
			std::move(name),
			inputs_str,
			outputs_str);
	}

	template<size_t N>
	size_t FrameGraph::RegisterCopyPassNode(
		const std::array<std::string_view, N>& inputs_str,
		const std::array<std::string_view, N>& outputs_str)
	{
		RegisterCopyNode(
			GenerateCopyPassNodeName(),
			inputs_str,
			outputs_str);
	}
}
