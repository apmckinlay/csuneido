#include "std.h"
#include <iostream>

int main()
	{
	int64 one = static_cast<int64>(1) << 32;
	std::cout << "one " << std::hex << one << std::endl;
	int i = static_cast<int>(one >> 1);
	std::cout << "i " << i << std::endl;
	int64 j = static_cast<int64>(static_cast<unsigned int>(i)) << 1;
	std::cout << "j " << std::hex << j << std::endl;
	}
