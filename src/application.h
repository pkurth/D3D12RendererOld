#pragma once

class dx_application
{
public:
	virtual void flush() = 0;
	virtual void update() = 0;
	virtual void render() = 0;
};
