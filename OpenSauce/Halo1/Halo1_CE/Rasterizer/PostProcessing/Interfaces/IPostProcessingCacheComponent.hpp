/*
	Yelo: Open Sauce SDK
		Halo 1 (CE) Edition

	See license\OpenSauce\Halo1_CE for specific license information
*/
#pragma once
#include "Rasterizer/PostProcessing/Interfaces/IPostProcessingComponent.hpp"

namespace Yelo
{
	namespace Rasterizer { namespace PostProcessing {
		interface IPostProcessingCacheComponent : public IPostProcessingComponent
		{
			virtual void Initialize_Cache() {}
			virtual void Dispose_Cache() {}

			virtual void InitializeResources_Cache() {}
			virtual void ReleaseResources_Cache() {}
		};
	};};
};