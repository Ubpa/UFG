#pragma once

#include "Rsrc.h"

#include "../core/core.h"

#include <unordered_set>

namespace Ubpa::UFG::DX12 {
	// manage per pass resources
	// CBV, SRV, UAV, RTV, DSV
	class RsrcMngr {
	public:
		~RsrcMngr();

		void Init(UDX12::GCmdList uGCmdList, UDX12::Device uDevice);

		// clear resources
		// you can call it when resize window
		void Clear();

		// clear per frame datas
		// call it per frame before registerations
		void NewFrame();

		// descriptor heap reserve
		// call by Ubpa::UFG::DX12::Executor
		void DHReserve();

		// allocate handles in descriptor heap for each resource nodes
		// call by Ubpa::UFG::DX12::Executor
		void AllocateHandle();

		// import, create or reuse buffer for the resource node
		// call by Ubpa::UFG::DX12::Executor
		void Construct(size_t rsrcNodeIdx);

		// recycle the buffer of the resource node
		// call by Ubpa::UFG::DX12::Executor
		void Destruct(size_t rsrcNodeIdx);

		// move the resource view of the source resource node to the destination resource node
		// call by Ubpa::UFG::DX12::Executor
		void Move(size_t dstRsrcNodeIdx, size_t srcRsrcNodeIdx);

		// - get the resource map (resource node index -> impl resource) of the pass node
		// - we will
		//   1. change buffer state
		//   2. init handle
		// - call by Ubpa::UFG::DX12::Executor
		PassRsrcs RequestPassRsrcs(size_t passNodeIdx);

		// mark the resource node as imported
		RsrcMngr& RegisterImportedRsrc(size_t rsrcNodeIdx, SRsrcView view) {
			importeds[rsrcNodeIdx] = view;
			return *this;
		}

		// mark the resource node as temporal
		RsrcMngr& RegisterTemporalRsrc(size_t rsrcNodeIdx, RsrcType type) {
			temporals[rsrcNodeIdx] = type;
			return *this;
		}

		// provide details for the resource node of the pass node
		RsrcMngr& RegisterPassRsrcs(size_t passNodeIdx, size_t rsrcNodeIdx, RsrcState state, RsrcImplDesc desc) {
			passNodeIdx2rsrcMap[passNodeIdx].emplace(rsrcNodeIdx, std::tuple(state, desc));
			return *this;
		}

		// provide external handles for the resource node
		RsrcMngr& RegisterRsrcHandle(
			size_t rsrcNodeIdx,
			RsrcImplDesc desc,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = { 0 });

		// only support CBV, SRV, UAV
		RsrcMngr& RegisterRsrcTable(const std::vector<std::tuple<size_t, RsrcImplDesc>>& rsrcNodeIndices);

		// you should
		// 1. use RegisterImportedRsrc or RegisterTemporalRsrc to mark each resource nodes
		// 2. use RegisterPassRsrcs to mark each resource nodes for every passes
		// if you do it correct, then return true, else return false
		bool CheckComplete(const FrameGraph& fg);

	private:
		bool IsImported(size_t rsrcNodeIdx) const noexcept {
			return importeds.find(rsrcNodeIdx) != importeds.end();
		}

		// CSU : CBV, CSU, UAV

		void CSUDHReserve(UINT num);
		void DsvDHReserve(UINT num);
		void RtvDHReserve(UINT num);

		UDX12::GCmdList uGCmdList;
		UDX12::Device uDevice;

		// type -> vector<view>
		std::vector<RsrcPtr> rsrcKeeper;
		std::unordered_map<RsrcType, std::vector<SRsrcView>> pool;

		// rsrcNodeIdx -> view
		std::unordered_map<size_t, SRsrcView> importeds;

		// rsrcNodeIdx -> type
		std::unordered_map<size_t, RsrcType> temporals;

		// passNodeIdx -> resource map
		std::unordered_map<size_t, std::unordered_map<size_t, std::tuple<RsrcState, RsrcImplDesc>>> passNodeIdx2rsrcMap;

		// rsrcNodeIdx -> view
		std::unordered_map<size_t, SRsrcView> actives;

		struct DHHandles {
			struct CpuGpuInfo {
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ 0 };
				D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{ 0 };
				bool init{ false };
			};

			struct CpuInfo {
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ 0 };
				bool init{ false };
			};

			std::unordered_map<D3D12_CONSTANT_BUFFER_VIEW_DESC, CpuGpuInfo>  desc2info_cbv;
			std::unordered_map<D3D12_SHADER_RESOURCE_VIEW_DESC, CpuGpuInfo>  desc2info_srv;
			std::unordered_map<D3D12_UNORDERED_ACCESS_VIEW_DESC, CpuGpuInfo> desc2info_uav;

			std::unordered_map<D3D12_RENDER_TARGET_VIEW_DESC, CpuInfo>       desc2info_rtv;
			std::unordered_map<D3D12_DEPTH_STENCIL_VIEW_DESC, CpuInfo>       desc2info_dsv;

			CpuGpuInfo null_info_srv;
			CpuGpuInfo null_info_uav;

			CpuInfo    null_info_dsv;
			CpuInfo    null_info_rtv;

			bool HaveNullSrv() const {
				return null_info_srv.cpuHandle.ptr != 0;
			}
			bool HaveNullUav() const {
				return null_info_uav.cpuHandle.ptr != 0;
			}
			bool HaveNullDsv() const {
				return null_info_dsv.cpuHandle.ptr != 0;
			}
			bool HaveNullRtv() const {
				return null_info_rtv.cpuHandle.ptr != 0;
			}
		};

		// rsrcNodeIdx -> handles
		std::unordered_map<size_t, DHHandles> handleMap;
		
		UDX12::DynamicSuballocMngr* csuDynamicDH{ nullptr };

		UDX12::DescriptorHeapAllocation csuDH;
		std::vector<UINT> csuDHfree;
		std::unordered_set<UINT> csuDHused;

		UDX12::DescriptorHeapAllocation rtvDH;
		std::vector<UINT> rtvDHfree;
		std::unordered_set<UINT> rtvDHused;

		UDX12::DescriptorHeapAllocation dsvDH;
		std::vector<UINT> dsvDHfree;
		std::unordered_set<UINT> dsvDHused;
	};
}
