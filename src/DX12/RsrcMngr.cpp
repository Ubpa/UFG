#include <UFG/DX12/RsrcMngr.h>

using namespace Ubpa::DX12;
using namespace std;

#include <DirectXColors.h>

FG::RsrcMngr::~RsrcMngr() {
	if (!csuDH.IsNull())
		DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(move(csuDH));
	if (!rtvDH.IsNull())
		DescriptorHeapMngr::Instance().GetRTVCpuDH()->Free(move(rtvDH));
	if (!dsvDH.IsNull())
		DescriptorHeapMngr::Instance().GetDSVCpuDH()->Free(move(dsvDH));
	csuDynamicDH->ReleaseAllocations();
	delete csuDynamicDH;
}

void FG::RsrcMngr::NewFrame() {
	importeds.clear();
	temporals.clear();
	passNodeIdx2rsrcs.clear();
	actives.clear();

	for (const auto& idx : csuDHused)
		csuDHfree.push_back(idx);
	for (const auto& idx : rtvDHused)
		rtvDHfree.push_back(idx);
	for (const auto& idx : dsvDHused)
		dsvDHfree.push_back(idx);

	csuDHused.clear();
	rtvDHused.clear();
	dsvDHused.clear();

	if(csuDynamicDH)
		csuDynamicDH->ReleaseAllocations();
	handleMap.clear();
}

void FG::RsrcMngr::Clear() {
	NewFrame();
	rsrcKeeper.clear();
	pool.clear();
}

void FG::RsrcMngr::CSUDHReserve(UINT num) {
	assert(csuDHused.empty());

	UINT origSize = csuDH.GetNumHandles();
	if (origSize >= num)
		return;

	if (!csuDH.IsNull())
		DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(move(csuDH));
	csuDH = DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(num);

	for (UINT i = origSize; i < num; i++)
		csuDHfree.push_back(i);
}

void FG::RsrcMngr::RtvDHReserve(UINT num) {
	assert(rtvDHused.empty());

	UINT origSize = rtvDH.GetNumHandles();
	if (origSize >= num)
		return;

	if (!rtvDH.IsNull())
		DescriptorHeapMngr::Instance().GetRTVCpuDH()->Free(move(rtvDH));
	rtvDH = DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(num);

	for (UINT i = origSize; i < num; i++)
		rtvDHfree.push_back(i);
}

void FG::RsrcMngr::DsvDHReserve(UINT num) {
	assert(dsvDHused.empty());

	UINT origSize = dsvDH.GetNumHandles();
	if (dsvDH.GetNumHandles() >= num)
		return;

	if (!dsvDH.IsNull())
		DescriptorHeapMngr::Instance().GetDSVCpuDH()->Free(move(dsvDH));
	dsvDH = DescriptorHeapMngr::Instance().GetDSVCpuDH()->Allocate(num);

	for (UINT i = origSize; i < num; i++)
		dsvDHfree.push_back(i);
}

