#include "raw/detail/assert.h"

int main()
{
	int a = 11;
	RAW_ASSERT(a == 10, "a is not 10");
	return 0;
}
