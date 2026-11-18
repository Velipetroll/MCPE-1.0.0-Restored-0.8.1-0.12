//tac todo

#pragma once
#include "RenderContext.h"
namespace mce
{
	class RenderContextImmediate
	{
	public:
		static RenderContext& get()
		{
			static RenderContext l;
			return l;
		}
	};
}