void FG::RsrcMngr::DHReserve() {
	UINT numCSU = 0;
	UINT numRTV = 0;
	UINT numDSV = 0;
	
	struct DHRecord {
		std::unordered_set<D3D12_CONSTANT_BUFFER_VIEW_DESC>  descs_cbv;
		std::unordered_set<D3D12_SHADER_RESOURCE_VIEW_DESC>  descs_srv;
		std::unordered_set<D3D12_UNORDERED_ACCESS_VIEW_DESC> descs_uav;
		std::unordered_set<D3D12_RENDER_TARGET_VIEW_DESC>    descs_rtv;
		std::unordered_set<D3D12_DEPTH_STENCIL_VIEW_DESC>    descs_dsv;

		bool null_cbv{ false };
		bool null_srv{ false };
		bool null_uav{ false };
		bool null_rtv{ false };
		bool null_dsv{ false };
	};
	std::unordered_map<size_t, DHRecord> rsrc2record;
	for (auto [passNodeIdx, rsrcs] : passNodeIdx2rsrcs) {
		for (const auto& [rsrcNodeIdx, state, desc] : rsrcs) {
			auto& record = rsrc2record[rsrcNodeIdx];
			std::visit([&](const auto& desc) {
				using T = std::decay_t<decltype(desc)>;
				// CBV
				if constexpr (std::is_same_v<T, D3D12_CONSTANT_BUFFER_VIEW_DESC>) {
					if (record.descs_cbv.find(desc) != record.descs_cbv.end())
						return;
					record.descs_cbv.insert(desc);
					numCSU++;
				}
				// SRV
				else if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>) {
					if (record.descs_srv.find(desc) != record.descs_srv.end())
						return;
					record.descs_srv.insert(desc);
					numCSU++;
				}
				// UAV
				else if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>) {
					if (record.descs_uav.find(desc) != record.descs_uav.end())
						return;
					record.descs_uav.insert(desc);
					numCSU++;
				}
				// RTV
				else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>) {
					if (record.descs_rtv.find(desc) != record.descs_rtv.end())
						return;
					record.descs_rtv.insert(desc);
					numRTV++;
				}
				// DTV
				else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>) {
					if (record.descs_dsv.find(desc) != record.descs_dsv.end())
						return;
					record.descs_dsv.insert(desc);
					numDSV++;
				}
				// SRV null
				else if constexpr (std::is_same_v<T, RsrcImplDesc_SRV_NULL>) {
					if (record.null_srv)
						return;
					record.null_srv = true;
					numCSU++;
				}
				// UAV null
				else if constexpr (std::is_same_v<T, RsrcImplDesc_UAV_NULL>) {
					if (record.null_uav)
						return;
					record.null_uav = true;
					numCSU++;
				}
				// RTV null
				else if constexpr (std::is_same_v<T, RsrcImplDesc_RTV_Null>) {
					if (record.null_rtv)
						return;
					record.null_rtv = true;
					numRTV++;
				}
				// DSV null
				else if constexpr (std::is_same_v<T, RsrcImplDesc_DSV_Null>) {
					if (record.null_dsv)
						return;
					record.null_dsv = true;
					numDSV++;
				}
				else
					static_assert(always_false_v<T>, "non-exhaustive visitor!");
				}, desc);
		}
	}

	CSUDHReserve(numCSU);
	RtvDHReserve(numRTV);
	DsvDHReserve(numDSV);
}

void FG::RsrcMngr::Construct(size_t rsrcNodeIdx) {
	SRsrcView view;

	if (IsImported(rsrcNodeIdx)) {
		view = importeds[rsrcNodeIdx];
		//cout << "[Construct] Import | " << name << " @" << rsrc.buffer << endl;
	}
	else {
		const auto& type = temporals[rsrcNodeIdx];
		auto& typefrees = pool[type];
		if (typefrees.empty()) {
			view.state = D3D12_RESOURCE_STATE_COMMON;
			RsrcPtr ptr;
			ThrowIfFailed(uDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&type.desc,
				view.state,
				&type.clearValue,
				IID_PPV_ARGS(ptr.GetAddressOf())));
			rsrcKeeper.push_back(ptr);
			view.pRsrc = ptr.Get();
			//cout << "[Construct] Create | " << name << " @" << rsrc.buffer << endl;
		}
		else {
			view = typefrees.back();
			typefrees.pop_back();
			//cout << "[Construct] Init | " << name << " @" << rsrc.buffer << endl;
		}
	}
	actives[rsrcNodeIdx] = view;
}

void FG::RsrcMngr::Destruct(size_t rsrcNodeIdx) {
	auto view = actives[rsrcNodeIdx];
	if (!IsImported(rsrcNodeIdx)) {
		pool[temporals[rsrcNodeIdx]].push_back(view);
		//cout << "[Destruct] Recycle | " << name << " @" << rsrc.buffer << endl;
	}
	else {
		auto orig_state = importeds[rsrcNodeIdx].state;
		if (view.state != orig_state) {
			uGCmdList.ResourceBarrier(
				view.pRsrc,
				view.state,
				orig_state);
		}
		//cout << "[Destruct] Import | " << name << " @" << rsrc.buffer << endl;
	}
	actives.erase(rsrcNodeIdx);
}

