#pragma once

namespace Ubpa::UFG {
	template<size_t N, size_t M>
	size_t FrameGraph::RegisterPassNode(
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

		return RegisterPassNode(std::move(name), std::move(inputs), std::move(outputs));
	}
}
