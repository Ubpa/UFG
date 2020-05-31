#pragma once

#include <string>

namespace Ubpa::FG {
	struct Resource {
		void* raw_ptr; // pointer to raw resource
		void* impl_ptr; // pointer to impl resource
	};

	struct ResourceRawDesc {
		const void* raw_type; // e.g. D3D_RESOURCE_DESC
	};

	struct ResourceImplDesc {
		const void* impl_type; // e.g. D3D12_SHADER_RESOURCE_VIEW_DESC / D3D12_RENDER_TARGET_VIEW_DESC
		size_t raw_state; // e.g. D3D12_SHADER_RESOURCE_VIEW_DESC / D3D12_RENDER_TARGET_VIEW_DESC
	};

	struct ResourceImportView {
		void* raw_ptr; // pointer to raw resource
		size_t raw_init_state; // e.g. D3D12_RESOURCE_STATES
	};

	template<typename T>
	struct Named : T {
		std::string name;
		T& DeNamed() noexcept { return *this; }
		const T& DeNamed() const noexcept { return *this; }
	};
}