FG::RsrcMngr& FG::RsrcMngr::RegisterRsrcHandle(
	size_t rsrcNodeIdx,
	RsrcImplDesc desc,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	auto& handles = handleMap[rsrcNodeIdx];
	std::visit([&](const auto& desc) {
		using T = std::decay_t<decltype(desc)>;
		// CBV
		if constexpr (std::is_same_v<T, D3D12_CONSTANT_BUFFER_VIEW_DESC>) {
			assert(gpuHandle.ptr != 0);
			handles.desc2info_cbv[desc] = { cpuHandle, gpuHandle, true };
		}
		// SRV
		else if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>) {
			assert(gpuHandle.ptr != 0);
			handles.desc2info_srv[desc] = { cpuHandle, gpuHandle, true };
		}
		else if constexpr (std::is_same_v<T, RsrcImplDesc_SRV_NULL>) {
			assert(gpuHandle.ptr != 0);
			handles.null_info_srv = { cpuHandle, gpuHandle, true };
		}
		// UAV
		else if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>) {
			assert(gpuHandle.ptr != 0);
			handles.desc2info_uav[desc] = { cpuHandle, gpuHandle, true };
		}
		else if constexpr (std::is_same_v<T, RsrcImplDesc_UAV_NULL>) {
			assert(gpuHandle.ptr != 0);
			handles.null_info_uav = { cpuHandle, gpuHandle, true };
		}
		// RTV
		else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>) {
			assert(gpuHandle.ptr == 0);
			handles.desc2info_rtv[desc] = { cpuHandle,true };
		}
		else if constexpr (std::is_same_v<T, RsrcImplDesc_RTV_Null>) {
			assert(gpuHandle.ptr == 0);
			handles.null_info_rtv = { cpuHandle,true };
		}
		// DSV
		else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>) {
			assert(gpuHandle.ptr == 0);
			handles.desc2info_dsv[desc] = { cpuHandle,true };
		}
		else if constexpr (std::is_same_v<T, RsrcImplDesc_DSV_Null>) {
			assert(gpuHandle.ptr == 0);
			handles.null_info_dsv = { cpuHandle,true };
		}
		else
			static_assert(always_false_v<T>, "non-exhaustive visitor!");
		}, desc);
	return *this;
}

FG::RsrcMngr& FG::RsrcMngr::RegisterRsrcTable(const std::vector<std::tuple<size_t, RsrcImplDesc>>& rsrcNodeIndices) {
	auto allocation = csuDynamicDH->Allocate(static_cast<uint32_t>(rsrcNodeIndices.size()));
	for (uint32_t i = 0; i < rsrcNodeIndices.size(); i++) {
		const auto& [rsrcNodeIdx, desc] = rsrcNodeIndices[i];
		auto& handles = handleMap[rsrcNodeIdx];
		auto cpuHandle = allocation.GetCpuHandle(i);
		auto gpuHandle = allocation.GetGpuHandle(i);
		std::visit([&](const auto& desc) {
			using T = std::decay_t<decltype(desc)>;
			// CBV
			if constexpr (std::is_same_v<T, D3D12_CONSTANT_BUFFER_VIEW_DESC>) {
				handles.desc2info_cbv[desc].cpuHandle = cpuHandle;
				handles.desc2info_cbv[desc].gpuHandle = gpuHandle;
			}
			// SRV
			else if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>) {
				handles.desc2info_srv[desc].cpuHandle = cpuHandle;
				handles.desc2info_srv[desc].gpuHandle = gpuHandle;
			}
			else if constexpr (std::is_same_v<T, RsrcImplDesc_SRV_NULL>) {
				handles.null_info_srv.cpuHandle = cpuHandle;
				handles.null_info_srv.gpuHandle = gpuHandle;
			}
			// UAV
			else if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>) {
				handles.desc2info_uav[desc].cpuHandle = cpuHandle;
				handles.desc2info_uav[desc].gpuHandle = gpuHandle;
			}
			else if constexpr (std::is_same_v<T, RsrcImplDesc_UAV_NULL>) {
				handles.null_info_uav.cpuHandle = cpuHandle;
				handles.null_info_uav.gpuHandle = gpuHandle;
			}
			else
				assert("CBV, SRV, UAV");
			}, desc);
	}
	return *this;
}

