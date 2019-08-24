
typedef float4 quat;

float3 quatRotate(in float3 v, in quat q)
{
	float3 t = 2 * cross(q.xyz, v);
	return v + q.w * t + cross(q.xyz, t);
}

quat quatFrom3x3(in float3x3 m)
{
	float3x3 a = transpose(m);
	quat q;
	float trace = a[0][0] + a[1][1] + a[2][2];
	if (trace > 0)
	{
		float s = 0.5f / sqrt(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (a[2][1] - a[1][2]) * s;
		q.y = (a[0][2] - a[2][0]) * s;
		q.z = (a[1][0] - a[0][1]) * s;
	}
	else
	{
		if (a[0][0] > a[1][1] && a[0][0] > a[2][2])
		{
			float s = 2.0f * sqrt(1.0f + a[0][0] - a[1][1] - a[2][2]);
			q.w = (a[2][1] - a[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (a[0][1] + a[1][0]) / s;
			q.z = (a[0][2] + a[2][0]) / s;
		}
		else if (a[1][1] > a[2][2])
		{
			float s = 2.0f * sqrt(1.0f + a[1][1] - a[0][0] - a[2][2]);
			q.w = (a[0][2] - a[2][0]) / s;
			q.x = (a[0][1] + a[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (a[1][2] + a[2][1]) / s;
		}
		else
		{
			float s = 2.0f * sqrt(1.0f + a[2][2] - a[0][0] - a[1][1]);
			q.w = (a[1][0] - a[0][1]) / s;
			q.x = (a[0][2] + a[2][0]) / s;
			q.y = (a[1][2] + a[2][1]) / s;
			q.z = 0.25f * s;
		}
	}
	return q;
}

float3x3 quatTo3x3(in quat q)
{
	float3x3 m = float3x3(1.0f - 2.0f * q.y * q.y - 2.0f * q.z * q.z, 2.0f * q.x * q.y - 2.0f * q.z * q.w, 2.0f * q.x * q.z + 2.0f * q.y * q.w,
		2.0f * q.x * q.y + 2.0f * q.z * q.w, 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z, 2.0f * q.y * q.z - 2.0f * q.x * q.w,
		2.0f * q.x * q.z - 2.0f * q.y * q.w, 2.0f * q.y * q.z + 2.0f * q.x * q.w, 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y);
	return transpose(m);
}

float4x4 quatTo4x4(in quat q)
{
	float3x3 m3x3 = quatTo3x3(q);
	return float4x4(m3x3._m00, m3x3._m01, m3x3._m02, 0.0f,
		m3x3._m10, m3x3._m11, m3x3._m12, 0.0f,
		m3x3._m20, m3x3._m21, m3x3._m22, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}
