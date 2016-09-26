#pragma once

#include <exception>
#include <cstdio>


class Exception : public std::exception
{
public:
	Exception()
		: std::exception()
	{
	}

	Exception(const char* message)
		: std::exception()
	{
		fprintf(stderr, "%s\n", message);
	}

};

class InvalidOperationException : public Exception
{

};
