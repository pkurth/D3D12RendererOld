#pragma once

#include <exception>


static void checkResult(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::exception();
	}
}