void FG::RsrcMngr::AllocateHandle() {
	for (const auto& [passNodeIdx, rsrcs] : passNodeIdx2rsrcs) {
		for (const auto& [rsrcNodeIdx, state, desc] : rsrcs) {
			auto& handles = handleMap[rsrcNodeIdx];
			std::visit([&](const auto & desc) {
				using T = std::decay_t<decltype(desc)>;
				// CBV
				if constexpr (std::is_same_v<T, D3D12_CONSTANT_BUFFER_VIEW_DESC>) {
					if (handles.desc2info_cbv.find(desc) != handles.desc2info_cbv.end())
						return;
					auto idx = csuDHfree.back();
					csuDHfree.pop_back();
					csuDHused.insert(idx);
					handles.desc2info_cbv[desc] = { csuDH.GetCpuHandle(idx), csuDH.GetGpuHandle(idx), false };
				}
				// SRV
				else if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>
					|| std::is_same_v<T, RsrcImplDesc_SRV_NULL>)
				{
					if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>) {
						if (handles.desc2info_srv.find(desc) != handles.desc2info_srv.end())
							return;
					}
					else { // std::is_same_v<T, RsrcImplDesc_SRV_NULL>
						if (handles.HaveNullSrv())
							return;
					}
					auto idx = csuDHfree.back();
					csuDHfree.pop_back();
					csuDHused.insert(idx);
					if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>)
						handles.desc2info_srv[desc] = { csuDH.GetCpuHandle(idx), csuDH.GetGpuHandle(idx), false };
					else
						handles.null_info_srv = { csuDH.GetCpuHandle(idx), csuDH.GetGpuHandle(idx), false };
				}
				// UAV
				else if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>
					|| std::is_same_v<T, RsrcImplDesc_UAV_NULL>)
				{
					if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>) {
						if (handles.desc2info_uav.find(desc) != handles.desc2info_uav.end())
							return;
					}
					else { // std::is_same_v<T, RsrcImplDesc_UAV_NULL>
						if (handles.HaveNullUav())
							return;
					}
					auto idx = csuDHfree.back();
					csuDHfree.pop_back();
					csuDHused.insert(idx);
					if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>)
						handles.desc2info_uav[desc] = { csuDH.GetCpuHandle(idx), csuDH.GetGpuHandle(idx), false };
					else
						handles.null_info_uav = { csuDH.GetCpuHandle(idx), csuDH.GetGpuHandle(idx), false };
				}
				// RTV
				else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>
					|| std::is_same_v<T, RsrcImplDesc_RTV_Null>)
				{
					if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>) {
						if (handles.desc2info_rtv.find(desc) != handles.desc2info_rtv.end())
							return;
					}
					else { // std::is_same_v<T, RsrcImplDesc_RTV_Null>
						if (handles.HaveNullRtv())
							return;
					}
					auto idx = rtvDHfree.back();
					rtvDHfree.pop_back();
					rtvDHused.insert(idx);
					if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>)
						handles.desc2info_rtv[desc] = { rtvDH.GetCpuHandle(idx), false };
					else
						handles.null_info_rtv = { rtvDH.GetCpuHandle(idx), false };
				}
				// DSV
				else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>
					|| std::is_same_v<T, RsrcImplDesc_DSV_Null>)
				{
					if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>) {
						if (handles.desc2info_dsv.find(desc) != handles.desc2info_dsv.end())
							return;
					}
					else { // std::is_same_v<T, RsrcImplDesc_DSV_Null>
						if (handles.HaveNullDsv())
							return;
					}
					auto idx = dsvDHfree.back();
					dsvDHfree.pop_back();
					dsvDHused.insert(idx);
					if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>)
						handles.desc2info_dsv[desc] = { dsvDH.GetCpuHandle(idx), false };
					else
						handles.null_info_dsv = { dsvDH.GetCpuHandle(idx), false };
				}
				else
					static_assert(always_false_v<T>, "non-exhaustive visitor!");
			}, desc);
		}
	}
}

