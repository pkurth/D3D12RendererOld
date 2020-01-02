

RWStructuredBuffer<uint> counter	: register(u0);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	counter[0] = 0;
}

