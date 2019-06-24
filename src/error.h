#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <exception>


static void checkResult(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::exception();
	}
}
