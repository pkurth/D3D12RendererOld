struct ps_input
{
	float4 color : COLOR;
};

float4 main(ps_input IN) : SV_TARGET
{
	return float4(IN.color);
}