FG::PassRsrcs FG::RsrcMngr::RequestPassRsrcs(size_t passNodeIdx) {
	PassRsrcs passRsrc;
	const auto& rsrcStates = passNodeIdx2rsrcs[passNodeIdx];
	for (const auto& [rsrcNodeIdx, state, desc] : rsrcStates) {
		auto& view = actives[rsrcNodeIdx];
		auto& handles = handleMap[rsrcNodeIdx];

		if (view.state != state) {
			uGCmdList.ResourceBarrier(
				view.pRsrc,
				view.state,
				state);
			view.state = state;
		}

		auto pRsrc = actives[rsrcNodeIdx].pRsrc;
		passRsrc[rsrcNodeIdx] = std::visit([&, rsrcNodeIdx = rsrcNodeIdx](const auto& desc) -> RsrcImpl {
			using T = std::decay_t<decltype(desc)>;

			if constexpr (std::is_same_v<T, D3D12_CONSTANT_BUFFER_VIEW_DESC>)
			{
				auto& info = handles.desc2info_cbv[desc];

				if (!info.init) {
					assert(desc.BufferLocation == static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0)
						|| IsImported(rsrcNodeIdx) && desc.BufferLocation == view.pRsrc->GetGPUVirtualAddress());

					D3D12_CONSTANT_BUFFER_VIEW_DESC bindDesc = desc;
					bindDesc.BufferLocation = view.pRsrc->GetGPUVirtualAddress();

					uDevice->CreateConstantBufferView(&bindDesc, info.cpuHandle);

					info.init = true;
				}

				return { pRsrc, info.cpuHandle, info.gpuHandle };
			}
			else if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_SRV_NULL>)
			{
				DHHandles::CpuGpuInfo* info;
				if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>)
					info = &handles.desc2info_srv[desc];
				else // std::is_same_v<T, RsrcImplDesc_SRV_NULL>
					info = &handles.null_info_srv;

				if (!info->init) {
					const D3D12_SHADER_RESOURCE_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateShaderResourceView(view.pRsrc, pdesc, info->cpuHandle);

					info->init = true;
				}

				return { pRsrc, info->cpuHandle, info->gpuHandle };
			}
			else if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_UAV_NULL>)
			{
				DHHandles::CpuGpuInfo* info;
				if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>)
					info = &handles.desc2info_uav[desc];
				else // std::is_same_v<T, RsrcImplDesc_UAV_NULL>
					info = &handles.null_info_uav;

				if (!info->init) {
					const D3D12_UNORDERED_ACCESS_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_UNORDERED_ACCESS_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateUnorderedAccessView(view.pRsrc, nullptr, pdesc, info->cpuHandle);

					info->init = true;
				}

				return { pRsrc, info->cpuHandle, info->gpuHandle };
			}
			else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_RTV_Null>)
			{
				DHHandles::CpuInfo* info;
				if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>)
					info = &handles.desc2info_rtv[desc];
				else // std::is_same_v<T, RsrcImplDesc_RTV_NULL>
					info = &handles.null_info_rtv;

				if (!info->init) {
					const D3D12_RENDER_TARGET_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateRenderTargetView(view.pRsrc, pdesc, info->cpuHandle);

					info->init = true;
				}

				return { pRsrc, info->cpuHandle, {0} };
			}
			else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_DSV_Null>)
			{
				DHHandles::CpuInfo* info;
				if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>)
					info = &handles.desc2info_dsv[desc];
				else // std::is_same_v<T, RsrcImplDesc_RTV_NULL>
					info = &handles.null_info_dsv;

				if (!info->init) {
					const D3D12_DEPTH_STENCIL_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateDepthStencilView(view.pRsrc, pdesc, info->cpuHandle);

					info->init = true;
				}

				return { pRsrc, info->cpuHandle, {0} };
			}
			else
				static_assert(always_false_v<T>, "non-exhaustive visitor!");
			}, desc);
	}
	return passRsrc;
}
