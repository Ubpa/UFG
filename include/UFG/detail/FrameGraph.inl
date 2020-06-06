#pragma once

namespace Ubpa::FG {
	template<size_t N, size_t M>
	size_t FrameGraph::AddPassNode(
		std::string name,
		const std::array<std::string, N>& inputs_str,
		const std::array<std::string, M>& outputs_str)
	{
		std::vector<size_t> inputs(N);
		std::vector<size_t> outputs(M);

		for (size_t i = 0; i < N; i++)
			inputs[i] = GetPassNodeIndex(inputs_str[i]);
		for (size_t i = 0; i < M; i++)
			outputs[i] = GetPassNodeIndex(outputs_str[i]);

		return AddPassNode(std::move(name), std::move(inputs), std::move(outputs));
	}
}
