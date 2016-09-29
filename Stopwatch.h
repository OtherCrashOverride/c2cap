/*
*
* Copyright (C) 2016 OtherCrashOverride@users.noreply.github.com.
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2, as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
*/

#pragma once

#include "Exception.h"


class Stopwatch
{
	timeval startTime;
	timeval endTime;
	double elapsed = 0;
	bool isRunning = false;


public:

	Stopwatch()
	{
		Reset();
	}



	void Start()
	{
		if (isRunning)
			throw InvalidOperationException();

		gettimeofday(&startTime, NULL);

		isRunning = true;
	}

	void Stop()
	{
		if (!isRunning)
			throw InvalidOperationException();

		gettimeofday(&endTime, NULL);
		
		isRunning = false;
		elapsed = Elapsed();
	}

	void Reset()
	{
		elapsed = 0;

		gettimeofday(&startTime, NULL);
		endTime = startTime;
	}
	
	double Elapsed()
	{
		if (isRunning)
		{
			gettimeofday(&endTime, NULL);
		}

		double seconds = (endTime.tv_sec - startTime.tv_sec);
		double milliseconds = ((double)(endTime.tv_usec - startTime.tv_usec)) / 1000000.0;

		return elapsed + seconds + milliseconds;
	}
};
