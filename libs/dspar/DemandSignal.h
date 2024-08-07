#pragma once
namespace dspar
{
	struct DemandSignal
	{
		int sender;
		int target;
		int amount;
	};
} // namespace dspar