
export void test() {
}

export uniform bool TestAllBitEqual(
	const uniform int Num, 
	const uniform uint32 In0[], 
	const uniform uint32 In1[]) 
{
	for(uniform int i = 0; i<Num; i+= programCount)
	{

		if(In0[i + programIndex] != In1[ i+programIndex ])
			return false;

	}

	return true;
}